#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Template for the portable test runner script.
"""
This script is the entrypoint for running packaged cobalt_browsertests.
It configures the environment and invokes the appropriate platform runner.
"""

import logging
import os
import shutil
import subprocess
import sys

# TARGET_MAP will be injected by the collection script.
TARGET_MAP = {}

logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] %(levelname)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S")


def main():
  """Main entrypoint for the portable test runner.

  This function performs the following steps:
  1. Sets up the environment (PATH, CHROME_SRC).
  2. Selects the target platform to run tests for.
  3. Resolves paths for runtime dependencies and test runners.
  4. Configures LD_LIBRARY_PATH for shared library resolution.
  5. Executes the platform-specific test runner (Android or Linux).

  Returns:
    The exit code of the test runner process.
  """
  script_dir = os.path.dirname(os.path.abspath(__file__))

  # 1. Setup Environment
  logging.info("Configuring environment...")
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
      logging.error("Multiple targets available. Please specify one: %s",
                    available_targets)
      logging.info("Usage: python3 run_tests.py <target_name> [args...]")
      sys.exit(1)

  if target_name not in TARGET_MAP:
    logging.error("Target '%s' not found. Available: %s", target_name,
                  available_targets)
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
    logging.error("runtime_deps file not found at %s", deps_path)
    sys.exit(1)

  if not os.path.isfile(test_runner):
    logging.error("test runner not found at %s", test_runner)
    sys.exit(1)

  # Add build root and starboard dir to LD_LIBRARY_PATH so shared libraries can
  # be found
  os.environ["LD_LIBRARY_PATH"] = (
      target_build_root_abs + os.pathsep + starboard_dir + os.pathsep +
      os.environ.get("LD_LIBRARY_PATH", ""))
  logging.info("LD_LIBRARY_PATH set to: %s", os.environ.get("LD_LIBRARY_PATH"))

  # 5. Sanity Checks
  logging.info("Checking for vpython3...")
  vpython_path = shutil.which("vpython3")
  if not vpython_path:
    logging.error("vpython3 not found in bundled depot_tools.")
    sys.exit(1)
  logging.info("Using vpython3 at: %s", vpython_path)

  # 6. Execute
  if is_android:
    logging.info("Executing Android test runner for '%s': %s", target_name,
                 test_runner)
    cmd = [vpython_path, test_runner, "--runtime-deps-path", deps_path
          ] + sys.argv[1:]
  else:
    logging.info(
        "Executing Linux test runner for '%s' using xvfb.py and "
        "run_browser_tests.py", target_name)
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
