#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Template for the portable test runner script.
"""
This script is the entrypoint for running packaged cobalt_browsertests.
It configures the environment and invokes the appropriate platform runner.
"""

import datetime
import os
import shutil
import subprocess
import sys

# TARGET_MAP will be injected by the collection script.
TARGET_MAP = {}


def log(msg):
  timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
  print(f"[{timestamp}] {msg}")


def main():
  script_dir = os.path.dirname(os.path.abspath(__file__))

  # 1. Setup Environment
  log("Configuring environment...")
  depot_tools_path = os.path.join(script_dir, "depot_tools")
  os.environ["PATH"] = depot_tools_path + os.pathsep + os.environ.get(
      "PATH", "")
  os.environ["DEPOT_TOOLS_UPDATE"] = "0"

  src_dir = os.path.join(script_dir, "src")
  os.environ["CHROME_SRC"] = src_dir

  # 2. Target Selection
  available_targets = list(TARGET_MAP.keys())

  target_name = os.environ.get("TARGET_PLATFORM")
  if not target_name and len(sys.argv) > 1 and sys.argv[1] in available_targets:
    target_name = sys.argv.pop(1)

  if not target_name:
    if len(available_targets) == 1:
      target_name = available_targets[0]
    else:
      print("Error: Multiple targets available. "
            f"Please specify one: {available_targets}")
      print("Usage: python3 run_tests.py <target_name> [args...]")
      sys.exit(1)

  if target_name not in TARGET_MAP:
    print(f"Error: Target '{target_name}' not found. "
          f"Available: {available_targets}")
    sys.exit(1)

  target_config = TARGET_MAP[target_name]
  is_android = target_config.get("is_android", False)

  # 3. Resolve Paths
  deps_path = os.path.join(src_dir, target_config["deps"])
  test_runner = os.path.join(src_dir, target_config["runner"])

  # Resolve the specific build root for this target (one level above 'out/')
  target_build_root = os.path.dirname(target_config["runner"])
  while target_build_root:
    parent = os.path.dirname(target_build_root)
    if os.path.basename(parent) == "out" or parent == target_build_root:
      break
    target_build_root = parent

  target_build_root_abs = os.path.join(src_dir, target_build_root)
  starboard_dir = os.path.join(target_build_root_abs, "starboard")

  if not os.path.isfile(deps_path):
    log(f"Error: runtime_deps file not found at {deps_path}")
    sys.exit(1)

  if not os.path.isfile(test_runner):
    log(f"Error: test runner not found at {test_runner}")
    sys.exit(1)

  # Add build root and starboard dir to LD_LIBRARY_PATH so shared libraries can
  # be found
  os.environ["LD_LIBRARY_PATH"] = (
      target_build_root_abs + os.pathsep + starboard_dir + os.pathsep +
      os.environ.get("LD_LIBRARY_PATH", ""))
  log(f"LD_LIBRARY_PATH set to: {os.environ.get('LD_LIBRARY_PATH')}")  # pylint: disable=inconsistent-quotes

  # 5. Sanity Checks
  log("Checking for vpython3...")
  vpython_path = shutil.which("vpython3")
  if not vpython_path:
    log("Error: vpython3 not found in bundled depot_tools.")
    sys.exit(1)
  log(f"Using vpython3 at: {vpython_path}")

  # 6. Execute
  if is_android:
    log(f"Executing Android test runner for '{target_name}': "
        f"{test_runner}")
    cmd = [vpython_path, test_runner, "--runtime-deps-path", deps_path
          ] + sys.argv[1:]
  else:
    log(f"Executing Linux test runner for '{target_name}' "
        "using xvfb.py and run_browser_tests.py")
    xvfb_py = os.path.join(src_dir, "testing/xvfb.py")
    run_browser_tests_py = os.path.join(
        src_dir, "cobalt/testing/browser_tests/run_browser_tests.py")
    # Wrap the whole command in xvfb.py to provide a DISPLAY
    cmd = [
        vpython_path, xvfb_py, sys.executable, run_browser_tests_py, test_runner
    ] + sys.argv[1:]

  try:
    return subprocess.call(cmd)
  except KeyboardInterrupt:
    return 1


if __name__ == "__main__":
  sys.exit(main())
