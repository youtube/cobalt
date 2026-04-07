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
#
# Usage Examples:
#   Prerequisite: Set the RDK_DEVICE_ID environment variable.
#   (Obtain the ID by running 'adb devices')
#   export RDK_DEVICE_ID=<device_id>
#
#   1. Build and deploy as Cobalt plugin (default: config: qa, out: out/evergreen-arm-hardfp-rdk_qa):
#      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py
#
#   2. Build, deploy, and RUN Cobalt plugin on device:
#      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run
#
#   3. Build, deploy, and RUN nplb tests on device (uses devel config):
#      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --tests nplb --run
#
#   4. Build and deploy as standalone executable (loader_app):
#      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --mode executable
#
#   5. Force deploy and run even if artifacts are up-to-date:
#      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --run --force-deploy
#
#   6. Reset RDK display (fixes stuck sessions caused by executable mode):
#      python3 starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py --reset
#
#
# Note: The script applies rdk_plugin_temporary.patch at the start and skips deployment if autoninja is up-to-date.
#       This patch is temporary and will be deleted after https://github.com/youtube/cobalt/pull/9914 is submitted.
#       This script MUST be run from the repository root.

import argparse
import json
import os
import subprocess
import sys

PLATFORM = "evergreen-arm-hardfp-rdk"

def run_command(command, capture_output=False, verbose=True, check=True, stream_output=False):
    """Utility to run shell commands with optimized performance."""
    if verbose:
        cmd_str = command if isinstance(command, str) else ' '.join(command)
        print(f">>> Executing: {cmd_str}")
    
    is_shell = isinstance(command, str)
    
    if stream_output:
        process = subprocess.Popen(
            command, 
            shell=is_shell, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.STDOUT, 
            text=True, 
            bufsize=1
        )
        full_output = []
        for line in process.stdout:
            print(line, end='', flush=True)
            full_output.append(line)
        process.wait()
        if check and process.returncode != 0:
            sys.exit(process.returncode)
        return "".join(full_output)

    process = subprocess.run(
        command, 
        shell=is_shell, 
        check=False, 
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.PIPE if capture_output else None, 
        text=True
    )
    
    if check and process.returncode != 0:
        print(f"Error executing command.")
        sys.exit(process.returncode)
        
    return process.stdout if capture_output else ""

def main():
    parser = argparse.ArgumentParser(description="Build and deploy Cobalt to RDK.")
    parser.add_argument('--mode', choices=['executable', 'plugin'], default='plugin',
                        help='Deploy as standalone executable or plugin (default).')
    parser.add_argument('--only-lib', action='store_true', help='Deploy only libcobalt.lz4.')
    parser.add_argument('--tests', type=str, metavar='TEST_NAME', help='Build and run a test (e.g., nplb).')
    parser.add_argument('--config', type=str, help='Override default build configuration.')
    parser.add_argument('--out-dir', type=str, help='Custom build output directory.')
    parser.add_argument('--run', action='store_true', help='Run on device after build/deploy.')
    parser.add_argument('--force-deploy', action='store_true', help='Force deployment even if up-to-date.')
    parser.add_argument('--reset', action='store_true', help='Run rdkDisplay remove on device. Useful if the display is inactive due to a previous executable mode session.')
    args = parser.parse_args()

    device_id = os.environ.get('RDK_DEVICE_ID')
    if not device_id:
        print("Error: RDK_DEVICE_ID environment variable must be set.")
        sys.exit(1)

    if args.reset:
        print("=== Resetting display ===")
        run_command(["adb", "-s", device_id, "shell", "bash -l -c 'rdkDisplay remove || true'"])
        # If no other core actions are requested, exit early
        if not (args.run or args.tests or args.force_deploy):
            print("=== Reset finished. ===")
            return


    # 1. Apply Patch if missing
    if os.path.exists("rdk_plugin_temporary.patch"):
        is_applied = subprocess.run(
            ["git", "apply", "--reverse", "--check", "rdk_plugin_temporary.patch"], 
            capture_output=True
        ).returncode == 0
        
        if not is_applied:
            print("=== Applying rdk_plugin_temporary.patch ===")
            run_command(["git", "apply", "rdk_plugin_temporary.patch"])
        else:
            print("=== Patch already applied. ===")

    # 2. Setup Build Paths
    config = args.config or ("devel" if args.tests else "qa")
    out_dir = args.out_dir or f"out/{PLATFORM}_{config}"
    
    if args.tests:
        target = [f"{args.tests}_loader"]
        remote_dir = "/data/test"
        deps_file = f"{out_dir}/{args.tests}_loader.runtime_deps"
    else:
        target = ["loader_app", "cobalt"]
        if args.mode == 'plugin':
            remote_dir = "/data/out_cobalt"
            target.append("libloader_app.so")
        else:
            remote_dir = "/data/out_loader_app_executable"
        deps_file = f"{out_dir}/cobalt_loader.runtime_deps"

    # 3. GN Configuration & Build
    print(f"=== Configuring {PLATFORM} ({config}) ===")
    run_command(["python3", "cobalt/build/gn.py", "-p", PLATFORM, "-C", config, "--out_directory", out_dir])
    
    print(f"=== Building {' '.join(target)} ===")
    build_output = run_command(["autoninja", "-C", out_dir] + target, stream_output=True)

    # 4. Deployment Check
    is_up_to_date = build_output and any(msg in build_output for msg in ["Everything is up-to-date", "no work to do"])
    
    if is_up_to_date and not args.force_deploy:
        print("=== Up to date. Skipping deployment. ===")
        if not args.run:
            return
    else:
        print("=== Packaging & Deploying artifacts ===")
        os.makedirs(f"{out_dir}/gen", exist_ok=True)
        if os.path.exists("gen/build_info.json"):
            run_command(f"cp gen/build_info.json {out_dir}/gen/")
        
        # Use the exact tar logic requested
        tar_cmd = f"tar -czf archive.tar.gz -C {out_dir} -T {deps_file}"
        if args.mode == 'plugin' and os.path.exists(f"{out_dir}/libloader_app.so"):
            tar_cmd += " libloader_app.so"
        elif args.mode == 'executable' and os.path.exists(f"{out_dir}/loader_app"):
            tar_cmd += " loader_app"
            
        run_command(tar_cmd)
        run_command(["adb", "-s", device_id, "shell", f"mkdir -p {remote_dir}"])
        run_command(["adb", "-s", device_id, "push", "archive.tar.gz", f"{remote_dir}/"])
        os.remove("archive.tar.gz")


    # 5. Remote Execution
    if args.run:
        print("=== Launching on device ===")
        remote_cmds = [f"cd {remote_dir}"]
        if not is_up_to_date or args.force_deploy:
            remote_cmds += ["tar -xzf archive.tar.gz", "rm archive.tar.gz"]
        
        if args.tests:
            remote_cmds += ["rdkDisplay remove || true", "sleep 2"]
            remote_cmds.append("mkdir -p results")
            gtest_filter = ""
            filter_file = f"cobalt/testing/filters/{PLATFORM}/{args.tests}_loader_filter.json"
            if os.path.exists(filter_file):
                with open(filter_file) as f:
                    fails = json.load(f).get('failing_tests', [])
                    if fails: gtest_filter = f" --gtest_filter=-{':'.join(fails)}"
            
            xml_out = f"--gtest_output=xml:{remote_dir}/results/{args.tests}_loader.xml"
            remote_cmds += [
                "rdkDisplay create", "sleep 2",
                f"./{args.tests}_loader.py {xml_out}{gtest_filter}",
                "rdkDisplay remove", "sleep 2"
            ]
        elif args.mode == 'plugin':
            # Fresh launch: reset display -> deactivate -> sleep -> activate
            rpc_deactivate = r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.deactivate\",\"params\":{\"callsign\":\"YouTube\"}}'
            rpc_activate = r'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"Controller.1.activate\",\"params\":{\"callsign\":\"YouTube\"}}'
            remote_cmds += [
                "rdkDisplay remove || true",
                "sleep 2",
                f"curl http://127.0.0.1:9998/jsonrpc -d '{rpc_deactivate}'",
                "sleep 2",
                f"curl http://127.0.0.1:9998/jsonrpc -d '{rpc_activate}'"
            ]
        else:
            remote_cmds += ["rdkDisplay remove || true", "sleep 2"]
            remote_cmds += [
                "rdkDisplay create", "sleep 2",
                "./loader_app",
                "rdkDisplay remove", "sleep 2"
            ]
        
        full_cmd = " && ".join(remote_cmds)
        # Wrap the full command in escaped double quotes to ensure adb passes it as one unit
        run_command(["adb", "-s", device_id, "shell", f"bash -l -c \"{full_cmd}\""])

    print("=== Finished ===")

if __name__ == '__main__':
    main()
