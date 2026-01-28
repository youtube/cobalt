#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
"""Helper script to run Cobalt browser tests.

Cobalt is designed to run in single process mode on TV devices. Its
browsertests is following it with --single-process-test as well.
To avoid regressions crashed the process, the test process management
is left in this python script, which list each single test case
and iterate them.
"""

import argparse
import os
import re
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(
      description="Helper to run Cobalt browser tests.")
  parser.add_argument(
      "binary", help="Path to the test binary or runner script.")
  parser.add_argument(
      "filter",
      nargs="?",
      default="*",
      help="Gtest filter as positional argument.")
  parser.add_argument("-f", "--gtest_filter", help="Gtest filter as flag.")
  parser.add_argument("--gtest_output", help="Gtest output (e.g., xml:path).")
  parser.add_argument(
      "--test-launcher-bot-mode",
      action="store_true",
      help="Ignored, for compatibility.")

  # We might get passed extra gtest arguments.
  args, unknown = parser.parse_known_args()

  binary_path = args.binary

  # Determine the filter:
  # 1. Check if --gtest_filter was passed as a flag
  # 2. Check if it was passed in unknown args (e.g. --gtest_filter=...)
  # 3. Check if it was passed as the second positional argument
  # 4. Default to "*"
  extra_filter = args.gtest_filter
  if not extra_filter:
    for u in unknown:
      if u.startswith("--gtest_filter="):
        extra_filter = u.split("=", 1)[1]
        break
  if not extra_filter:
    extra_filter = args.filter

  # Filter out --gtest_filter from unknown to avoid duplication
  unknown = [u for u in unknown if not u.startswith("--gtest_filter=")]

  if not os.path.isfile(binary_path):
    print(f"Error: Binary not found at {binary_path}", file=sys.stderr)
    sys.exit(1)

  # 1. List all tests
  print(f"Listing tests from {binary_path}...")
  try:
    # Use a set of standard flags for listing. Some Cobalt binaries require
    # these even just to list tests.
    list_cmd = [
        binary_path,
        "--gtest_list_tests",
        f"--gtest_filter={extra_filter}",
        "--no-sandbox",
        "--single-process",
        "--no-zygote",
        "--ozone-platform=starboard",
    ] + unknown
    output = subprocess.check_output(
        list_cmd, text=True, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    print(f"Failed to list tests. Output:\n{e.output}", file=sys.stderr)
    sys.exit(e.returncode)

  tests = []
  current_suite = None

  # Regex to match a valid GTest suite name: Alphanumeric, underscores,
  # slashes, or hyphens, ending with a dot.
  # Example: NavigationBrowserTest.
  # Example: MyParameterizedTest/0.
  suite_regex = re.compile(r"^([a-zA-Z0-9_/ \-]+)\.$")

  for line in output.splitlines():
    # Skip logs or other noise. X.Org and Cobalt logs often have prefixes.
    if any(
        line.startswith(p) for p in [
            "(II)", "(WW)", "(EE)", "(++)", "(==)", "(!!)", "(??)", "[",
            "Command:", "Openbox-Message", "Additional test environment"
        ]):
      continue

    stripped = line.strip()
    if not stripped:
      continue

    # Remove comments (starting with #)
    line_no_comment = line.split("#")[0].rstrip()
    stripped_no_comment = line_no_comment.strip()

    if not stripped_no_comment:
      continue

    # Gtest list-tests output:
    # SuiteName.
    #   TestName1
    #   TestName2

    # A suite name has no leading whitespace and matches the suite regex.
    suite_match = suite_regex.match(line_no_comment)
    if not line.startswith(" ") and suite_match:
      current_suite = suite_match.group(0)
      continue

    # A test name starts with a space and requires a current_suite
    if line.startswith(" ") and current_suite:
      # Test name is the first word
      test_name = stripped_no_comment.split()[0]
      if test_name and not test_name.startswith("DISABLED_"):
        tests.append(f"{current_suite}{test_name}")

  if not tests:
    print(f"No tests found matching filter: {extra_filter}")
    # Print the output to help debugging if this happens unexpectedly
    if output.strip():
      print("Output from binary during listing:")
      print(output)
    sys.exit(0)

  # Sort tests to ensure PRE_ tests run before their main tests.
  # Logic: PRE_PRE_Test -> PRE_Test -> Test
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

  # Ensure the output directory exists if gtest_output is requested.
  if args.gtest_output and args.gtest_output.startswith("xml:"):
    out_dir = os.path.dirname(args.gtest_output[4:])
    if out_dir:
      os.makedirs(out_dir, exist_ok=True)

  # 2. Run each test in a fresh process
  failed_tests = []
  passed_count = 0

  for i, test in enumerate(tests):
    print(f"[{i + 1}/{len(tests)}] Running {test}...")
    try:
      cmd = [
          binary_path,
          f"--gtest_filter={test}",
          "--single-process-tests",
          "--no-sandbox",
          "--single-process",
          "--no-zygote",
          "--ozone-platform=starboard",
      ] + unknown

      # Forward the output flag if provided, but adjust per test
      if args.gtest_output and args.gtest_output.startswith("xml:"):
        base_path = args.gtest_output[4:]
        test_xml = f"{base_path}_{test}.xml"
        cmd.append(f"--gtest_output=xml:{test_xml}")

      # Run the test
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

  if args.gtest_output:
    print(f"Results written to: {args.gtest_output}")

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
