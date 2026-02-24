#!/usr/bin/env python3
"""Helper script to run Cobalt browser tests.

Cobalt is designed to run in single process mode on TV devices. Its
browsertests is following it with --single-process-test as well.
To avoid regressions crashed the process, the test process management
is left in this python script, which list each single test case
and iterate them.
"""
import argparse
import os
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(
      description="Run Cobalt browser tests one by one.", add_help=False)
  parser.add_argument(
      "binary", help="Path to the cobalt_browsertests executable.")
  parser.add_argument(
      "-h", "--help", action="store_true", help="Show this help message.")

  args, runner_args = parser.parse_known_args()

  if args.help:
    parser.print_help()
    print("\nAll other arguments are passed to the underlying test binary.")
    sys.exit(0)

  binary_path = args.binary
  if not os.path.isfile(binary_path):
    print(f"Error: Binary not found at {binary_path}", file=sys.stderr)
    sys.exit(1)

  gtest_filter = "*"
  gtest_output = None
  new_runner_args = []

  i = 0
  while i < len(runner_args):
    arg = runner_args[i]
    if arg.startswith("--gtest_filter="):
      gtest_filter = arg.split("=", 1)[1]
    elif arg == "--gtest_filter" and i + 1 < len(runner_args):
      gtest_filter = runner_args[i + 1]
      i += 1
    elif arg.startswith("--gtest_output="):
      gtest_output = arg.split("=", 1)[1]
    elif arg == "--gtest_output" and i + 1 < len(runner_args):
      gtest_output = runner_args[i + 1]
      i += 1
    else:
      new_runner_args.append(arg)
    i += 1

  # 1. List all tests matching the filter
  print(f"Listing tests from {binary_path} with filter: {gtest_filter}")
  try:
    list_cmd = [
        binary_path, "--gtest_list_tests", f"--gtest_filter={gtest_filter}"
    ]
    output = subprocess.check_output(list_cmd, text=True)
  except subprocess.CalledProcessError as e:
    print("Failed to list tests.", file=sys.stderr)
    sys.exit(e.returncode)

  tests = []
  current_suite = None
  for line in output.splitlines():
    line = line.strip()
    if not line or line.startswith("["):
      continue
    line = line.split("#")[0].strip()
    if not line:
      continue

    if line.endswith("."):
      current_suite = line
    else:
      if current_suite:
        full_test_name = f"{current_suite}{line}"
        if "DISABLED_" not in full_test_name:
          tests.append(full_test_name)

  if not tests:
    print("No tests found matching the filter.")
    sys.exit(0)

  # Sort tests to ensure PRE_ tests run before their main tests.
  def get_sort_key(test_name):
    parts = test_name.split(".", 1)
    if len(parts) != 2:
      return (test_name, 0)
    suite, case = parts
    pre_count = 0
    while case.startswith("PRE_"):
      case = case[4:]
      pre_count += 1
    return (f"{suite}.{case}", -pre_count)

  tests.sort(key=get_sort_key)

  print(f"Found {len(tests)} tests. Running them one by one...\n")

  # 2. Run each test in a fresh process
  failed_tests = []
  passed_count = 0

  for i, test in enumerate(tests):
    print(f"[{i+1}/{len(tests)}] Running {test}...")
    try:
      current_test_args = list(new_runner_args)

      # Handle gtest_output if provided. We want each test to have its own XML.
      if gtest_output:
        if gtest_output.startswith("xml:"):
          base_xml = gtest_output[4:]
          if base_xml.endswith(".xml"):
            test_xml = f"{base_xml[:-4]}_{test}.xml"
          else:
            test_xml = f"{base_xml}_{test}.xml"
          current_test_args.append(f"--gtest_output=xml:{test_xml}")
        else:
          # Fallback if it's just a path
          current_test_args.append(f"--gtest_output={gtest_output}_{test}")

      cmd = [
          binary_path,
          f"--gtest_filter={test}",
          "--single-process-tests",
          "--no-sandbox",
          "--single-process",
          "--no-zygote",
          "--ozone-platform=starboard",
      ] + current_test_args

      retcode = subprocess.run(cmd, check=False).returncode

      if retcode != 0:
        print(f"FAILED: {test} (Exit code: {retcode})")
        failed_tests.append(test)
      else:
        passed_count += 1

    except KeyboardInterrupt:
      print("\nAborted by user.")
      sys.exit(130)
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f"Error running {test}: {e}")
      failed_tests.append(test)

  print("\n" + "=" * 40)
  print(f"Total: {len(tests)}, Passed: {passed_count}, "
        f"Failed: {len(failed_tests)}")

  if failed_tests:
    print("\nFailed Tests:")
    for t in failed_tests:
      print(f"  {t}")
    sys.exit(1)
  else:
    print("\nAll tests PASSED.")
    sys.exit(0)


if __name__ == "__main__":
  main()
