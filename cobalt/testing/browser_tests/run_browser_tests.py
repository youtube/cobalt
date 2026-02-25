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
import re
import shutil
import subprocess
import sys
import tempfile


def sanitize_filename(filename):
  """Sanitizes a filename by replacing invalid characters with underscores."""
  # Replace characters not allowed in filenames on most systems (e.g. NTFS).
  return re.sub(r"[\\/*?:\"<>|]", "_", filename)


def is_valid_test_name(name):
  """Checks if a string looks like a valid gtest case name."""
  # Gtest names usually contain alphanumerics and underscores.
  return bool(re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name))


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
    # Ensure standard flags for listing if needed by the binary
    list_cmd += ["--ozone-platform=starboard", "--no-sandbox"]
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
      potential_suite = line[:-1]
      if is_valid_test_name(potential_suite):
        current_suite = line
    else:
      if current_suite and is_valid_test_name(line):
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

  # Initialize the final XML file if requested
  final_xml_path = None
  if gtest_output and gtest_output.startswith("xml:"):
    final_xml_path = gtest_output[4:]
    # Ensure the directory exists
    os.makedirs(os.path.dirname(final_xml_path), exist_ok=True)
    with open(final_xml_path, "w", encoding="utf-8") as f:
      f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
      f.write('<testsuites tests="REPLACE_TESTS" failures="REPLACE_FAILURES" '
              'disabled="0" errors="0" time="0" name="AllTests">\n')

  # 2. Run each test in a fresh process
  failed_tests = []
  passed_count = 0

  for i, test in enumerate(tests):
    print(f"[{i+1}/{len(tests)}] Running {test}...")
    temp_user_data_dir = tempfile.mkdtemp(prefix="cobalt_browser_test_")
    test_xml_path = None
    try:
      current_test_args = list(new_runner_args)

      # Handle gtest_output if provided. We want each test to have its own XML.
      if final_xml_path:
        test_xml_path = os.path.join(temp_user_data_dir, "test_result.xml")
        current_test_args.append(f"--gtest_output=xml:{test_xml_path}")

      cmd = [
          binary_path,
          f"--gtest_filter={test}",
          "--single-process-tests",
          "--no-sandbox",
          "--single-process",
          "--no-zygote",
          "--ozone-platform=starboard",
          f"--user-data-dir={temp_user_data_dir}",
      ] + current_test_args

      retcode = subprocess.run(cmd, check=False).returncode

      if retcode != 0:
        print(f"FAILED: {test} (Exit code: {retcode})")
        failed_tests.append(test)
      else:
        passed_count += 1

      # Append results to the final XML
      if final_xml_path:
        if os.path.isfile(test_xml_path):
          with open(test_xml_path, "r", encoding="utf-8") as f:
            xml_content = f.read()
            # Extract the <testsuite> block(s)
            # We look for everything between <testsuites ...> and </testsuites>
            match = re.search(r"<testsuites[^>]*>(.*)</testsuites>",
                              xml_content, re.DOTALL)
            if match:
              with open(final_xml_path, "a", encoding="utf-8") as out_f:
                out_f.write(match.group(1))
        else:
          # Synthesize failure XML if file is missing
          suite_name, case_name = test.split(".", 1)
          with open(final_xml_path, "a", encoding="utf-8") as out_f:
            out_f.write(f'  <testsuite name="{suite_name}" tests="1" '
                        f'failures="1" errors="0" time="0">\n')
            out_f.write(f'    <testcase name="{case_name}" status="run" '
                        f'time="0" classname="{suite_name}">\n')
            out_f.write('      <failure message="Test crashed or failed to '
                        'output XML" type="Crash"/>\n')
            out_f.write("    </testcase>\n")
            out_f.write("  </testsuite>\n")

    except KeyboardInterrupt:
      print("\nAborted by user.")
      sys.exit(130)
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f"Error running {test}: {e}")
      failed_tests.append(test)
    finally:
      shutil.rmtree(temp_user_data_dir, ignore_errors=True)

  # Finalize the XML file
  if final_xml_path:
    with open(final_xml_path, "a", encoding="utf-8") as f:
      f.write("</testsuites>\n")

    # Update counts
    with open(final_xml_path, "r", encoding="utf-8") as f:
      content = f.read()
    content = content.replace("REPLACE_TESTS", str(len(tests)))
    content = content.replace("REPLACE_FAILURES", str(len(failed_tests)))
    with open(final_xml_path, "w", encoding="utf-8") as f:
      f.write(content)

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
