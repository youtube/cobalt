#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Template for the portable test runner script.
"""
This script is the entrypoint for running packaged cobalt_browsertests.
It configures the environment and invokes the appropriate platform runner.
"""

import argparse
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
  1. Sets up the environment (PATH, CHROME_SRC, PYTHONPATH).
  2. Resolves paths for runtime dependencies and test runners.
  3. Configures LD_LIBRARY_PATH for shared library resolution.
  4. Executes the platform-specific test runner (Android or Linux).
  """
  script_dir = os.path.dirname(os.path.abspath(__file__))

  # 1. Setup Environment
  logging.info("Configuring environment...")

  # Debugging: List all files in the archive to find where test_env.py went.
  try:
    logging.info("Current Working Directory: %s", os.getcwd())
    logging.info("Archive root (%s) contents: %s", script_dir,
                 os.listdir(script_dir))

    found_test_env = []
    all_files = []
    for root, _, files in os.walk(script_dir):
      for f in files:
        rel_f = os.path.join(os.path.relpath(root, script_dir), f)
        all_files.append(rel_f)
        if f == "test_env.py":
          found_test_env.append(os.path.join(root, f))

    if found_test_env:
      logging.info("Located test_env.py at: %s", found_test_env)
    else:
      logging.warning("test_env.py NOT FOUND anywhere under %s", script_dir)
      # Print first 50 files to see structure
      logging.info("Partial file list: %s", all_files[:50])
  except OSError as e:
    logging.info("Debug listing failed: %s", e)

  # Determine src_dir. It's usually script_dir if bundled at root,
  # or script_dir/src if nested.
  src_dir = os.path.join(script_dir, "src")
  if not os.path.isdir(src_dir):
    src_dir = script_dir

  # Find depot_tools.
  # It could be in several locations depending on how the archive was structured.  # pylint: disable=line-too-long
  # Reference archive has it at the root.
  potential_paths = [
      os.path.join(script_dir, "depot_tools"),
      os.path.join(script_dir, "third_party", "depot_tools"),
      os.path.join(src_dir, "third_party", "depot_tools"),
      os.path.join(src_dir, "depot_tools"),
  ]

  found_depot_tools = False
  for p in potential_paths:
    if os.path.isdir(p):
      logging.info("Found bundled depot_tools at %s", p)
      # Some environments might have vpython3 in a subfolder or as a different
      # name. We add the directory itself to PATH.
      os.environ["PATH"] = p + os.pathsep + os.environ.get("PATH", "")

      # Also check for .cipd_bin if it exists (some vpython3 setups use it)
      cipd_bin = os.path.join(p, ".cipd_bin")
      if os.path.isdir(cipd_bin):
        os.environ["PATH"] = cipd_bin + os.pathsep + os.environ.get("PATH", "")

      found_depot_tools = True
      break

  if not found_depot_tools:
    logging.warning("No bundled depot_tools found in: %s", potential_paths)
    # List files in script_dir and third_party to help debug
    try:
      tp_dir = os.path.join(script_dir, "third_party")
      if os.path.isdir(tp_dir):
        tp_contents = os.listdir(tp_dir)
        logging.info("third_party contents: %s", tp_contents)
        if "depot_tools" in tp_contents:
          dt_dir = os.path.join(tp_dir, "depot_tools")
          logging.info("depot_tools contents (partial): %s",
                       os.listdir(dt_dir)[:20])
      elif os.path.isdir(os.path.join(src_dir, "third_party")):
        tp_dir = os.path.join(src_dir, "third_party")
        tp_contents = os.listdir(tp_dir)
        logging.info("src/third_party contents: %s", tp_contents)
        if "depot_tools" in tp_contents:
          dt_dir = os.path.join(tp_dir, "depot_tools")
          logging.info("src/third_party/depot_tools contents (partial): %s",
                       os.listdir(dt_dir)[:20])
    except OSError as e:
      logging.info("Could not list directories: %s", e)

  # Check for critical test files
  critical_files = ["testing/test_env.py", "build/android/test_runner.py"]
  for f in critical_files:
    full_f = os.path.join(src_dir, f)
    if not os.path.isfile(full_f):
      logging.warning("Critical file MISSING: %s (expected at %s)", f, full_f)
    else:
      logging.info("Verified critical file exists: %s", f)

  os.environ["DEPOT_TOOLS_UPDATE"] = "0"
  os.environ["CHROME_SRC"] = src_dir
  os.environ["PYTHONPATH"] = (
      os.path.join(src_dir, "tools", "python") + os.pathsep +
      os.environ.get("PYTHONPATH", ""))

  # 2. Argument Parsing and Target Selection
  available_targets = list(TARGET_MAP.keys())

  parser = argparse.ArgumentParser(
      description="Portable test runner for Cobalt browser tests.",
      add_help=False)
  parser.add_argument("--init-command", help="Command to run before tests.")
  parser.add_argument(
      "target", nargs="?", help="Target platform to run tests for.")
  parser.add_argument(
      "-h", "--help", action="store_true", help="Show this help message.")

  args, runner_args = parser.parse_known_args()

  if args.help:
    parser.print_help()
    print("\nAvailable targets: " + ", ".join(available_targets))
    print("\nAll other arguments are passed to the underlying test runner.")
    sys.exit(0)

  if args.init_command:
    logging.info("Executing init-command: %s", args.init_command)
    subprocess.run(args.init_command, shell=True, check=True, cwd=src_dir)

  target_name = os.environ.get("TARGET_PLATFORM") or args.target

  if not target_name:
    if len(available_targets) == 1:
      target_name = available_targets[0]
    else:
      logging.error("Multiple targets available. Please specify one: %s",
                    available_targets)
      parser.print_usage()
      sys.exit(1)

  if target_name not in TARGET_MAP:
    # If the provided target is not in the map, it might be an argument
    # for the runner if only one target exists.
    if len(available_targets) == 1:
      if target_name:
        runner_args.insert(0, target_name)
      target_name = available_targets[0]
    else:
      logging.error("Target '%s' not found. Available: %s", target_name,
                    available_targets)
      sys.exit(1)

  target_config = TARGET_MAP[target_name]
  is_android = target_config.get("is_android", False)

  # 3. Resolve Paths
  deps_path = os.path.join(src_dir, target_config["deps"])
  test_runner = os.path.join(src_dir, target_config["runner"])

  # Resolve the specific build root for this target.
  target_build_root_abs = os.path.join(src_dir, target_config["build_dir"])
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
    logging.error("vpython3 not found in PATH or bundled depot_tools.")
    sys.exit(1)
  logging.info("Using vpython3 at: %s", vpython_path)

  # Resolve vpython to absolute path before changing CWD
  vpython_path = os.path.abspath(vpython_path)

  # 6. Execute
  if is_android:
    logging.info("Executing Android test runner for '%s' in %s: %s",
                 target_name, src_dir, test_runner)
    cmd = [vpython_path, test_runner, "--runtime-deps-path", deps_path
          ] + runner_args
  else:
    logging.info(
        "Executing Linux test runner for '%s' in %s using xvfb.py and "
        "run_browser_tests.py", target_name, src_dir)
    xvfb_py = os.path.join(src_dir, "testing/xvfb.py")
    run_browser_tests_py = os.path.join(
        src_dir, "cobalt/testing/browser_tests/run_browser_tests.py")
    # Wrap the whole command in xvfb.py to provide a DISPLAY
    cmd = [
        vpython_path, xvfb_py, sys.executable, run_browser_tests_py, test_runner
    ] + runner_args

  try:
    return subprocess.call(cmd, cwd=src_dir)
  except KeyboardInterrupt:
    return 1


if __name__ == "__main__":
  sys.exit(main())
