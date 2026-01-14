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
    print("Usage: python3 run_browser_tests.py <path_to_executable> [args...]")
    print("  Supported args:")
    print("    --gtest_filter=<pattern>       Run specific tests")
    print("    --gtest_total_shards=<N>       Total number of shards")
    print("    --gtest_shard_index=<I>        Shard index (0 to N-1)")
    print("    --test-launcher-total-shards=<N>")
    print("    --test-launcher-shard-index=<I>")
    print("    Other flags are passed to the test binary.")
    sys.exit(1)

  binary_path = sys.argv[1]
  raw_args = sys.argv[2:]

  # Default sharding parameters
  total_shards = 1
  shard_index = 0

  # Check environment variables first (can be overridden by args)
  if "GTEST_TOTAL_SHARDS" in os.environ:
    try:
      total_shards = int(os.environ["GTEST_TOTAL_SHARDS"])
    except ValueError:
      pass

  if "GTEST_SHARD_INDEX" in os.environ:
    try:
      shard_index = int(os.environ["GTEST_SHARD_INDEX"])
    except ValueError:
      pass

  binary_args = []

  # Parse arguments
  for arg in raw_args:
    if arg.startswith("--gtest_total_shards="):
      total_shards = int(arg.split("=")[1])
    elif arg.startswith("--gtest_shard_index="):
      shard_index = int(arg.split("=")[1])
    elif arg.startswith("--test-launcher-total-shards="):
      total_shards = int(arg.split("=")[1])
    elif arg.startswith("--test-launcher-shard-index="):
      shard_index = int(arg.split("=")[1])
    else:
      binary_args.append(arg)

  if shard_index < 0 or shard_index >= total_shards:
    print(
        f"Error: Invalid shard index {shard_index} for total shards "
        f"{total_shards}",
        file=sys.stderr)
    sys.exit(1)

  if not os.path.isfile(binary_path):
    print(f"Error: Binary not found at {binary_path}", file=sys.stderr)
    sys.exit(1)

  # 1. List all tests
  print(f"Listing tests from {binary_path}...")
  try:
    # Pass arguments to list_tests so filters apply during listing
    cmd = [binary_path, "--gtest_list_tests"] + binary_args
    output = subprocess.check_output(cmd, text=True)
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

  # Sort tests to ensure deterministic order for sharding
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

  # 2. Apply Sharding
  sharded_tests = []
  for i, test in enumerate(tests):
    if i % total_shards == shard_index:
      sharded_tests.append(test)

  print(f"Found {len(tests)} tests. Running {len(sharded_tests)} tests "
        f"for shard {shard_index}/{total_shards}...\n")

  # 3. Run filtered tests
  failed_tests = []
  passed_count = 0

  for i, test in enumerate(sharded_tests):
    print(f"[{i+1}/{len(sharded_tests)}] Running {test}...")
    try:
      # Construct command for single test execution
      cmd = [
          binary_path,
          f"--gtest_filter={test}",
          "--single-process-tests",
          "--no-sandbox",
          "--single-process",
          "--no-zygote",
          "--ozone-platform=starboard",
      ]

      # Append other user args (excluding gtest_filter which we handle)
      cmd.extend(
          [arg for arg in binary_args if not arg.startswith("--gtest_filter=")])

      retcode = subprocess.run(cmd, check=False).returncode

      if retcode != 0:
        print(f"FAILED: {test} (Exit code: {retcode})")
        failed_tests.append(test)
      else:
        passed_count += 1

    except KeyboardInterrupt:
      print("\nAborted by user.")
      sys.exit(130)
    # pylint: disable=broad-exception-caught
    except Exception as e:
      print(f"Error running {test}: {e}")
      failed_tests.append(test)

  print("\n" + "=" * 40)
  print(f"Shard {shard_index}/{total_shards} Finished.")
  print(f"Total: {len(sharded_tests)}, Passed: {passed_count}, "
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
