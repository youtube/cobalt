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
  1. Build and deploy as Cobalt plugin (default: config: qa, out: out/evergreen-arm-hardfp-rdk_qa):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py

  2. Deploy a pre-built package and run it:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --out-dir ~/Downloads/evergreen-arm-hardfp-rdk_qa/ --skip-build --run

  3. Build, deploy, and RUN Cobalt plugin on device:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run

  4. Build, deploy, and RUN nplb tests on device (uses devel config):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --tests nplb --run

  5. Build and deploy as standalone executable (loader_app):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --mode executable

  6. Force deploy and run even if artifacts are up-to-date:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run --force-deploy

  7. Reset RDK display and restart WPEFramework (fixes stuck displays/frozen sessions):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --reset

  8. Deploy only the libcobalt library:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --only-lib

  9. View filtered application logs (YouTube/Cobalt):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --logs

  10. Follow application logs in real-time (journalctl -f):
      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --logs --follow

  11. View raw global OS/system logs (journalctl):
      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --system-logs

  12. Build, deploy, and run Cobalt plugin with Chrome DevTools remote debugging enabled:
      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run --devtools

  13. Download and install cross-compilation toolchain:
      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --setup-toolchain

  14. Revert active Cobalt loader configuration to Cobalt 25:
      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --revert-c25
"""

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
from typing import List, Optional, Union

# Disable AI agent environment variables to avoid siso wrapper flag issues
os.environ.pop("ANTIGRAVITY_AGENT", None)
os.environ.pop("GEMINI_CLI", None)

# Constants
PLATFORM = "evergreen-arm-hardfp-rdk"
DEFAULT_REMOTE_DIR = "/data/out_cobalt"
EXECUTABLE_REMOTE_DIR = "/data/out_loader_app_executable"
TEST_REMOTE_DIR = "/data/test"
MIN_SYSTEM_SOFTWARE_VERSION = "20260420"


def run_command(
    command: Union[str, List[str]],
    verbose: bool = True,
    check: bool = True,
    sleep_time: int = 0,
) -> str:
    """Utility to run shell commands."""
    if verbose:
        cmd_str = command if isinstance(command, str) else " ".join(command)
        print(f">>> Executing: {cmd_str}")

    is_shell = isinstance(command, str)
    process = subprocess.Popen(
        command, shell=is_shell, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
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


def configure_build(platform: str, config: str, out_dir: Path, no_rbe: bool = False) -> None:
    """Runs GN configuration."""
    print(f"=== Configuring {platform} ({config}) ===")
    cmd = [
        "python3", "cobalt/build/gn.py", "-p", platform, "-C", config,
        "--out_directory",
        str(out_dir)
    ]
    if no_rbe:
        cmd.append("--no-rbe")
    run_command(cmd)


def build_targets(out_dir: Path, targets: List[str]) -> str:
    """Builds the specified targets using autoninja."""
    print(f"=== Building {' '.join(targets)} ===")
    return run_command(["autoninja", "-C", str(out_dir)] + targets)


def deploy_only_lib(device_id: str, out_dir: Path, remote_dir: str) -> None:
    """Pushes libcobalt.lz4 directly to the device."""
    local_lz4 = out_dir / "app/cobalt/lib/libcobalt.lz4"
    if not local_lz4.exists():
        print(f"Error: {local_lz4} not found.")
        sys.exit(1)

    remote_lib_dir = f"{remote_dir}/app/cobalt/lib"
    run_command(
        ["adb", "-s", device_id, "shell", f"mkdir -p {remote_lib_dir}"])
    run_command([
        "adb", "-s", device_id, "push",
        str(local_lz4), f"{remote_lib_dir}/libcobalt.lz4"
    ])


def package_and_deploy(
    device_id: str,
    out_dir: Path,
    remote_dir: str,
    deps_file: Optional[Path],
    mode: str,
) -> None:
    """Packages artifacts using runtime_deps and pushes to device."""
    print("=== Packaging & Deploying artifacts ===")
    archive_name = "archive.tar.gz"

    if deps_file and deps_file.exists():
        tar_cmd = ["tar", "-czvf", archive_name, "-C", str(out_dir), "-T", str(deps_file)]
        if mode == "plugin":
            tar_cmd.append("libloader_app.so")
        build_info = out_dir / "gen/build_info.json"
        if build_info.exists():
            tar_cmd.append("gen/build_info.json")
    else:
        if deps_file:
            print(f"Warning: deps_file {deps_file} not found.")
        print(f"Packaging everything in {out_dir}")
        tar_cmd = ["tar", "-czvf", archive_name, "-C", str(out_dir), "."]

    print(f"Packaging with: {' '.join(tar_cmd)}")
    run_command(tar_cmd)
    run_command(["adb", "-s", device_id, "shell", f"mkdir -p {remote_dir}"])
    run_command(["adb", "-s", device_id, "push", archive_name, f"{remote_dir}/"])
    Path(archive_name).unlink(missing_ok=True)


def launch_on_device(
    device_id: str,
    remote_dir: str,
    is_up_to_date: bool,
    force_deploy: bool,
    test_name: Optional[str],
    mode: str,
    devtools: bool = False,
) -> None:
    """Executes remote commands to launch Cobalt or tests."""
    print("=== Launching on device ===")
    remote_cmds = [f"cd {remote_dir}"]

    if not is_up_to_date or force_deploy:
        # Ensure unprivileged container users have access to extracted artifacts.
        remote_cmds += [
            "tar -xzf archive.tar.gz",
            "rm archive.tar.gz",
            f"chmod -R 777 {remote_dir}",
        ]

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
            f"./{test_name}_loader.py {xml_out}{gtest_filter}",
            "rdkDisplay remove",
            "sleep 2",
        ]
    elif mode == "plugin":
        if devtools:
            print("[INFO] Enabling DevTools support...")

        # Deactivate first to change config
        deactivate_json = '{"jsonrpc":"2.0","id":1,"method":"Controller.1.deactivate","params":{"callsign":"YouTube"}}'
        run_command(["adb", "-s", device_id, "shell", f"curl -s http://127.0.0.1:9998/jsonrpc -d '{deactivate_json}'"])

        # Get configuration
        get_config_json = '{"jsonrpc":"2.0","id":1,"method":"Controller.1.configuration@YouTube"}'
        res_str = run_command(["adb", "-s", device_id, "shell", f"curl -s http://127.0.0.1:9998/jsonrpc -d '{get_config_json}'"])

        rpc_deactivate = (
            r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.deactivate\",'
            r'\"params\":{\"callsign\":\"YouTube\"}}')
        try:
            res = json.loads(res_str.strip())
            if "result" in res:
                config = res["result"]
                sb_args = config.get("sbmainargs", [])
                
                # Filter out any existing remote debugging port argument
                sb_args = [arg for arg in sb_args if not arg.startswith("--remote-debugging-port=")]
                
                if devtools:
                    sb_args.append("--remote-debugging-port=9222")
                
                config["sbmainargs"] = sb_args

                # Set configuration
                rpc_set_config = json.dumps({
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "Controller.1.configuration@YouTube",
                    "params": config
                })
                run_command(["adb", "-s", device_id, "shell", f"curl -s http://127.0.0.1:9998/jsonrpc -d '{rpc_set_config}'"])
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

        if devtools:
            print("[INFO] Setting up DevTools port forwarding...")
            run_command(["adb", "-s", device_id, "forward", "tcp:9222", "tcp:9222"])
            print("[INFO] DevTools is enabled. Please open Chrome and navigate to 'chrome://inspect' (add 'localhost:9222' to discover targets).")
    else:
        remote_cmds += [
            "rdkDisplay remove || true",
            "sleep 2",
            "rdkDisplay create",
            "sleep 2",
            "./loader_app",
            "rdkDisplay remove",
            "sleep 2",
        ]

    full_cmd = " && ".join(remote_cmds)
    output = run_command(["adb", "-s", device_id, "shell", f"bash -l -c \"{full_cmd}\""])
    print(output)
    if "ERROR_OPENING_FAILED" in output or "error" in output.lower():
        print("\n[WARNING] Activation failed with error (e.g., ERROR_OPENING_FAILED).")
        print("[WARNING] Please check the physical device state. It might be in setup/Out-of-Box Experience (OOBE) mode or not connected to a network.")


def parse_args() -> argparse.Namespace:
    """Parses command line arguments."""
    parser = argparse.ArgumentParser(
        description="Build and deploy Cobalt to RDK.")
    parser.add_argument(
        "--mode",
        choices=["executable", "plugin"],
        default="plugin",
        help="Deploy as standalone executable or plugin (default).",
    )
    parser.add_argument(
        "--only-lib", action="store_true", help="Deploy only libcobalt.lz4.")
    parser.add_argument(
        "--tests",
        type=str,
        metavar="TEST_NAME",
        help="Build and run a test (e.g., nplb).",
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
        "--run", action="store_true", help="Run on device after build/deploy.")
    parser.add_argument(
        "--force-deploy",
        action="store_true",
        help="Force deployment even if up-to-date.",
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
        help="Download and install the RDK toolchain to RDK_HOME.",
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
    return parser.parse_args()


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

    dev = rdk_devices[0]
    print(f"Using RDK device: {dev} (AH212)")
    return dev


def assert_software_version(device_id: str, min_version_date: str) -> None:
    """Asserts that the device software date is at least the min_version_date."""
    try:
        res = subprocess.run(
            ["adb", "-s", device_id, "shell", "cat /version.txt"],
            capture_output=True,
            text=True,
            timeout=5
        )
        if res.returncode != 0:
            print("Error: Could not read /version.txt from the device to verify system software version.")
            sys.exit(1)

        custom_version = None
        for line in res.stdout.splitlines():
            if "custom version:" in line.lower():
                custom_version = line.split(":", 1)[1].strip()
                break

        if not custom_version:
            print("Error: 'custom version' field not found in /version.txt.")
            sys.exit(1)

        # Extract date suffix from custom version (e.g. RDK_V6_AH212_20260330 -> 20260330)
        parts = custom_version.split("_")
        if not parts:
            print(f"Error: Could not parse date from custom version: {custom_version}")
            sys.exit(1)

        date_str = parts[-1]
        try:
            min_date = datetime.strptime(min_version_date, "%Y%m%d").date()
        except ValueError:
            print(f"Error: Invalid min_version_date format: {min_version_date}. Expected YYYYMMDD.")
            sys.exit(1)

        try:
            device_date = datetime.strptime(date_str, "%Y%m%d").date()
        except ValueError:
            print(f"Error: Extracted version suffix '{date_str}' is not a valid YYYYMMDD calendar date.")
            sys.exit(1)

        if device_date < min_date:
            print(f"Error: Device system software version ({date_str}) is older than the required minimum version ({min_version_date}).")
            sys.exit(1)

        print(f"Device system software version verified: {date_str} (required: >= {min_version_date})")

    except Exception as e:
        print(f"Error checking software version: {e}")
        sys.exit(1)


def check_and_switch_cobalt_version(device_id: str) -> None:
    """Checks if Cobalt 26 is active on the device, otherwise switches and reboots."""
    try:
        res = subprocess.run(
            ["adb", "-s", device_id, "shell", "readlink /usr/lib/libloader_app.so"],
            capture_output=True,
            text=True,
            timeout=5
        )
        current_target = res.stdout.strip()

        # If it already points to /data/out_cobalt/libloader_app.so, it's already set to c26
        if current_target == "/data/out_cobalt/libloader_app.so":
            print("Cobalt 26 configuration is already active on the device.")
            return

        print("Cobalt configuration is not active. Running 'chCobalt custom_cobalt' on the device...")
        res = subprocess.run(
            ["adb", "-s", device_id, "shell", "chCobalt custom_cobalt"],
            capture_output=True,
            text=True,
            timeout=10
        )
        if res.returncode != 0:
            print(f"Error: Failed to run 'chCobalt custom_cobalt': {res.stderr}")
            sys.exit(1)

        print("Device is being rebooted to apply changes...")
        subprocess.run(
            ["adb", "-s", device_id, "shell", "reboot"],
            timeout=5
        )

        print("Waiting 30 seconds for the device to reboot...")
        time.sleep(30)

        print("Attempting to reconnect to device via ADB...")
        reconnected = False
        for i in range(10): # try up to 10 times with 3 second intervals
            res = subprocess.run(
                ["adb", "-s", device_id, "shell", "echo ok"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if res.returncode == 0 and res.stdout.strip() == "ok":
                reconnected = True
                break
            print(f"Device not ready yet. Retrying in 3 seconds... ({i+1}/10)")
            time.sleep(3)

        if not reconnected:
            print("Error: Could not reconnect to the device via ADB after reboot.")
            sys.exit(1)

        print("Reconnected to device successfully.")

    except Exception as e:
        print(f"Error during Cobalt version switch: {e}")
        sys.exit(1)


def revert_to_cobalt_25(device_id: str) -> None:
    """Reverts the active Cobalt configuration on the device back to Cobalt 25."""
    target = run_command(
        ["adb", "-s", device_id, "shell", "readlink /usr/lib/libloader_app.so"]
    ).strip()

    if target != "/data/out_cobalt/libloader_app.so":
        print("Cobalt 25 is already active on the device.")
        return

    print("Running 'chCobalt c25' on the device...")
    run_command(["adb", "-s", device_id, "shell", "chCobalt c25"])
    run_command(["adb", "-s", device_id, "shell", "reboot -f"])
    print("Revert to Cobalt 25 completed. The device is rebooting.")


def is_toolchain_installed(rdk_home: Optional[str]) -> bool:
    """Checks if the RDK toolchain is installed in the target path."""
    return bool(rdk_home and os.path.exists(os.path.join(rdk_home, "sysroots")))


def setup_toolchain() -> None:
    """Downloads and installs the RDK toolchain."""
    rdk_home = os.environ.get("RDK_HOME")
    if not rdk_home:
        print("Error: RDK_HOME environment variable is not set.")
        print("Please add it to your ~/.bashrc (e.g., export RDK_HOME=/workspaces/rdk/toolchain) and restart your shell.")
        sys.exit(1)

    if is_toolchain_installed(rdk_home):
        print(f"RDK toolchain is already installed in: {rdk_home}. Skipping setup.")
        return

    url = "https://storage.googleapis.com/cobalt-static-storage-public/20250521_rdk-glibc-x86_64-arm-toolchain-2.0.sh"
    print(f"=== Setting up toolchain in: {rdk_home} ===")
    with tempfile.TemporaryDirectory() as tmp_dir:
        installer_path = os.path.join(tmp_dir, "installer.sh")
        run_command(f"wget {url} -O '{installer_path}'")
        run_command(f"sh '{installer_path}' -d '{rdk_home}' -y")


def main() -> None:
    """Main execution flow."""
    args = parse_args()

    if args.setup_toolchain:
        setup_toolchain()
        return

    if args.revert_c25:
        device_id = get_device_id()
        revert_to_cobalt_25(device_id)
        return

    if args.logs or args.system_logs:
        device_id = get_device_id()
        cmd = ["adb", "-s", device_id, "shell", "journalctl"]
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
        device_id = get_device_id()
        print("=== Resetting display ===")
        run_command([
            "adb", "-s", device_id, "shell", "bash -l -c 'rdkDisplay remove || true'"
        ])
        print("=== Restarting WPEFramework ===")
        run_command([
            "adb", "-s", device_id, "shell", "systemctl restart wpeframework"
        ], sleep_time=5)
        if not (args.run or args.tests or args.force_deploy):
            print("=== Reset finished. ===")
            return

    device_id = get_device_id()
    assert_software_version(device_id, MIN_SYSTEM_SOFTWARE_VERSION)
    if args.mode == "plugin" and not args.tests:
        check_and_switch_cobalt_version(device_id)

    # Setup Build Paths
    config = args.config or ("devel" if args.tests else "qa")
    out_dir = Path(args.out_dir or f"out/{PLATFORM}_{config}")

    if args.tests:
        targets = [f"{args.tests}_loader"]
        remote_dir = TEST_REMOTE_DIR
        deps_file = out_dir / f"{args.tests}_loader.runtime_deps"
    elif args.only_lib:
        targets = ["cobalt"]
        remote_dir = DEFAULT_REMOTE_DIR if args.mode == "plugin" else EXECUTABLE_REMOTE_DIR
        deps_file = None
    else:
        # Standard deployment uses cobalt_loader to generate the runtime_deps list.
        targets = ["cobalt_loader", "loader_app"]
        deps_file = out_dir / "cobalt_loader.runtime_deps"
        
        if args.mode == "plugin":
            targets.append("loader_app_rdk_plugin")
            remote_dir = DEFAULT_REMOTE_DIR
        else:
            remote_dir = EXECUTABLE_REMOTE_DIR

    if not args.skip_build:
        rdk_home = os.environ.get("RDK_HOME")
        if not is_toolchain_installed(rdk_home):
            print(f"Error: RDK toolchain is not set up in RDK_HOME ({rdk_home}).")
            print("Please run this script with --setup-toolchain to download and install the toolchain.")
            print("Also, make sure to add it to your ~/.bashrc (e.g., export RDK_HOME=/workspaces/rdk/toolchain).")
            sys.exit(1)

        configure_build(PLATFORM, config, out_dir, args.no_rbe)
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
        res = subprocess.run(
            ["adb", "-s", device_id, "shell", f"[ -d {remote_dir} ]"],
            capture_output=True,
            timeout=5
        )
        remote_dir_exists = (res.returncode == 0)
    except Exception as e:
        print(f"[WARNING] Failed to check if remote directory exists: {e}")

    if is_up_to_date and not args.force_deploy and remote_dir_exists:
        print("=== Up to date. Skipping deployment. ===")
        if not args.run:
            return
    else:
        if args.only_lib:
            deploy_only_lib(device_id, out_dir, remote_dir)
        else:
            package_and_deploy(device_id, out_dir, remote_dir, deps_file, "executable" if args.tests else args.mode)

    if args.run:
        launch_on_device(
            device_id,
            remote_dir,
            is_up_to_date,
            args.force_deploy,
            args.tests,
            "executable" if args.tests else args.mode,
            config != "gold" and args.mode == "plugin" and not args.tests,
        )

    print("=== Finished ===")


if __name__ == "__main__":
    main()
