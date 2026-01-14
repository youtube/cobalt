#!/usr/bin/env python3
"""Helper script to run Cobalt browser tests.

Cobalt is designed to run in single process mode on TV devices. Its
browsertests is following it with --single-process-test as well.
To avoid regressions crashed the process, the test process management
is left in this python script, which list each single test case
and iterate them.
"""
import subprocess
import sys
import os


def main():
  if len(sys.argv) < 2:
    print("Usage: python3 run_browser_tests.py <path_to_executable> "
          "[gtest_filter]")
    sys.exit(1)

  binary_path = sys.argv[1]
  extra_filter = sys.argv[2] if len(sys.argv) > 2 else "*"

  if not os.path.isfile(binary_path):
    print(f"Error: Binary not found at {binary_path}", file=sys.stderr)
    sys.exit(1)

  # 1. List all tests
  print(f"Listing tests from {binary_path}...")
  try:
    output = subprocess.check_output(
        [binary_path, "--gtest_list_tests", f"--gtest_filter={extra_filter}"],
        text=True)
  except subprocess.CalledProcessError as e:
    print("Failed to list tests.", file=sys.stderr)
    sys.exit(e.returncode)

  tests = []
  current_suite = None
  for line in output.splitlines():
    # Clean up the line
    line = line.strip()

    # Skip empty lines or logs (logs usually start with [pid:...)
    if not line or line.startswith("["):
      continue

    # Remove comments (starting with #)
    line = line.split("#")[0].strip()

    if not line:
      continue

    if line.endswith("."):
      current_suite = line
    else:
      # It's a test name
      if current_suite:
        full_test_name = f"{current_suite}{line}"
        if "DISABLED_" not in full_test_name:
          tests.append(full_test_name)
      else:
        print(
            f"Warning: Found test '{line}' without a suite. Skipping.",
            file=sys.stderr)

  if not tests:
    print("No tests found matching the filter.")
    sys.exit(0)

  # Sort tests to ensure PRE_ tests run before their main tests.
  # Logic: PRE_PRE_Test -> PRE_Test -> Test
  def get_sort_key(test_name):
    # test_name is Suite.Test
    parts = test_name.split(".", 1)
    if len(parts) != 2:
      return (test_name, 0)
    suite, case = parts

    pre_count = 0
    while case.startswith("PRE_"):
      case = case[4:]
      pre_count += 1

    # Sort by base name (Suite.TestBase), then by pre_count DESCENDING
    return (f"{suite}.{case}", -pre_count)

  tests.sort(key=get_sort_key)

  print(f"Found {len(tests)} tests. Running them one by one...\n")

  # 2. Run each test in a fresh process
  failed_tests = []
  passed_count = 0

  for i, test in enumerate(tests):
    print(f"[{i+1}/{len(tests)}] Running {test}...")
    try:
      # We pass the filter to run EXACTLY this test case.
      # Using --gtest_filter=ExactTestName
      # Add standard Cobalt flags:
      #   --no-sandbox --single-process --no-zygote --ozone-platform=starboard
      cmd = [
          binary_path,
          f"--gtest_filter={test}",
          "--single-process-tests",
          "--no-sandbox",
          "--single-process",
          "--no-zygote",
          "--ozone-platform=starboard",
      ]

      # Run the test and let it print to stdout/stderr
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