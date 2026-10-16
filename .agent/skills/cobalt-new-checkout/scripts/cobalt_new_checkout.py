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
"""Cobalt New Checkout Setup Framework.

Orchestrates verification, initialization, and environmental checks for a
freshly initialized source repository containing Cobalt development files.
"""

# pylint: disable=bad-indentation

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
from typing import Optional, List


def stream_reader(pipe, stream, destination_list):
    """Reads from process pipe line-by-line to capture logs live."""
    try:
        for line in iter(pipe.readline, ""):
            if line:
                stream.write(line)
                stream.flush()
                destination_list.append(line)
    except (ValueError, IOError):
        pass
    finally:
        pipe.close()


def execute_command(
    args: List[str],
    cwd: Optional[Path] = None,
    check: bool = True,
    env: Optional[dict] = None,
) -> subprocess.CompletedProcess:
    """Runs a shell command streaming standard execution outputs asynchronously.

    Enables live printing to screen while properly retaining diagnostic strings
    on subprocess execution errors.
    """
    current_env = os.environ.copy()
    if env:
        current_env.update(env)

    print(f'\n>>> Running: {" ".join(args)}')

    stdout_lines = []
    stderr_lines = []

    try:
        # Initialize process execution context tracking
        process = subprocess.Popen(  # pylint: disable=consider-using-with
            args,
            cwd=str(cwd) if cwd else None,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=current_env,
            bufsize=1,  # Line-buffered output configuration
        )

        # Launch background thread readers for standard stream tracking
        t1 = threading.Thread(target=stream_reader,
                              args=(process.stdout, sys.stdout, stdout_lines))
        t2 = threading.Thread(target=stream_reader,
                              args=(process.stderr, sys.stderr, stderr_lines))

        t1.start()
        t2.start()

        # Join process return codes
        return_code = process.wait()

        t1.join()
        t2.join()

        stdout_str = "".join(stdout_lines)
        stderr_str = "".join(stderr_lines)

        # Raise runtime exception on unexpected subprocess termination states
        if check and return_code != 0:
            raise subprocess.CalledProcessError(
                returncode=return_code,
                cmd=args,
                output=stdout_str,
                stderr=stderr_str,
            )

        return subprocess.CompletedProcess(
            args,
            returncode=return_code,
            stdout=stdout_str,
            stderr=stderr_str,
        )

    except subprocess.CalledProcessError as e:
        print(f'❌ Command failed: {" ".join(args)}', file=sys.stderr)
        if e.output:
            print(f"Output Summary:\n{e.output}", file=sys.stderr)
        if e.stderr:
            print(f"Diagnostics Details:\n{e.stderr}", file=sys.stderr)
        raise


class CobaltCheckoutSetup:
    """Orchestrates checkout steps for Cobalt source checkouts."""

    def __init__(self, options: argparse.Namespace):
        self.options = options
        self.repo_root = Path.cwd().resolve()
        self.is_internal = False
        self.github_user: Optional[str] = None

        # Locate tracking file inside top directory parent enclosing project
        # sources
        self.checkpoint_file = self.repo_root.parent / ".setup_checkpoints.json"
        self.checkpoints = self._load_checkpoints()

    def _load_checkpoints(self) -> dict:
        """Loads progress verification data file from workspace disk storage."""
        if self.options.reset_checkpoints:
            print("Bypassing stored status data: Re-running all "
                  "configuration phases.")
            return {}

        if self.checkpoint_file.exists():
            try:
                with open(self.checkpoint_file, "r",
                          encoding="utf-8") as handle:
                    state = json.load(handle)
                    if isinstance(state, dict):
                        return state
            except (IOError, json.JSONDecodeError) as err:
                print(
                    f"Warning: checkpoint tracking manifest parse error: {err}",
                    file=sys.stderr)

        return {}

    def _save_checkpoint(self, stage_name: str):
        """Writes milestone status to tracking storage."""
        self.checkpoints[stage_name] = True
        try:
            with open(self.checkpoint_file, "w", encoding="utf-8") as handle:
                json.dump(self.checkpoints, handle, indent=2)
            print(f"✓ Marked phase milestone completed: {stage_name}")
        except IOError as err:
            print(
                f"Warning: failed recording validation state on storage: {err}",
                file=sys.stderr)

    def is_stage_completed(self, stage_name: str) -> bool:
        """Verifies if execution phase state holds valid confirmation status."""
        return self.checkpoints.get(stage_name, False)

    def validate_configuration(self):
        """Confirms mandatory argument compliance early to fail fast."""
        if self.options.non_interactive:
            if not self.options.github_user:
                raise ValueError(
                    "Early Validation Failure: Parameter `--github-user` "
                    "is required for non-interactive runs when configuring "
                    "fork targets.")

    def prompt_user(self,
                    prompt_msg: str,
                    default: Optional[str] = None) -> str:
        """Handles operational terminal user interaction or fails
        non-interactively.
        """
        if self.options.non_interactive:
            if default is not None:
                return default
            raise ValueError(
                "Missing required parameter for automated runtime: "
                f"{prompt_msg}")

        suffix = f" [{default}]" if default else ""
        val = input(f"{prompt_msg}{suffix}: ").strip()
        return val if val else (default or "")

    def initialize(self):
        """Executes the system pipeline sequentially ensuring step checking."""
        print("=== Cobalt New Checkout Setup ===")

        if not (self.repo_root / ".git").is_dir():
            print(
                "Error: Please run this script from the root of the Cobalt Git "
                "repository.",
                file=sys.stderr)
            sys.exit(1)

        # Perform setup safety validation early
        self.validate_configuration()

        # 0. Handle Internal Status
        if self.options.internal is not None:
            self.is_internal = self.options.internal
        else:
            choice = self.prompt_user(
                "Are you an internal Google user? (y/n)").lower()
            self.is_internal = choice.startswith("y")

        # Execution checkpoints system skipping stages that are already finished

        # 1. Setup Depot tools integration
        if not self.is_stage_completed("depot_tools"):
            self.setup_depot_tools()
            self._save_checkpoint("depot_tools")
        else:
            print("Skipping Phase: depot_tools (Already marked complete)")

        # 2. Add Remotes
        if not self.is_stage_completed("remotes"):
            self.setup_remotes()
            self._save_checkpoint("remotes")
        else:
            print("Skipping Phase: remotes (Already marked complete)")

        # 3. Set up gclient
        if not self.is_stage_completed("gclient_setup"):
            self.setup_gclient()
            self._save_checkpoint("gclient_setup")
        else:
            print("Skipping Phase: gclient_setup (Already marked complete)")

        # 4. Run gclient sync
        if not self.is_stage_completed("gclient_sync"):
            self.run_gclient_sync()
            self._save_checkpoint("gclient_sync")
        else:
            print("Skipping Phase: gclient_sync (Already marked complete)")

        # 5. Disable Build Telemetry
        if not self.is_stage_completed("disable_telemetry"):
            self.disable_telemetry()
            self._save_checkpoint("disable_telemetry")
        else:
            print(
                "Skipping Phase: disable_telemetry (Already marked complete)")

        # 6. Generate Build Files
        if not self.is_stage_completed("generate_build"):
            self.generate_build_files()
            self._save_checkpoint("generate_build")
        else:
            print("Skipping Phase: generate_build (Already marked complete)")

        # 7. Enable pre-commit
        if not self.is_stage_completed("pre_commit"):
            self.setup_pre_commit()
            self._save_checkpoint("pre_commit")
        else:
            print("Skipping Phase: pre_commit (Already marked complete)")

        # 8. Build
        if not self.is_stage_completed("build_targets"):
            self.build_targets()
            self._save_checkpoint("build_targets")
        else:
            print("Skipping Phase: build_targets (Already marked complete)")

        print("\n🎉 Checkout and build setup completed successfully!")

    def setup_depot_tools(self):
        """Installs, updates, and validates Google chromium repository helper
    systems.
    """
        print("\n--- Setting up depot_tools ---")

        # Design requirement: depot_tools must be created within top parent
        # directory enclosing src/
        tools_dir = self.repo_root.parent / "tools"
        depot_tools_dir = tools_dir / "depot_tools"

        if depot_tools_dir.is_dir():
            print(f"depot_tools verified in {depot_tools_dir}")
        else:
            if not self.options.non_interactive:
                input(f"depot_tools not found at {depot_tools_dir}. "
                      "Press Enter to clone it there...")

            tools_dir.mkdir(parents=True, exist_ok=True)
            execute_command([
                "git", "clone",
                "https://chromium.googlesource.com/chromium/tools/"
                "depot_tools.git",
                str(depot_tools_dir)
            ])

        # Prepend depot_tools into process execution context environment tables
        path_val = os.environ.get("PATH", "")
        os.environ["PATH"] = f"{depot_tools_dir}:{path_val}"
        try:
            print("Verifying gclient tools integration...")
            execute_command(["gclient", "--version"])
        except subprocess.CalledProcessError as err:
            print(
                "Depot tools bootstrap failure: Validation was not "
                "completed via gclient.",
                file=sys.stderr)
            raise err

        print(f"Prepended {depot_tools_dir} to current session PATH variable.")

        if not self.options.non_interactive:
            print(
                "\nPlease add the following to your ~/.bashrc manually if not "
                "present:")
            print(f'export PATH="{depot_tools_dir}:$PATH"')
            input("Press Enter to continue...")

    def setup_remotes(self):
        """Injects fork integration remotes dynamically while validating
    connections.
    """
        print("\n--- Adding Remotes ---")

        # Safely check for base origin/upstream _gclient remote configurations
        try:
            execute_command(["git", "remote", "get-url", "_gclient"],
                            cwd=self.repo_root)
            print("Remote `_gclient` confirmed.")
        except subprocess.CalledProcessError:
            execute_command([
                "git", "remote", "add", "_gclient",
                "git@github.com:youtube/cobalt.git"
            ],
                            cwd=self.repo_root)

        if self.options.github_user:
            self.github_user = self.options.github_user
        else:
            self.github_user = self.prompt_user(
                "Enter your GitHub username to add fork remote")

        if self.github_user:
            fork_target_url = f"git@github.com:{self.github_user}/cobalt.git"
            try:
                process_res = execute_command(
                    ["git", "remote", "get-url", "fork"],
                    cwd=self.repo_root,
                    check=False)
                current_remote = process_res.stdout.strip(
                ) if process_res.returncode == 0 else ""

                if current_remote:
                    if current_remote == fork_target_url:
                        print("Remote `fork` configured correctly.")
                    else:
                        print("Updating `fork` URL remote configuration: "
                              f"{current_remote} -> {fork_target_url}")
                        execute_command([
                            "git", "remote", "set-url", "fork", fork_target_url
                        ],
                                        cwd=self.repo_root)
                else:
                    execute_command(
                        ["git", "remote", "add", "fork", fork_target_url],
                        cwd=self.repo_root)
            except subprocess.CalledProcessError:
                execute_command(
                    ["git", "remote", "add", "fork", fork_target_url],
                    cwd=self.repo_root)

    def setup_gclient(self):
        """Constructs declarative gclient configuration workspace parameters."""
        print("\n--- Setting up gclient ---")

        # Requirement: gclient workflow looks for definitions located in
        # immediate parent folder
        parent_dir = self.repo_root.parent

        config_args = [
            "gclient", "config", "--name=src", "--unmanaged",
            "git@github.com:youtube/cobalt.git"
        ]
        if self.is_internal:
            print("Configuring for internal user with RBE instances...")
            config_args.append(
                "--custom-var=rbe_instance="
                "\"projects/cobalt-actions-prod/instances/default_instance\"")
        else:
            print("Configuring for external user...")

        execute_command(config_args, cwd=parent_dir)

        gclient_file = parent_dir / ".gclient"
        if gclient_file.exists():
            content = gclient_file.read_text()
            if "target_os" not in content:
                print("Appending target_os and target_cpu configuration "
                      "parameters to .gclient file...")
                with open(gclient_file, "a", encoding="utf-8") as handle:
                    handle.write("\ntarget_os = [ 'linux', 'android' ]\n")
                    handle.write("target_cpu = ['x64', 'arm', 'arm64']\n")

    def run_gclient_sync(self):
        """Fetches repository dependencies utilizing internal toolchain
        syncs.
        """
        print("\n--- Running gclient sync ---")
        try:
            rev = subprocess.check_output(["git", "rev-parse", "@"],
                                          text=True).strip()
        except subprocess.CalledProcessError:
            rev = "HEAD"

        sync_args = ["gclient", "sync", "--no-history", "-r", rev]
        execute_command(sync_args, cwd=self.repo_root)

    def disable_telemetry(self):
        """Opts out of reporting build usage trends."""
        if self.is_internal:
            print("\n--- Disabling Build Telemetry ---")
            execute_command(["build_telemetry", "opt-out"],
                            cwd=self.repo_root.parent,
                            check=False)

    def generate_build_files(self):
        """Issues compilation preparation files for target architectures."""
        print("\n--- Generating Build Files ---")
        gn_script = self.repo_root / "cobalt" / "build" / "gn.py"
        execute_command([
            sys.executable,
            str(gn_script), "-p", "linux-x64x11", "-c", "devel"
        ],
                        cwd=self.repo_root)

    def setup_pre_commit(self):
        """Installs pre-commit rules supporting automated Git syntax checks."""
        print("\n--- Enabling pre-commit ---")
        if shutil.which("pre-commit"):
            try:
                execute_command(["pre-commit", "clean"],
                                cwd=self.repo_root,
                                check=True)
                execute_command([
                    "pre-commit", "install", "-t", "pre-commit", "-t",
                    "pre-push"
                ],
                                cwd=self.repo_root,
                                check=True)
            except subprocess.CalledProcessError as err:
                print(f"Warning: setup failure for pre-commit routines: {err}",
                      file=sys.stderr)
        else:
            print(
                "Warning: `pre-commit` executable was not detected in PATH, "
                "skipping configuration.",
                file=sys.stderr)

    def build_targets(self):
        """Compiles workspace sources with ninja tasks integration."""
        print("\n--- Building targets ---")
        if shutil.which("autoninja"):
            execute_command(
                ["autoninja", "-C", "out/linux-x64x11_devel", "cobalt:gn_all"],
                cwd=self.repo_root)
        else:
            print(
                "Warning: `autoninja` was not detected in PATH, skipping build "
                "compilation step.",
                file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="Orchestrates checkout steps for Cobalt source checkouts.")
    parser.add_argument("--non-interactive",
                        action="store_true",
                        help="Run non-interactively bypassing input prompts.")
    parser.add_argument(
        "--internal",
        action="store_true",
        default=None,
        help="Identifies whether setup is inside internal context.")
    parser.add_argument("--no-internal",
                        action="store_false",
                        dest="internal",
                        help="Identifies setup context as external user.")
    parser.add_argument(
        "--github-user",
        type=str,
        help="User's Github identifier for adding workspace remotes.")
    parser.add_argument(
        "--reset-checkpoints",
        action="store_true",
        help="Reset the setup state checkpoints manifest file.")

    args = parser.parse_args()

    try:
        setup_runner = CobaltCheckoutSetup(args)
        setup_runner.initialize()
    except Exception as error:  # pylint: disable=broad-exception-caught
        print(f"\n❌ Process terminated with an error: {error}",
              file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
