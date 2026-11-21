#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Build and deploy Cobalt to RDK devices.

This script handles the full lifecycle of building, packaging, deploying,
and launching Cobalt (as a plugin or executable) or Cobalt tests on RDK.

Usage Examples:
  1. Build and deploy as Cobalt plugin (default: config: qa):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py

  2. Build, deploy, and RUN Cobalt plugin or tests on device:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --tests nplb --run

  (For more complex workflows—such as deep-linking, profiling, DevTools, or log streaming—run the script with --help to see all available parameters.)
"""

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from typing import Dict, List, Optional, Union

# Disable AI agent environment variables to avoid siso wrapper flag issues
os.environ.pop("ANTIGRAVITY_AGENT", None)
os.environ.pop("GEMINI_CLI", None)

# Constants
PLATFORM = "evergreen-arm-hardfp-rdk"
DEFAULT_REMOTE_DIR = "/data/out_cobalt"
TEST_REMOTE_DIR = "/data/test"
MIN_SYSTEM_SOFTWARE_VERSION = "20260420"
MIN_SYSTEM_SOFTWARE_VERSIONS = {
    6: "20260420",
    7: "20261110",
}
TOOLCHAIN_INSTALLER_FILENAMES = {
    6: "20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh",
    7: "20260415_rdk-glibc-x86_64-arm-toolchain-2.0.sh",
}
TOOLCHAIN_INSTALLER_BASE_URL = (
    "https://storage.googleapis.com/cobalt-static-storage-public"
)
RDK7_ABI_MARKERS = (b"GLIBC_2.34", b"GLIBCXX_3.4.29")
_RDK_MAJOR_VERSION_RE = re.compile(r"^RDK_V(\d+)_", re.IGNORECASE)


def run_command(
    command: Union[str, List[str]],
    verbose: bool = True,
    check: bool = True,
    sleep_time: int = 0,
    cwd: Optional[str] = None,
    env: Optional[Dict[str, str]] = None,
) -> str:
    """Utility to run shell commands."""
    if verbose:
        cmd_str = command if isinstance(command, str) else " ".join(command)
        print(f">>> Executing: {cmd_str}")

    is_shell = isinstance(command, str)
    process = subprocess.Popen(
        command,
        shell=is_shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        cwd=cwd,
        env=env,
    )
    stdout_lines = []
    for line in process.stdout:
        print(line, end="", flush=True)
        stdout_lines.append(line)
    process.wait()
    stdout = "".join(stdout_lines)

    if check and process.returncode != 0:
        sys.exit(process.returncode)

    if sleep_time > 0:
        if verbose:
            print(f"Waiting {sleep_time} seconds...")
        time.sleep(sleep_time)

    return stdout


def run_remote_command(
    command: Union[str, List[str]],
    device_id: Optional[str] = None,
    device_ip: Optional[str] = None,
    verbose: bool = True,
    check: bool = True,
    sleep_time: int = 0,
) -> str:
    """Runs command on remote device via ADB or SSH."""
    if device_id:
        adb_cmd = ["adb", "-s", device_id, "shell"]
        if isinstance(command, str):
            adb_cmd.append(command)
        else:
            adb_cmd.extend(command)
        return run_command(adb_cmd, verbose, check, sleep_time)
    elif device_ip:
        ssh_cmd = ["ssh", "-q", f"root@{device_ip}"]
        if isinstance(command, str):
            ssh_cmd.append(command)
        else:
            ssh_cmd.append(" ".join(command))
        return run_command(ssh_cmd, verbose, check, sleep_time)
    else:
        raise ValueError("Either device_id or device_ip must be provided for remote command.")


def push_to_device(
    local_path: Union[str, Path],
    remote_path: str,
    device_id: Optional[str] = None,
    device_ip: Optional[str] = None,
    verbose: bool = True,
) -> None:
    """Pushes local file/directory to remote device via ADB or SCP."""
    if device_id:
        adb_cmd = ["adb", "-s", device_id, "push", str(local_path), remote_path]
        run_command(adb_cmd, verbose=verbose)
    elif device_ip:
        scp_cmd = [
            "scp",
            "-q",
            "-r",
            str(local_path),
            f"root@{device_ip}:{remote_path}",
        ]
        run_command(scp_cmd, verbose=verbose)
    else:
        raise ValueError("Either device_id or device_ip must be provided to push file.")


def configure_build(
    platform: str,
    config: str,
    out_dir: Path,
    no_rbe: bool = False,
    rdk_version: Optional[int] = None,
    rdk_home: Optional[str] = None,
) -> None:
    """Runs GN configuration."""
    print(f"=== Configuring {platform} ({config}) ===")
    cmd = [
        "python3", "cobalt/build/gn.py", "-p", platform, "-C", config,
        str(out_dir)
    ]
    if no_rbe:
        cmd.append("--no-rbe")
    if rdk_version is not None:
        cmd.extend(["--rdk-version", str(rdk_version)])
    if rdk_home:
        cmd.extend(["--rdk-home", rdk_home])
    run_command(cmd)



def build_targets(out_dir: Path, targets: List[str]) -> str:
    """Builds the specified targets using autoninja."""
    print(f"=== Building {' '.join(targets)} ===")
    return run_command(["autoninja", "-C", str(out_dir)] + targets)


def deploy_only_lib(device_id: Optional[str], device_ip: Optional[str], out_dir: Path, remote_dir: str) -> None:
    """Pushes libcobalt.lz4 directly to the device."""
    local_lz4 = out_dir / "app/cobalt/lib/libcobalt.lz4"
    if not local_lz4.exists():
        print(f"Error: {local_lz4} not found.")
        sys.exit(1)
        return

    remote_lib_dir = f"{remote_dir}/app/cobalt/lib"
    run_remote_command(f"mkdir -p {remote_lib_dir}", device_id, device_ip)
    push_to_device(local_lz4, f"{remote_lib_dir}/libcobalt.lz4", device_id, device_ip)


def package_and_deploy(
    device_id: Optional[str],
    device_ip: Optional[str],
    out_dir: Path,
    remote_dir: str,
    deps_file: Optional[Path], is_test: bool) -> None:
    """Packages artifacts using runtime_deps and pushes to device."""
    print("=== Packaging & Deploying artifacts ===")
    archive_name = "archive.tar.gz"

    if deps_file and deps_file.exists():
        tar_cmd = ["tar", "-czvf", archive_name, "-C", str(out_dir), "-T", str(deps_file)]
        try:
            deps_entries = {
                os.path.normpath(line.strip())
                for line in deps_file.read_text(encoding="utf-8").splitlines()
                if line.strip()
            }
        except OSError:
            deps_entries = set()
        if not is_test and "libloader_app.so" not in deps_entries:
            tar_cmd.append("libloader_app.so")
        build_info = out_dir / "gen/build_info.json"
        if build_info.exists() and os.path.normpath("gen/build_info.json") not in deps_entries:
            tar_cmd.append("gen/build_info.json")
    else:
        if deps_file:
            print(f"Warning: deps_file {deps_file} not found.")
        print(f"Packaging everything in {out_dir}")
        tar_cmd = ["tar", "-czvf", archive_name, "-C", str(out_dir), "."]

    print(f"Packaging with: {' '.join(tar_cmd)}")
    run_command(tar_cmd)
    if not remote_dir or remote_dir == "/":
        print(f"Error: remote_dir '{remote_dir}' is invalid or dangerous for cleanup.")
        sys.exit(1)
        return
    print("=== Cleaning previous remote deployment ===")
    # Remove old app directories and binaries, ignoring errors if they don't exist
    run_remote_command(
        f"rm -rf {remote_dir}/app {remote_dir}/native_target {remote_dir}/*.so {remote_dir}/loader_app {remote_dir}/*.lz4 {remote_dir}/archive.tar.gz || true",
        device_id, device_ip, check=False)

    run_remote_command(f"mkdir -p {remote_dir}", device_id, device_ip)
    push_to_device(archive_name, f"{remote_dir}/", device_id, device_ip)
    Path(archive_name).unlink(missing_ok=True)

    print("=== Extracting archive on device ===")
    extract_cmds = [
        f"cd {remote_dir}",
        "tar -xzf archive.tar.gz",
        "rm archive.tar.gz",
        f"chmod -R 777 {remote_dir}",
    ]
    run_remote_command(" && ".join(extract_cmds), device_id, device_ip)


def ensure_dolby_vision_policy(device_id: Optional[str], device_ip: Optional[str]) -> None:
    """Ensures /sys/module/aml_media/parameters/dolby_vision_policy is set to 1."""
    # TODO(b/532753892): Remove this workaround once the device driver is updated.
    policy_file = "/sys/module/aml_media/parameters/dolby_vision_policy"
    run_remote_command(f"echo 1 > {policy_file}", device_id, device_ip)


def _extract_flag_key(arg: str) -> str:
    """Extracts option key from flag string (e.g. '--foo' from '--foo=val' or '--foo')."""
    return arg.split("=", 1)[0]


def _filter_args_by_keys(base_args: List[str], override_args: List[str]) -> List[str]:
    """Filters flags from base_args whose option keys match any flag in override_args."""
    override_keys = {
        _extract_flag_key(arg)
        for arg in override_args
        if arg.startswith("--")
    }
    return [
        arg
        for arg in base_args
        if not (arg.startswith("--") and _extract_flag_key(arg) in override_keys)
    ]


def _merge_args(base_args: List[str], override_args: List[str]) -> List[str]:
    """Replaces flags in base_args with matching option keys from override_args, and appends override_args."""
    return _filter_args_by_keys(base_args, override_args) + override_args


def remove_duplicate_sb_args(
    cobalt_json_args: List[str],
    script_args: Optional[List[str]] = None,
    user_override_args: Optional[List[str]] = None,
) -> List[str]:
    """Combines cobalt_json_args, script_args, and user_override_args with key deduplication.

    Precedence order (highest to lowest):
      1. user_override_args (passed via --param)
      2. script_args (script-added flags e.g. --remote-debugging-port=9222)
      3. cobalt_json_args (pre-existing flags from WPEFramework cobalt.json / sbmainargs)
    """
    script_args = script_args or []
    user_override_args = user_override_args or []

    # 1. Merge script_args with user_override_args
    override_args = _merge_args(script_args, user_override_args)

    # 2. Merge cobalt_json_args with override_args
    return _merge_args(cobalt_json_args, override_args)


def launch_on_device(
    device_id: Optional[str],
    device_ip: Optional[str],
    remote_dir: str,
    test_name: Optional[str],
    devtools: bool = False,
    param: Optional[List[str]] = None,
    deeplink: Optional[str] = None,
) -> None:
    """Executes remote commands to launch Cobalt or tests."""
    print("=== Launching on device ===")
    remote_cmds = [f"cd {remote_dir}"]

    if test_name:
        remote_cmds += ["rdkDisplay remove || true", "sleep 2", "mkdir -p results"]
        gtest_filter = ""
        filter_file = (Path("cobalt/testing/filters") / PLATFORM /
                       f"{test_name}_loader_filter.json")
        if filter_file.exists():
            with open(filter_file) as f:
                fails = json.load(f).get("failing_tests", [])
                if fails:
                    gtest_filter = f" --gtest_filter=-{':'.join(fails)}"

        xml_out = f"--gtest_output=xml:{remote_dir}/results/{test_name}_loader.xml"
        remote_cmds += [
            "rdkDisplay create",
            "sleep 2",
            "if [ -S /tmp/test-0 ]; then xdg_runtime_dir=/tmp; else xdg_runtime_dir=/run; fi",
            r"if [ ! -e /etc/localtime ]; then export TZ=\${TZ:-UTC}; fi",
            f"XDG_RUNTIME_DIR=\\$xdg_runtime_dir WAYLAND_DISPLAY=test-0 ./{test_name}_loader.py {xml_out}{gtest_filter}",
            "rdkDisplay remove",
            "sleep 2",
        ]
    else:
        if devtools:
            print("[INFO] Enabling DevTools support...")

        # Deactivate first to change config
        deactivate_json = '{"jsonrpc":"2.0","id":1,"method":"Controller.1.deactivate","params":{"callsign":"YouTube"}}'
        run_remote_command(f"curl -s http://127.0.0.1:9998/jsonrpc -d '{deactivate_json}'", device_id, device_ip)

        # Get configuration
        get_config_json = '{"jsonrpc":"2.0","id":1,"method":"Controller.1.configuration@YouTube"}'
        res_str = run_remote_command(f"curl -s http://127.0.0.1:9998/jsonrpc -d '{get_config_json}'", device_id, device_ip)

        rpc_deactivate = (
            r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.deactivate\",'
            r'\"params\":{\"callsign\":\"YouTube\"}}')
        try:
            res = json.loads(res_str.strip())
            if "result" in res:
                config = res["result"]
                cobalt_json_args = config.get("sbmainargs", [])
                
                script_args = []
                if devtools:
                    script_args.append("--remote-debugging-port=9222")

                user_override_args = param if param else []

                config["sbmainargs"] = remove_duplicate_sb_args(
                    cobalt_json_args, script_args, user_override_args
                )

                # Set configuration
                rpc_set_config = json.dumps({
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "Controller.1.configuration@YouTube",
                    "params": config
                })
                run_remote_command(f"curl -s http://127.0.0.1:9998/jsonrpc -d '{rpc_set_config}'", device_id, device_ip)
                print("[INFO] DevTools configuration updated successfully.")
        except Exception as e:
            print(f"[WARNING] Failed to update DevTools configuration: {e}")

        rpc_activate = (
            r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.activate\",'
            r'\"params\":{\"callsign\":\"YouTube\"}}')
        remote_cmds += [
            "rdkDisplay remove || true",
            "sleep 2",
            f"curl http://127.0.0.1:9998/jsonrpc -d '{rpc_deactivate}'",
            "sleep 2",
            f"curl -s http://127.0.0.1:9998/jsonrpc -d '{rpc_activate}'",
        ]

        if deeplink:
            rpc_deeplink_json = json.dumps({
                "jsonrpc": "2.0",
                "id": 3,
                "method": "YouTube.deeplink",
                "params": deeplink
            }).replace('"', r'\"')
            remote_cmds.append(f"curl -X POST http://127.0.0.1:9998/jsonrpc -d '{rpc_deeplink_json}'")

        if devtools:
            print("[INFO] Setting up DevTools port forwarding...")
            if device_id:
                run_command(["adb", "-s", device_id, "forward", "tcp:9222", "tcp:9222"])
            elif device_ip:
                ssh_tunnel_cmd = [
                    "ssh",
                    "-q",
                    "-fN",
                    "-L",
                    "9222:localhost:9222",
                    f"root@{device_ip}",
                ]
                try:
                    subprocess.Popen(ssh_tunnel_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                except Exception as e:
                    print(f"[WARNING] Failed to start SSH tunnel: {e}")
            print("[INFO] DevTools is enabled. Please open Chrome and navigate to 'chrome://inspect' (add 'localhost:9222' or the device IP to discover targets).")


    full_cmd = " && ".join(remote_cmds)
    output = run_remote_command(f"bash -l -c \"{full_cmd}\"", device_id, device_ip)
    print(output)
    if "ERROR_OPENING_FAILED" in output or "error" in output.lower():
        print("\n[WARNING] Activation failed with error (e.g., ERROR_OPENING_FAILED).")
        print("[WARNING] Please check the physical device state. It might be in setup/Out-of-Box Experience (OOBE) mode or not connected to a network.")


def parse_args() -> argparse.Namespace:
    """Parses command line arguments."""
    parser = argparse.ArgumentParser(
        description="Build and deploy Cobalt to RDK.")
    parser.add_argument(
        "--only-lib", action="store_true", help="Deploy only libcobalt.lz4.")
    parser.add_argument(
        "--tests",
        type=str,
        metavar="TEST_NAME",
        help="Build and run a test (e.g., nplb).",
    )
    parser.add_argument(
        "--device-ip",
        type=str,
        help="Target RDK device IP address (uses SSH/SCP instead of ADB).",
    )
    parser.add_argument(
        "--device-id",
        type=str,
        help=(
            "Target RDK device ADB serial (e.g. 'localhost:44133'). Use when more "
            "than one RDK device is attached, to avoid auto-detection picking the "
            "wrong one. Mutually exclusive with --device-ip."
        ),
    )
    parser.add_argument(
        "--config", type=str, help="Override default build configuration.")
    parser.add_argument(
        "--out-dir", type=str, help="Custom build output directory.")
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Skip configure and build steps.",
    )
    parser.add_argument(
        "--skip-deploy",
        action="store_true",
        help="Skip deployment step (assumes files are already on the device).",
    )
    parser.add_argument(
        "--run", action="store_true", help="Run on device after build/deploy.")
    parser.add_argument(
        "--force-deploy",
        action="store_true",
        help="Force deployment even if up-to-date.",
    )

    parser.add_argument(
        "--deeplink",
        type=str,
        dest="deeplink",
        help="Deeplink parameter (e.g. v=dQw4w9WgXcQ) to pass to Cobalt.",
    )
    parser.add_argument(
        "--reset",
        action="store_true",
        help=(
            "Run rdkDisplay remove and restart WPEFramework on the device. "
            "Useful if the display is inactive or WPEFramework is stuck."),
    )
    parser.add_argument(
        "--setup-toolchain",
        action="store_true",
        help="Download and install the RDK toolchain (to RDK_HOME or ~/rdk/toolchain_ah212_{6,7}).",
    )
    parser.add_argument(
        "--rdk-version",
        type=int,
        choices=[6, 7],
        default=None,
        help="Target RDK OS major version (6 or 7; defaults to 6).",
    )
    parser.add_argument(
        "--rdk-home",
        type=str,
        default=None,
        help="Path to the RDK SDK toolchain root (forwarded to gn.py and --setup-toolchain).",
    )
    parser.add_argument(
        "--toolchain-installer",
        type=str,
        default=None,
        help="Path to a local Yocto SDK .sh installer for --setup-toolchain.",
    )
    parser.add_argument(
        "--revert-c25",
        action="store_true",
        help="Revert the active Cobalt configuration on the device back to Cobalt 25.",
    )
    parser.add_argument(
        "--no-rbe",
        action="store_true",
        help="Disable Remote Build Execution (RBE) in GN configuration.",
    )
    parser.add_argument(
        "--logs",
        action="store_true",
        help="View system/Cobalt logs from the device.",
    )
    parser.add_argument(
        "--follow",
        action="store_true",
        help="Follow log output in real-time (runs journalctl -f).",
    )
    parser.add_argument(
        "--system-logs",
        action="store_true",
        help="View global OS/kernel/systemd logs from the device (runs raw journalctl).",
    )
    parser.add_argument(
        "--param",
        nargs=argparse.REMAINDER,
        default=[],
        help=(
            "Additional runtime parameter(s) to pass to StarboardMain (must be specified last). "
            "All arguments must start with '--' (positional arguments are not supported)."
        ),
    )
    args = parser.parse_args()

    if args.param:
        for arg in args.param:
            if not arg.startswith("--"):
                print(
                    f"Error: All arguments passed to --param must start with '--' (positional arguments are not supported). "
                    f"Positional argument '{arg}' is not supported."
                )
                sys.exit(1)

    return args


def get_model_name(device_id: str) -> Optional[str]:
    """Attempts to retrieve the model name from /etc/device.properties."""
    try:
        res = subprocess.run(
            ["adb", "-s", device_id, "shell", "cat /etc/device.properties"],
            capture_output=True, text=True, timeout=5
        )
        if res.returncode == 0:
            for line in res.stdout.splitlines():
                if "MODEL_NAME=" in line:
                    return line.split("=", 1)[1].strip().strip('"')
    except Exception:
        pass
    return None


def is_rdk_device(device_id: str) -> bool:
    """Checks if the device is an RDK device by checking if model is AH212."""
    model = get_model_name(device_id)
    return model is not None and model.lower() == "ah212"


def get_device_id() -> str:
    """Gets the target RDK device ID."""
    try:
        result = subprocess.run(
            ["adb", "devices"],
            capture_output=True,
            text=True,
            check=True
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Error running 'adb devices': {e}")
        sys.exit(1)

    # Parse 'adb devices' output to collect online device serial IDs.
    devices = []
    for line in result.stdout.strip().split("\n")[1:]:
        if not line.strip():
            continue
        parts = line.split()
        if len(parts) >= 2 and parts[1] == "device":
            devices.append(parts[0])

    if not devices:
        print("Error: No ADB devices connected.")
        sys.exit(1)

    # Filter for RDK devices
    rdk_devices = []
    for dev in devices:
        if is_rdk_device(dev):
            rdk_devices.append(dev)

    if not rdk_devices:
        print(f"Error: None of the connected devices {devices} were identified as RDK (AH212).")
        sys.exit(1)

    # Explicit assumption: if multiple devices are connected, the first one is picked.
    if len(rdk_devices) > 1:
        print(f"Note: Multiple RDK devices detected: {rdk_devices}. Picking the first one: {rdk_devices[0]}")
        print("      Pass --device-id <serial> (or --device-ip <ip>) to target a specific device.")

    dev = rdk_devices[0]
    print(f"Using RDK device: {dev} (AH212)")
    return dev


def assert_software_version(
    device_id: Optional[str],
    device_ip: Optional[str],
    min_version_date: Optional[str] = None,
) -> Optional[int]:
    """Verifies device software version floor and returns the RDK major version."""
    try:
        output = run_remote_command("cat /version.txt", device_id, device_ip, check=False)
        if "imagename:" not in output:
            print("Error: Could not read /version.txt from the device to verify system software version.")
            print(f"Details: {output}")
            sys.exit(1)
            return None

        custom_version = None
        for line in output.splitlines():
            if "custom version:" in line.lower():
                custom_version = line.split(":", 1)[1].strip()
                break

        if not custom_version:
            print("Error: 'custom version' field not found in /version.txt.")
            sys.exit(1)
            return None

        major_match = _RDK_MAJOR_VERSION_RE.match(custom_version)
        device_major = int(major_match.group(1)) if major_match else None
        required_date_str = min_version_date or MIN_SYSTEM_SOFTWARE_VERSIONS.get(
            device_major, MIN_SYSTEM_SOFTWARE_VERSION
        )

        # Extract date suffix from custom version (e.g. RDK_V6_AH212_20260330 -> 20260330)
        parts = custom_version.split("_")
        if not parts:
            print(f"Error: Could not parse date from custom version: {custom_version}")
            sys.exit(1)
            return None

        date_str = parts[-1]
        try:
            min_date = datetime.strptime(required_date_str, "%Y%m%d").date()
        except ValueError:
            print(f"Error: Invalid min_version_date format: {required_date_str}. Expected YYYYMMDD.")
            sys.exit(1)
            return None

        try:
            device_date = datetime.strptime(date_str, "%Y%m%d").date()
        except ValueError:
            print(f"Error: Extracted version suffix '{date_str}' is not a valid YYYYMMDD calendar date.")
            sys.exit(1)
            return None

        if device_date < min_date:
            print(
                f"Error: Device system software version ({custom_version}) is older than "
                f"the required minimum version ({required_date_str})."
            )
            sys.exit(1)
            return None

        print(
            f"Device system software version verified: {custom_version} "
            f"(required: >= {required_date_str})"
        )
        return device_major

    except Exception as e:
        print(f"Error checking software version: {e}")
        sys.exit(1)
        return None


def detect_binary_rdk_version(binary_path: Path) -> Optional[int]:
    """Detects whether a native Starboard binary targets RDK7 or RDK6 ABI."""
    if not binary_path.exists():
        return None
    overlap = max(len(marker) for marker in RDK7_ABI_MARKERS) - 1
    try:
        with binary_path.open("rb") as f:
            tail = b""
            while True:
                chunk = f.read(65536)
                if not chunk:
                    break
                search_buf = tail + chunk
                if any(marker in search_buf for marker in RDK7_ABI_MARKERS):
                    return 7
                tail = search_buf[-overlap:] if overlap > 0 else b""
    except OSError:
        return None
    return 6


def verify_abi_compatibility(
    out_dir: Path,
    device_rdk_version: Optional[int],
    *,
    is_test: bool,
    only_lib: bool,
) -> bool:
    """Checks that local native Starboard binaries are ABI-compatible with the target device."""
    if only_lib or device_rdk_version is None:
        return True

    binary_path = out_dir / ("elf_loader_sandbox" if is_test else "libloader_app.so")
    binary_rdk_version = detect_binary_rdk_version(binary_path)
    if binary_rdk_version is None:
        return True

    if binary_rdk_version == 7 and device_rdk_version == 6:
        print(
            f"Error: Local binary {binary_path} was compiled with the RDK7 toolchain "
            "(links GLIBC_2.34 / GLIBCXX_3.4.29) and cannot load on an RDK6 device "
            "(glibc 2.31 / libstdc++.so.6.0.28)."
        )
        print("Build with --rdk-version 6 for this device, or target an RDK7 device.")
        sys.exit(1)
        return False

    if binary_rdk_version == 6 and device_rdk_version == 7:
        print(
            f"[WARNING] Deploying an RDK6-compiled binary ({binary_path}) to an RDK7 device. "
            "Pass --rdk-version 7 to build with the native RDK7 toolchain."
        )

    return True


def check_and_switch_cobalt_version(device_id: Optional[str], device_ip: Optional[str]) -> None:
    """Checks if custom Cobalt is active on the device, otherwise switches and reboots."""
    try:
        current_target = run_remote_command(
            "readlink /usr/lib/libloader_app.so", device_id, device_ip, check=False
        ).strip()
        current_plugin_config = run_remote_command(
            "readlink /etc/WPEFramework/plugins/Cobalt.json",
            device_id,
            device_ip,
            check=False,
        ).strip()

        if (
            current_target == "/data/out_cobalt/libloader_app.so"
            and current_plugin_config == "/etc/WPEFramework/plugins/Cobalt_custom.json"
        ):
            print("Custom Cobalt configuration is already active on the device.")
            return

        boot_id_before = run_remote_command(
            "cat /proc/sys/kernel/random/boot_id",
            device_id,
            device_ip,
            check=False,
            verbose=False,
        ).strip()

        print("Custom Cobalt configuration is not active. Running 'chCobalt custom_cobalt' on the device...")
        run_remote_command("chCobalt custom_cobalt", device_id, device_ip)

        print("Device is being rebooted to apply changes...")
        run_remote_command("bash -l -c 'sync && reboot'", device_id, device_ip, check=False)

        print("Waiting 30 seconds for the device to reboot...")
        time.sleep(30)

        print("Attempting to reconnect to device...")
        reconnected = False
        for i in range(15):  # try up to 15 times with 3 second intervals
            out = run_remote_command("echo ok", device_id, device_ip, check=False, verbose=False).strip()
            if out == "ok":
                if boot_id_before:
                    boot_id_after = run_remote_command(
                        "cat /proc/sys/kernel/random/boot_id",
                        device_id,
                        device_ip,
                        check=False,
                        verbose=False,
                    ).strip()
                    if boot_id_after and boot_id_after == boot_id_before:
                        print(f"Device has not finished rebooting yet. Retrying in 3 seconds... ({i+1}/15)")
                        time.sleep(3)
                        continue
                reconnected = True
                break
            print(f"Device not ready yet. Retrying in 3 seconds... ({i+1}/15)")
            time.sleep(3)

        if not reconnected:
            print("Error: Could not reconnect to the device after reboot.")
            sys.exit(1)
            return

        print("Reconnected to device successfully. Waiting for WPEFramework readiness...")
        status_payload = '{"jsonrpc":"2.0","id":1,"method":"Controller.1.status"}'
        wpe_ready = False
        for i in range(15):
            status_out = run_remote_command(
                f"curl -s http://127.0.0.1:9998/jsonrpc -d '{status_payload}'",
                device_id,
                device_ip,
                check=False,
                verbose=False,
            )
            if '"result"' in status_out:
                wpe_ready = True
                break
            print(f"WPEFramework not ready yet. Retrying in 2 seconds... ({i+1}/15)")
            time.sleep(2)

        if not wpe_ready:
            print("[WARNING] Timed out waiting for WPEFramework JSON-RPC readiness; continuing.")

    except Exception as e:
        print(f"Error during Cobalt version switch: {e}")
        sys.exit(1)
        return


def revert_to_cobalt_25(device_id: Optional[str], device_ip: Optional[str]) -> None:
    """Reverts the active Cobalt configuration on the device back to Cobalt 25."""
    target = run_remote_command(
        "readlink /usr/lib/libloader_app.so", device_id, device_ip, check=False
    ).strip()

    if target != "/data/out_cobalt/libloader_app.so":
        print("Cobalt 25 is already active on the device.")
        return

    print("Running 'chCobalt c25' on the device...")
    run_remote_command("chCobalt c25", device_id, device_ip)
    run_remote_command("bash -l -c 'reboot -f'", device_id, device_ip, check=False)
    print("Revert to Cobalt 25 completed. The device is rebooting.")


def is_toolchain_installed(rdk_home: Optional[str]) -> bool:
    """Checks if the RDK toolchain is installed in the target path."""
    return bool(rdk_home and os.path.exists(os.path.join(rdk_home, "sysroots")))


def _normalize_path(path: str) -> str:
    return os.path.abspath(os.path.expanduser(path))


def _default_toolchain_dir(rdk_version: int) -> str:
    return _normalize_path(f"~/rdk/toolchain_ah212_{rdk_version}")


def resolve_toolchain_dir(
    rdk_version: Optional[int],
    rdk_home_arg: Optional[str],
    *,
    for_setup: bool = False,
) -> Optional[str]:
    """Resolves the RDK toolchain directory using the same precedence as gn.py."""
    if rdk_home_arg:
        return _normalize_path(rdk_home_arg)

    if rdk_version is not None:
        env_version_home = os.environ.get(f"RDK{rdk_version}_HOME")
        if env_version_home:
            return _normalize_path(env_version_home)

        well_known = _default_toolchain_dir(rdk_version)
        if for_setup or is_toolchain_installed(well_known):
            return well_known

        env_rdk_home = os.environ.get("RDK_HOME")
        if env_rdk_home:
            return _normalize_path(env_rdk_home)

        if rdk_version == 6 and is_toolchain_installed("/workspaces/rdk/toolchain"):
            return "/workspaces/rdk/toolchain"
        return well_known

    env_rdk_home = os.environ.get("RDK_HOME")
    if env_rdk_home:
        return _normalize_path(env_rdk_home)

    if for_setup:
        return _default_toolchain_dir(6)

    for candidate in (_default_toolchain_dir(6), "/workspaces/rdk/toolchain"):
        if is_toolchain_installed(candidate):
            return candidate
    return None


def _build_clean_installer_env() -> Dict[str, str]:
    """Returns a sanitized environment for Yocto SDK relocation scripts."""
    clean_env = os.environ.copy()
    for var in ("PYTHONPATH", "PYTHONHOME", "VIRTUAL_ENV", "LD_LIBRARY_PATH"):
        clean_env.pop(var, None)
    clean_env["PATH"] = "/usr/local/bin:/usr/bin:/bin"
    return clean_env


def setup_toolchain(
    rdk_version: Optional[int] = None,
    rdk_home_arg: Optional[str] = None,
    toolchain_installer: Optional[str] = None,
) -> None:
    """Downloads (or uses local) and installs the RDK6 or RDK7 toolchain."""
    target_version = rdk_version or 6
    rdk_home = resolve_toolchain_dir(rdk_version, rdk_home_arg, for_setup=True)
    if not rdk_home:
        print("Error: Could not determine target toolchain installation directory.")
        sys.exit(1)
        return

    if is_toolchain_installed(rdk_home):
        print(f"RDK{target_version} toolchain is already installed in: {rdk_home}. Skipping setup.")
        return

    os.makedirs(rdk_home, exist_ok=True)
    clean_env = _build_clean_installer_env()
    installer_filename = TOOLCHAIN_INSTALLER_FILENAMES[target_version]

    if toolchain_installer:
        local_installer = _normalize_path(toolchain_installer)
        if not os.path.exists(local_installer):
            print(f"Error: Local toolchain installer not found: {local_installer}")
            sys.exit(1)
            return
        print(f"=== Setting up RDK{target_version} toolchain in: {rdk_home} (local installer) ===")
        run_command(
            ["sh", local_installer, "-d", rdk_home, "-y"],
            cwd="/tmp",
            env=clean_env,
        )
        return

    url = f"{TOOLCHAIN_INSTALLER_BASE_URL}/{installer_filename}"
    print(f"=== Setting up RDK{target_version} toolchain in: {rdk_home} ===")
    with tempfile.TemporaryDirectory() as tmp_dir:
        installer_path = os.path.join(tmp_dir, installer_filename)
        try:
            run_command(["wget", url, "-O", installer_path])
        except (SystemExit, OSError):
            downloads_fallback = _normalize_path(f"~/Downloads/{installer_filename}")
            if os.path.isfile(downloads_fallback):
                print(
                    f"Download from {url} failed; using local fallback {downloads_fallback}."
                )
                installer_path = downloads_fallback
            else:
                print(
                    f"Error: Failed to download RDK{target_version} toolchain from {url} "
                    f"and no local installer was found at {downloads_fallback}."
                )
                print(
                    f"Download {installer_filename} from your RDK{target_version} release "
                    f"distribution and re-run with --toolchain-installer /path/to/{installer_filename}."
                )
                sys.exit(1)
                return

        run_command(
            ["sh", installer_path, "-d", rdk_home, "-y"],
            cwd="/tmp",
            env=clean_env,
        )


def main() -> None:
    """Main execution flow."""
    args = parse_args()

    if args.deeplink and args.tests:
        print("Error: --deeplink is only supported when running the Cobalt plugin (not tests).")
        sys.exit(1)
        return

    if args.setup_toolchain:
        setup_toolchain(
            rdk_version=args.rdk_version,
            rdk_home_arg=args.rdk_home,
            toolchain_installer=args.toolchain_installer,
        )
        return

    if args.device_ip and args.device_id:
        print(
            "Error: --device-ip and --device-id are mutually exclusive.",
            file=sys.stderr)
        sys.exit(1)
        return

    device_ip = args.device_ip
    device_id = None
    if not device_ip:
        device_id = args.device_id or get_device_id()

    if args.revert_c25:
        revert_to_cobalt_25(device_id, device_ip)
        return

    if args.logs or args.system_logs:
        if not device_ip:
            cmd = ["adb", "-s", device_id, "shell", "journalctl"]
        else:
            cmd = ["ssh", "-q", f"root@{device_ip}", "journalctl"]
        if args.logs:
            cmd.extend([
                "-t",
                "YouTube",
                "-t",
                "Cobalt",
                "-t",
                "loader_app",
                "-t",
                "WPEFramework",
            ])
        if args.follow:
            cmd.append("-f")
        try:
            run_command(cmd)
        except KeyboardInterrupt:
            print("\nLog streaming stopped.")
        return

    if args.reset:
        print("=== Resetting display ===")
        run_remote_command("bash -l -c 'rdkDisplay remove || true'", device_id, device_ip)
        print("=== Restarting WPEFramework ===")
        run_remote_command("systemctl restart wpeframework", device_id, device_ip, sleep_time=5)
        print("=== Cleaning up DevTools ports on host ===")
        # Always try to kill SSH tunnel on host
        run_command(["pkill", "-f", "9222:localhost:9222"], check=False)
        # Try to remove ADB forward if we have a device_id
        if device_id:
            run_command(["adb", "-s", device_id, "forward", "--remove", "tcp:9222"], check=False)
        if not (args.run or args.tests or args.force_deploy):
            print("=== Reset finished. ===")
            return

    device_rdk_version = assert_software_version(device_id, device_ip)
    if not args.tests:
        check_and_switch_cobalt_version(device_id, device_ip)

    # Setup Build Paths
    config = args.config or ("devel" if args.tests else "qa")
    default_out_prefix = (
        "evergreen-arm-hardfp-rdk7" if args.rdk_version == 7 else PLATFORM
    )
    out_dir = Path(args.out_dir or f"out/{default_out_prefix}_{config}")

    if args.tests:
        targets = [f"{args.tests}_loader"]
        remote_dir = TEST_REMOTE_DIR
        deps_file = out_dir / f"{args.tests}_loader.runtime_deps"
    elif args.only_lib:
        targets = ["cobalt"]
        remote_dir = DEFAULT_REMOTE_DIR
        deps_file = None
    else:
        # Standard deployment uses cobalt_loader to generate the runtime_deps list.
        targets = ["cobalt_loader"]
        deps_file = out_dir / "cobalt_loader.runtime_deps"
        remote_dir = DEFAULT_REMOTE_DIR

    if not args.skip_build:
        if not args.only_lib and args.rdk_version == 7 and device_rdk_version == 6:
            print(
                "Error: --rdk-version 7 produces binaries requiring GLIBC_2.34 / GLIBCXX_3.4.29 "
                "which cannot run on an RDK6 device."
            )
            print("Re-run with --rdk-version 6 (or omit --rdk-version) for this RDK6 device.")
            sys.exit(1)
            return

        rdk_home = resolve_toolchain_dir(args.rdk_version, args.rdk_home)
        if not is_toolchain_installed(rdk_home):
            target_version = args.rdk_version or 6
            print(f"Error: RDK{target_version} toolchain is not set up ({rdk_home}).")
            print(
                f"Please run this script with --setup-toolchain --rdk-version {target_version} "
                "(or pass --rdk-home <path> / set RDK_HOME)."
            )
            sys.exit(1)
            return

        configure_build(
            PLATFORM,
            config,
            out_dir,
            args.no_rbe,
            rdk_version=args.rdk_version,
            rdk_home=args.rdk_home,
        )
        build_output = build_targets(out_dir, targets)
        is_up_to_date = build_output and any(
            msg in build_output for msg in ["Everything is up-to-date", "no work to do"])
    else:
        print("=== Skipping build step ===")
        is_up_to_date = False

    # Check if the remote directory exists on the device.
    # If it doesn't, we must deploy even if the build is up-to-date.
    remote_dir_exists = False
    try:
        out = run_remote_command(
            f"[ -d {remote_dir} ] && echo yes || echo no",
            device_id,
            device_ip,
            check=False,
            verbose=False,
        ).strip()
        remote_dir_exists = (out == "yes")
    except Exception as e:
        print(f"[WARNING] Failed to check if remote directory exists: {e}")

    skip_deployment = args.skip_deploy or (is_up_to_date and not args.force_deploy and remote_dir_exists)

    if skip_deployment:
        print("=== Skipping deployment ===")
        if not args.run:
            return
    else:
        if not verify_abi_compatibility(
            out_dir,
            device_rdk_version,
            is_test=bool(args.tests),
            only_lib=args.only_lib,
        ):
            return
        if args.only_lib:
            deploy_only_lib(device_id, device_ip, out_dir, remote_dir)
        else:
            package_and_deploy(device_id, device_ip, out_dir, remote_dir, deps_file, is_test=bool(args.tests))

    if args.run:
        ensure_dolby_vision_policy(device_id, device_ip)
        launch_on_device(
            device_id,
            device_ip,
            remote_dir,
            args.tests,
            devtools=(config != "gold" and not args.tests),
            param=args.param,
            deeplink=args.deeplink,
        )

    print("=== Finished ===")


if __name__ == "__main__":
    main()
