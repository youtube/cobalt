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
import xml.etree.ElementTree as ET


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      "binary",
      nargs="?",
      default="./cobalt_browsertests",
      help="Path to the test binary")
  parser.add_argument("--gtest_filter", help="GTest filter pattern")
  parser.add_argument("--gtest_output", help="GTest output (xml:path)")
  parser.add_argument("--gtest_total_shards", type=int, default=0)
  parser.add_argument("--gtest_shard_index", type=int, default=0)
  parser.add_argument(
      "--test-launcher-total-shards",
      dest="tl_total_shards",
      type=int,
      default=0)
  parser.add_argument(
      "--test-launcher-shard-index", dest="tl_shard_index", type=int, default=0)

  args, unknown_args = parser.parse_known_args()

  if not os.path.isfile(args.binary):
    # Try finding it in the same directory as the script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    potential_binary = os.path.join(script_dir, args.binary)
    if os.path.isfile(potential_binary):
      args.binary = potential_binary
    else:
      # If binary is just a filename, try looking in current dir
      if os.path.isfile(os.path.join(os.getcwd(), args.binary)):
        args.binary = os.path.join(os.getcwd(), args.binary)
      else:
        print(f"Error: Binary not found at {args.binary}", file=sys.stderr)
        sys.exit(1)

  # Determine Sharding
  total_shards = args.gtest_total_shards or args.tl_total_shards or 1
  shard_index = args.gtest_shard_index or args.tl_shard_index or 0

  # Fallback to Env Vars
  if total_shards == 1 and "GTEST_TOTAL_SHARDS" in os.environ:
    total_shards = int(os.environ["GTEST_TOTAL_SHARDS"])
  if shard_index == 0 and "GTEST_SHARD_INDEX" in os.environ:
    shard_index = int(os.environ["GTEST_SHARD_INDEX"])

  # 1. List Tests
  print(f"Listing tests from {args.binary}...")
  list_cmd = [args.binary, "--gtest_list_tests"]
  if args.gtest_filter:
    list_cmd.append(f"--gtest_filter={args.gtest_filter}")

  # Pass other unknown args to listing too? Usually safe.
  list_cmd.extend(unknown_args)

  try:
    output = subprocess.check_output(list_cmd, text=True)
  except subprocess.CalledProcessError as e:
    print("Failed to list tests.", file=sys.stderr)
    sys.exit(e.returncode)

  tests = []
  current_suite = None
  for line in output.splitlines():
    line = line.strip()
    if not line or line.startswith("["):
      continue  # Skip logs

    # Remove comments
    line = line.split("#")[0].strip()
    if not line:
      continue

    if line.endswith("."):
      current_suite = line
    else:
      if current_suite:
        test_name = f"{current_suite}{line}"
        if "DISABLED_" not in test_name:
          tests.append(test_name)

  # Deterministic Sort
  def get_sort_key(name):
    return name

  tests.sort(key=get_sort_key)

  # 2. Filter Shard
  tests_to_run = []
  for i, test in enumerate(tests):
    if i % total_shards == shard_index:
      tests_to_run.append(test)

  print(
      f"Shard {shard_index}/{total_shards}: Running {len(tests_to_run)} tests.")

  # XML Setup
  xml_output_file = None
  if args.gtest_output and args.gtest_output.startswith("xml:"):
    xml_output_file = args.gtest_output[4:]
    # Initialize XML
    root = ET.Element(
        "testsuites",
        tests="0",
        failures="0",
        disabled="0",
        errors="0",
        time="0",
        name="AllTests")
    tree = ET.ElementTree(root)
    tree.write(xml_output_file, encoding="UTF-8", xml_declaration=True)

  passed_count = 0
  failed_count = 0

  # 3. Execution Loop
  for i, test in enumerate(tests_to_run):
    print(f"[RUN] {test}")

    cmd = [
        args.binary, f"--gtest_filter={test}", "--single-process-tests",
        "--no-sandbox", "--single-process", "--no-zygote",
        "--ozone-platform=starboard"
    ]

    temp_xml = None
    if xml_output_file:
      temp_xml = f"temp_shard_{shard_index}_{i}.xml"
      cmd.append(f"--gtest_output=xml:{temp_xml}")

    # Add unknown args (but NOT sharding args, we handle them)
    cmd.extend(unknown_args)

    # Force disable sharding for the binary execution
    env = os.environ.copy()
    env["GTEST_TOTAL_SHARDS"] = "1"
    env["GTEST_SHARD_INDEX"] = "0"

    try:
      retcode = subprocess.call(cmd, env=env)
    # pylint: disable=broad-exception-caught
    except Exception as e:
      print(f"Error executing test: {e}")
      retcode = 1

    if retcode == 0:
      passed_count += 1
    else:
      print(f"[FAILED] {test} (Exit Code: {retcode})")
      failed_count += 1

    # 4. Merge XML
    if xml_output_file:
      try:
        main_tree = ET.parse(xml_output_file)
        main_root = main_tree.getroot()

        if temp_xml and os.path.exists(temp_xml):
          shard_tree = ET.parse(temp_xml)
          shard_root = shard_tree.getroot()
          for testsuite in shard_root:
            main_root.append(testsuite)
          os.remove(temp_xml)
        else:
          # Synthesize Crash Entry
          parts = test.split(".", 1)
          suite_name = parts[0]
          case_name = parts[1] if len(parts) > 1 else test

          ts = ET.SubElement(
              main_root,
              "testsuite",
              name=suite_name,
              tests="1",
              failures="1",
              errors="0",
              time="0")
          tc = ET.SubElement(
              ts,
              "testcase",
              name=case_name,
              status="run",
              time="0",
              classname=suite_name)
          ET.SubElement(
              tc,
              "failure",
              message="Test crashed or failed to output XML",
              type="Crash")

        # Update Counts
        main_root.set("tests", str(passed_count + failed_count))
        main_root.set("failures", str(failed_count))
        main_tree.write(xml_output_file, encoding="UTF-8", xml_declaration=True)
      # pylint: disable=broad-exception-caught
      except Exception as e:
        print(f"Error merging XML: {e}")

  print("=" * 40)
  print(f"Total: {passed_count + failed_count}, "
        f"Passed: {passed_count}, Failed: {failed_count}")

  if failed_count != 0:
    sys.exit(1)
  sys.exit(0)


if __name__ == "__main__":
  main()
