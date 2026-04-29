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

Prerequisite:
  Set the RDK_DEVICE_ID environment variable.
  (Obtain the ID by running 'adb devices')
  export RDK_DEVICE_ID=<device_id>

Usage Examples:
  1. Build and deploy as Cobalt plugin (default: config: qa, out: out/evergreen-arm-hardfp-rdk_qa):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py

  2. Build, deploy, and RUN Cobalt plugin on device:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run

  3. Build, deploy, and RUN nplb tests on device (uses devel config):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --tests nplb --run

  4. Build and deploy as standalone executable (loader_app):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --mode executable

  5. Force deploy and run even if artifacts are up-to-date:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run --force-deploy

  6. Reset RDK display (fixes stuck sessions caused by executable mode):
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --reset

  7. Deploy only the libcobalt library:
     python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --only-lib
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import List, Optional, Union

# Constants
PLATFORM = "evergreen-arm-hardfp-rdk"
DEFAULT_REMOTE_DIR = "/data/out_cobalt"
EXECUTABLE_REMOTE_DIR = "/data/out_loader_app_executable"
TEST_REMOTE_DIR = "/data/test"


def run_command(
    command: Union[str, List[str]],
    capture_output: bool = False,
    verbose: bool = True,
    check: bool = True,
    stream_output: bool = False,
) -> str:
    """Utility to run shell commands."""
    if verbose:
        cmd_str = command if isinstance(command, str) else " ".join(command)
        print(f">>> Executing: {cmd_str}")

    is_shell = isinstance(command, str)

    if stream_output:
        process = subprocess.Popen(
            command,
            shell=is_shell,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        full_output = []
        if process.stdout:
            for line in process.stdout:
                print(line, end="", flush=True)
                full_output.append(line)
        process.wait()
        if check and process.returncode != 0:
            raise subprocess.CalledProcessError(process.returncode, command,
                                              "".join(full_output))
        return "".join(full_output)

    process = subprocess.run(
        command,
        shell=is_shell,
        check=False,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.PIPE if capture_output else None,
        text=True,
    )

    if check and process.returncode != 0:
        print(f"Error executing command: {process.stderr}")
        sys.exit(process.returncode)

    return process.stdout if capture_output else ""


def configure_build(platform: str, config: str, out_dir: Path) -> None:
    """Runs GN configuration."""
    print(f"=== Configuring {platform} ({config}) ===")
    run_command([
        "python3", "cobalt/build/gn.py", "-p", platform, "-C", config,
        "--out_directory",
        str(out_dir)
    ])


def build_targets(out_dir: Path, targets: List[str]) -> str:
    """Builds the specified targets using autoninja."""
    print(f"=== Building {' '.join(targets)} ===")
    return run_command(["autoninja", "-C", str(out_dir)] + targets,
                       stream_output=True)


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
    deps_file: Path,
    mode: str,
) -> None:
    """Packages artifacts using runtime_deps and pushes to device."""
    print("=== Packaging & Deploying artifacts ===")
    archive_name = "archive.tar.gz"

    if mode == "plugin":
        tar_cmd = ["tar", "-czf", archive_name, "-C", str(out_dir), "-T", deps_file.name, "libloader_app.so"]
    else:
        tar_cmd = ["tar", "-czf", archive_name, "-C", str(out_dir), "-T", deps_file.name]

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
) -> None:
    """Executes remote commands to launch Cobalt or tests."""
    print("=== Launching on device ===")
    remote_cmds = [f"cd {remote_dir}"]

    if not is_up_to_date or force_deploy:
        remote_cmds += ["tar -xzf archive.tar.gz", "rm archive.tar.gz"]

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
        rpc_deactivate = (
            r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.deactivate\",'
            r'\"params\":{\"callsign\":\"YouTube\"}}')
        rpc_activate = (
            r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.activate\",'
            r'\"params\":{\"callsign\":\"YouTube\"}}')
        remote_cmds += [
            "rdkDisplay remove || true",
            "sleep 2",
            f"curl http://127.0.0.1:9998/jsonrpc -d '{rpc_deactivate}'",
            "sleep 2",
            f"curl http://127.0.0.1:9998/jsonrpc -d '{rpc_activate}'",
        ]
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
    run_command(["adb", "-s", device_id, "shell", f"bash -l -c \"{full_cmd}\""])


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
            "Run rdkDisplay remove on device. Useful if the display is inactive "
            "due to a previous executable mode session."),
    )
    return parser.parse_args()


def main() -> None:
    """Main execution flow."""
    args = parse_args()

    device_id = os.environ.get("RDK_DEVICE_ID")
    if not device_id:
        print("Error: RDK_DEVICE_ID environment variable must be set.")
        sys.exit(1)

    if args.reset:
        print("=== Resetting display ===")
        run_command([
            "adb", "-s", device_id, "shell", "bash -l -c 'rdkDisplay remove || true'"
        ])
        if not (args.run or args.tests or args.force_deploy):
            print("=== Reset finished. ===")
            return

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

    configure_build(PLATFORM, config, out_dir)
    build_output = build_targets(out_dir, targets)

    # Deployment Check
    is_up_to_date = build_output and any(
        msg in build_output for msg in ["Everything is up-to-date", "no work to do"])

    if is_up_to_date and not args.force_deploy:
        print("=== Up to date. Skipping deployment. ===")
        if not args.run:
            return
    else:
        if args.only_lib:
            deploy_only_lib(device_id, out_dir, remote_dir)
        else:
            package_and_deploy(device_id, out_dir, remote_dir, deps_file, args.mode)

    if args.run:
        launch_on_device(
            device_id,
            remote_dir,
            is_up_to_date,
            args.force_deploy,
            args.tests,
            args.mode,
        )

    print("=== Finished ===")


if __name__ == "__main__":
    main()
