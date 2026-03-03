#!/usr/bin/env python3
"""Helper script to run Cobalt browser tests.

Cobalt is designed to run in single process mode on TV devices. Its
browsertests is following it with --single-process-test as well.
To avoid regressions crashed the process, the test process management
is left in this python script, which list each single test case
and iterate them.
"""

import argparse
import logging
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from typing import List, Tuple, Optional


class CobaltTestRunner:
  """Manages listing, sharding, executing, and reporting browser tests."""

  def __init__(self, args: argparse.Namespace, unknown_args: List[str]):
    """Initializes the test runner.

        Args:
            args: Parsed known arguments.
            unknown_args: List of unknown arguments to pass to the binary.
        """
    self.args = args
    self.unknown_args = unknown_args
    self.binary = self._find_binary(args.binary)
    self._setup_sharding()
    self.xml_output_file = self._get_xml_output_file()
    self.user_data_dir = args.user_data_dir or os.environ.get(
        "TEST_USER_DATA_DIR")
    self._temp_dir_obj = None
    self._xml_root = None
    self._xml_tree = None

    # Diable pylint because if the folder is passed by users, we should
    # keep it.
    # pylint: disable=consider-using-with
    if not self.user_data_dir:
      self._temp_dir_obj = tempfile.TemporaryDirectory(
          prefix="cobalt_user_data_")
      self.user_data_dir = self._temp_dir_obj.name
      logging.info("Using temporary user data directory: %s",
                   self.user_data_dir)
    else:
      logging.info("Using user data directory: %s", self.user_data_dir)

  def __enter__(self):
    """Enter the runtime context."""
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    """Exit the runtime context and cleanup resources."""
    self.cleanup()

  def cleanup(self):
    """Cleans up resources, like the temporary directory."""
    if self._temp_dir_obj:
      try:
        self._temp_dir_obj.cleanup()
        logging.info("Cleaned up temporary user data directory: %s",
                     self.user_data_dir)
      # pylint: disable=broad-exception-caught
      except Exception as e:
        logging.error("Error cleaning up temporary directory %s: %s",
                      self.user_data_dir, e)
      self._temp_dir_obj = None
    self._xml_root = None
    self._xml_tree = None

  def _find_binary(self, binary_path: str) -> str:
    """Locates the executable test binary.

        Args:
            binary_path: The initial path to the binary.

        Returns:
            The absolute path to the found binary.

        Raises:
            SystemExit: If the binary cannot be found.
        """
    if os.path.isfile(binary_path):
      return os.path.abspath(binary_path)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    potential_binary = os.path.join(script_dir, binary_path)
    if os.path.isfile(potential_binary):
      return potential_binary

    potential_binary_cwd = os.path.join(os.getcwd(), binary_path)
    if os.path.isfile(potential_binary_cwd):
      return potential_binary_cwd

    logging.error("Binary not found at %s", binary_path)
    sys.exit(1)

  def _setup_sharding(self):
    """Set sharding from arguments and environment variables."""
    self.total_shards = (
        self.args.gtest_total_shards or self.args.tl_total_shards or 1)
    self.shard_index = (
        self.args.gtest_shard_index or self.args.tl_shard_index or 0)

    if self.total_shards == 1 and "GTEST_TOTAL_SHARDS" in os.environ:
      try:
        self.total_shards = int(os.environ["GTEST_TOTAL_SHARDS"])
      except ValueError:
        logging.warning("Non-integer GTEST_TOTAL_SHARDS env var.")
        self.total_shards = 1
    if self.shard_index == 0 and "GTEST_SHARD_INDEX" in os.environ:
      try:
        self.shard_index = int(os.environ["GTEST_SHARD_INDEX"])
      except ValueError:
        logging.warning("Non-integer GTEST_SHARD_INDEX env var.")
        self.shard_index = 0

    if not 0 <= self.shard_index < self.total_shards:
      logging.error("Shard index %d is out of bounds for %d shards. Quit.",
                    self.shard_index, self.total_shards)
      sys.exit(1)

  def _get_xml_output_file(self) -> Optional[str]:
    """Extracts the XML output file path from arguments."""
    if self.args.gtest_output and self.args.gtest_output.startswith("xml:"):
      return self.args.gtest_output[4:]
    return None

  def list_tests(self) -> str:
    """Lists tests from the binary using --gtest_list_tests.

        Returns:
            The standard output from the test listing command.

        Raises:
            SystemExit: If the list command fails.
        """
    logging.info("Listing tests from %s...", self.binary)
    list_cmd = [self.binary, "--gtest_list_tests"]
    if self.args.gtest_filter:
      list_cmd.append(f"--gtest_filter={self.args.gtest_filter}")
    list_cmd.append(f"--user-data-dir={self.user_data_dir}")
    list_cmd.extend(self.unknown_args)

    try:
      output = subprocess.check_output(list_cmd, text=True)
      return output
    except subprocess.CalledProcessError as e:
      logging.error("Failed to list tests: %s", e.output)
      sys.exit(e.returncode)
    except FileNotFoundError:
      logging.error("Failed to list tests: Binary not found at %s", self.binary)
      sys.exit(1)

  def _get_sort_key(self, test_name: str) -> Tuple[str, int]:
    """Generates a key for sorting tests, prioritizing PRE_ tests."""
    parts = test_name.split(".", 1)
    if len(parts) != 2:
      return (test_name, 0)
    suite, case = parts

    pre_count = 0
    while case.startswith("PRE_"):
      case = case[4:]
      pre_count += 1
    return (f"{suite}.{case}", -pre_count)

  def parse_and_sort_tests(self, gtest_list_output: str) -> List[str]:
    """Parses the output of --gtest_list_tests and sorts test names.

        This method emulates the behavior of the provided awk script for
        extracting test names from the gtest output.
        """
    tests = []
    current_suite = None
    for line in gtest_list_output.splitlines():
      line = line.strip()
      if not line or line.startswith("["):
        continue

      # Remove comments
      line = line.split("#")[0].strip()

      # GTest output format:
      # SuiteName.
      #   TestName1
      #   TestName2
      if not line.startswith(" ") and line.endswith("."):
        current_suite = line
      elif line and current_suite:
        test_name = f"{current_suite}{line}"
        if "DISABLED_" not in test_name:
          tests.append(test_name)
      # Ignore other lines

    tests.sort(key=self._get_sort_key)
    return tests

  def filter_tests_for_shard(self, tests: List[str]) -> List[str]:
    """Filters the list of tests based on the current shard index."""
    return [
        test for i, test in enumerate(tests)
        if i % self.total_shards == self.shard_index
    ]

  def _initialize_xml(self):
    """Initializes the in-memory XML tree."""
    if not self.xml_output_file:
      return
    try:
      self._xml_root = ET.Element(
          "testsuites",
          tests="0",
          failures="0",
          disabled="0",
          errors="0",
          time="0",
          name="AllTests")
      self._xml_tree = ET.ElementTree(self._xml_root)
      if os.path.dirname(self.xml_output_file):
        os.makedirs(os.path.dirname(self.xml_output_file), exist_ok=True)
    # pylint: disable=broad-exception-caught
    except Exception as e:
      logging.error("Error initializing XML file %s: %s", self.xml_output_file,
                    e)

  def _run_single_test(self, test_name: str,
                       test_idx: int) -> Tuple[int, Optional[str]]:
    """Executes a single test case."""
    cmd = [
        self.binary,
        f"--gtest_filter={test_name}",
        "--single-process-tests",
        "--no-sandbox",
        "--single-process",
        "--no-zygote",
        "--ozone-platform=starboard",
        f"--user-data-dir={self.user_data_dir}",
    ]

    temp_xml_path = None
    if self.xml_output_file:
      temp_xml_path = f"temp_shard_{self.shard_index}_{test_idx}.xml"
      cmd.append(f"--gtest_output=xml:{temp_xml_path}")

    cmd.extend(self.unknown_args)

    env = os.environ.copy()
    env["GTEST_TOTAL_SHARDS"] = "1"
    env["GTEST_SHARD_INDEX"] = "0"

    try:
      retcode = subprocess.call(cmd, env=env)
    # pylint: disable=broad-exception-caught
    except Exception as e:
      logging.error("Error executing test %s: %s", test_name, e)
      retcode = 1

    return retcode, temp_xml_path

  def _create_crash_xml_entry(self, test_name: str) -> ET.Element:
    """Creates an XML testsuite entry for a crashed test."""
    parts = test_name.split(".", 1)
    suite_name = parts[0]
    case_name = parts[1] if len(parts) > 1 else test_name

    ts = ET.Element(
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
    return ts

  def _merge_test_xml(self, test_name: str, temp_xml_path: Optional[str]):
    """Merges the temporary test XML into the in-memory XML root."""
    if not self.xml_output_file or self._xml_root is None:
      return

    try:
      merged = False
      if temp_xml_path and os.path.exists(temp_xml_path):
        try:
          shard_tree = ET.parse(temp_xml_path)
          shard_root = shard_tree.getroot()
          for testsuite in shard_root:
            self._xml_root.append(testsuite)
          merged = True
        except ET.ParseError as pe:
          logging.warning("Failed to parse temp XML %s: %s", temp_xml_path, pe)
        finally:
          os.remove(temp_xml_path)

      if not merged:
        crash_entry = self._create_crash_xml_entry(test_name)
        self._xml_root.append(crash_entry)
    # pylint: disable=broad-exception-caught
    except Exception as e:
      logging.error("Error merging XML for %s: %s", test_name, e)

  def _finalize_and_write_xml(self, total_count: int, failed_count: int):
    """Finalizes summary counts and writes the in-memory XML tree to disk."""
    if (not self.xml_output_file or self._xml_root is None or
        self._xml_tree is None):
      return
    try:
      self._xml_root.set("tests", str(total_count))
      self._xml_root.set("failures", str(failed_count))
      self._xml_tree.write(
          self.xml_output_file, encoding="UTF-8", xml_declaration=True)
    # pylint: disable=broad-exception-caught
    except Exception as e:
      logging.error("Error writing final XML: %s", e)

  def run(self) -> int:
    """Runs the test execution process.

        Returns:
            The number of failed tests.
        """
    self._initialize_xml()

    gtest_list_output = self.list_tests()
    all_tests = self.parse_and_sort_tests(gtest_list_output)
    tests_to_run = self.filter_tests_for_shard(all_tests)

    logging.info("Shard %d/%d: Running %d tests.", self.shard_index,
                 self.total_shards, len(tests_to_run))

    passed_count = 0
    failed_count = 0

    for i, test in enumerate(tests_to_run):
      logging.info("[RUN] %s", test)
      retcode, temp_xml_path = self._run_single_test(test, i)

      if retcode == 0:
        passed_count += 1
      else:
        logging.error("[FAILED] %s (Exit Code: %d)", test, retcode)
        failed_count += 1

      if self.xml_output_file:
        self._merge_test_xml(test, temp_xml_path)

    if self.xml_output_file:
      self._finalize_and_write_xml(passed_count + failed_count, failed_count)

    logging.info("=" * 40)
    logging.info("Total: %d, Passed: %d, Failed: %d",
                 passed_count + failed_count, passed_count, failed_count)

    return failed_count


def parse_args() -> Tuple[argparse.Namespace, List[str]]:
  """Parses command line arguments."""
  parser = argparse.ArgumentParser(
      description="Cobalt Browser Test Runner Helper")
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
      default=0,
      help="Total shards for test launcher compatibility")
  parser.add_argument(
      "--test-launcher-shard-index",
      dest="tl_shard_index",
      type=int,
      default=0,
      help="Shard index for test launcher compatibility")
  parser.add_argument(
      "--user-data-dir",
      help="Path to the user data directory for the test binary. "
      "Overrides TEST_USER_DATA_DIR env var. "
      "If not set, a temporary directory is used.")

  return parser.parse_known_args()


def main():
  """Main entry point for the script."""
  logging.basicConfig(level=logging.INFO, format="%(message)s")
  args, unknown_args = parse_args()
  with CobaltTestRunner(args, unknown_args) as runner:
    failed_count = runner.run()
    sys.exit(1 if failed_count > 0 else 0)


if __name__ == "__main__":
  main()
