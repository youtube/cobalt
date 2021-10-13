#!/usr/bin/python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Cross-platform unit test runner."""

import argparse
import logging
import os
import re
import signal
import subprocess
import sys
import threading
import traceback

import _env  # pylint: disable=unused-import, relative-import
import cStringIO
from starboard.tools import abstract_launcher
from starboard.tools import build
from starboard.tools import command_line
from starboard.tools import paths
from starboard.tools.testing import build_tests
from starboard.tools.testing import test_filter
from starboard.tools.util import SetupDefaultLoggingConfig

_FLAKY_RETRY_LIMIT = 4
_TOTAL_TESTS_REGEX = re.compile(r"^\[==========\] (.*) tests? from .*"
                                r"test cases? ran. \(.* ms total\)")
_TESTS_PASSED_REGEX = re.compile(r"^\[  PASSED  \] (.*) tests?")
_TESTS_FAILED_REGEX = re.compile(r"^\[  FAILED  \] (.*) tests?, listed below:")
_SINGLE_TEST_FAILED_REGEX = re.compile(r"^\[  FAILED  \] (.*)")

_CRASHPAD_TARGET = "crashpad_handler"
_LOADER_TARGET = "elf_loader_sandbox"


def _EnsureBuildDirectoryExists(path):
  if not os.path.exists(path):
    raise ValueError("'{}' does not exist.".format(path))


def _FilterTests(target_list, filters, config_name):
  """Returns a Mapping of test targets -> filtered tests."""
  targets = {}
  for target in target_list:
    targets[target] = []

  for platform_filter in filters:
    if platform_filter == test_filter.DISABLE_TESTING:
      return {}

    # Only filter the tests specifying our config or all configs.
    if platform_filter.config and platform_filter.config != config_name:
      continue

    target_name = platform_filter.target_name
    if platform_filter.test_name == test_filter.FILTER_ALL:
      if target_name in targets:
        # Filter the whole test binary
        del targets[target_name]
    else:
      if target_name not in targets:
        continue
      targets[target_name].append(platform_filter.test_name)

  return targets


def _VerifyConfig(config):
  """Ensures a platform or app config is self-consistent."""
  targets = config.GetTestTargets()
  filters = config.GetTestFilters()
  filter_targets = [
      f.target_name for f in filters if f != test_filter.DISABLE_TESTING
  ]

  # Filters must be defined in the same config as the targets they're filtering,
  # platform filters in platform config, and app filters in app config.
  unknown_targets = set(filter_targets) - set(targets)
  if unknown_targets:
    raise ValueError("Unknown filter targets in {} config ({}): {}".format(
        config.GetName(), config.__class__.__name__, sorted(unknown_targets)))


class TestLineReader(object):
  """Reads lines from the test runner's launcher output via an OS pipe.

  This is used to keep the output in memory instead of having to
  to write it to tempfiles, as well as to write the output to stdout
  in real time instead of dumping the test results all at once later.
  """

  def __init__(self, read_pipe):
    self.read_pipe = read_pipe
    self.output_lines = cStringIO.StringIO()
    self.stop_event = threading.Event()
    self.reader_thread = threading.Thread(target=self._ReadLines)

    # Don't allow this thread to block sys.exit() when ctrl+c is pressed.
    # Otherwise it will hang forever waiting for the pipe to close.
    self.reader_thread.daemon = True

  def _ReadLines(self):
    """Continuously reads and stores lines of test output."""
    while not self.stop_event.is_set():
      line = self.read_pipe.readline()
      if line:
        # Normalize line endings to unix.
        line = line.replace("\r", "")
        try:
          sys.stdout.write(line)
          sys.stdout.flush()
        except IOError as err:
          self.output_lines.write("error: " + str(err) + "\n")
          return
      else:
        break
      self.output_lines.write(line)

  def Start(self):
    self.reader_thread.start()

  def Kill(self):
    """Kills the thread reading lines from the launcher's output.

    This is only used on a manual exit to ensure that the thread exits cleanly;
    in the normal case, the thread will exit on its own when no more lines of
    output are available.
    """
    self.stop_event.set()

  def Join(self):
    """Ensures that all test output from the test launcher has been read."""
    self.reader_thread.join()

  def GetLines(self):
    """Returns stored output as a list of lines."""
    return self.output_lines.getvalue().split("\n")


class TestLauncher(object):
  """Manages the thread that the test object runs in.

  A separate thread is used to make it easier for the runner and reader to
  communicate, and for the main thread to shut them down.
  """

  def __init__(self, launcher):
    self.launcher = launcher
    self.runner_thread = threading.Thread(target=self._Run)

    self.return_code_lock = threading.Lock()
    self.return_code = 1

  def Start(self):
    self.runner_thread.start()

  def Kill(self):
    """Kills the running launcher."""
    try:
      logging.info("Killing launcher")
      self.launcher.Kill()
      logging.info("Launcher killed")
    except Exception:  # pylint: disable=broad-except
      sys.stderr.write("Error while killing {}:\n".format(
          self.launcher.target_name))
      traceback.print_exc(file=sys.stderr)

  def Join(self):
    self.runner_thread.join()

  def _Run(self):
    """Runs the launcher, and assigns a return code."""
    return_code = 1
    try:
      logging.info("Running launcher")
      return_code = self.launcher.Run()
      logging.info("Finished running launcher")
    except Exception:  # pylint: disable=broad-except
      sys.stderr.write("Error while running {}:\n".format(
          self.launcher.target_name))
      traceback.print_exc(file=sys.stderr)

    with self.return_code_lock:
      self.return_code = return_code

  def GetReturnCode(self):
    with self.return_code_lock:
      return_code = self.return_code
    return return_code


class TestRunner(object):
  """Runs unit tests."""

  def __init__(self,
               platform,
               config,
               loader_platform,
               loader_config,
               device_id,
               specified_targets,
               target_params,
               out_directory,
               loader_out_directory,
               platform_tests_only,
               application_name=None,
               dry_run=False,
               xml_output_dir=None,
               log_xml_results=False,
               launcher_args=None):
    self.platform = platform
    self.config = config
    self.loader_platform = loader_platform
    self.loader_config = loader_config
    self.device_id = device_id
    self.target_params = target_params
    self.out_directory = out_directory
    self.loader_out_directory = loader_out_directory
    self.launcher_args = launcher_args
    if not self.out_directory:
      self.out_directory = paths.BuildOutputDirectory(self.platform,
                                                      self.config)
    self.coverage_directory = os.path.join(self.out_directory, "coverage")
    if (not self.loader_out_directory and self.loader_platform and
        self.loader_config):
      self.loader_out_directory = paths.BuildOutputDirectory(
          self.loader_platform, self.loader_config)

    logging.info("Getting platform configuration")
    self._platform_config = build.GetPlatformConfig(platform)
    if self.loader_platform:
      self._loader_platform_config = build.GetPlatformConfig(loader_platform)
    logging.info("Got platform configuration")
    logging.info("Getting application configuration")
    self._app_config = self._platform_config.GetApplicationConfiguration(
        application_name)
    logging.info("Got application configuration")
    self.application_name = application_name
    self.dry_run = dry_run
    self.xml_output_dir = xml_output_dir
    self.log_xml_results = log_xml_results
    self.threads = []

    _EnsureBuildDirectoryExists(self.out_directory)
    _VerifyConfig(self._platform_config)

    if self.loader_platform:
      _EnsureBuildDirectoryExists(self.loader_out_directory)
      _VerifyConfig(self._loader_platform_config)

    _VerifyConfig(self._app_config)

    # If a particular test binary has been provided, configure only that one.
    logging.info("Getting test targets")
    if specified_targets:
      self.test_targets = self._GetSpecifiedTestTargets(specified_targets)
    else:
      self.test_targets = self._GetTestTargets(platform_tests_only)
    logging.info("Got test targets")
    self.test_env_vars = self._GetAllTestEnvVariables()

  def _Exec(self, cmd_list, output_file=None):
    """Execute a command in a subprocess."""
    try:
      msg = "Executing:\n    " + " ".join(cmd_list)
      logging.info(msg)
      if output_file:
        with open(output_file, "wb") as out:
          p = subprocess.Popen(  # pylint: disable=consider-using-with
              cmd_list,
              stdout=out,
              universal_newlines=True,
              cwd=self.out_directory)
      else:
        p = subprocess.Popen(  # pylint: disable=consider-using-with
            cmd_list,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            cwd=self.out_directory)
      p.wait()
      return p.returncode
    except KeyboardInterrupt:
      p.kill()
      return 1

  def _GetSpecifiedTestTargets(self, specified_targets):
    """Sets up specified test targets for a given platform and configuration.

    Args:
      specified_targets:  Array of test names to run.

    Returns:
      A mapping from the test binary names to a list of filters for that binary.
      If the test has no filters, its list is empty.

    Raises:
      RuntimeError:  The specified test binary has been disabled for the given
        platform and configuration.
    """
    targets = _FilterTests(specified_targets, self._GetTestFilters(),
                           self.config)
    if len(targets) != len(specified_targets):
      # If any of the provided target names have been filtered,
      # they will not all run.
      sys.stderr.write("Test list has been filtered. Not all will run.\n"
                       "Original list: \"{}\".\n"
                       "Filtered list: \"{}\".\n".format(
                           specified_targets, list(targets.keys())))

    return targets

  def _GetTestTargets(self, platform_tests_only):
    """Collects all test targets for a given platform and configuration.

    Args:
      platform_tests_only: If True then only the platform tests are fetched.

    Returns:
      A mapping from names of test binaries to lists of filters for
        each test binary.  If a test binary has no filters, its list is
        empty.
    """
    targets = self._platform_config.GetTestTargets()
    if not platform_tests_only:
      targets.extend(self._app_config.GetTestTargets())
    targets = list(set(targets))

    final_targets = _FilterTests(targets, self._GetTestFilters(), self.config)
    if not final_targets:
      sys.stderr.write("All tests were filtered; no tests will be run.\n")

    return final_targets

  def _GetTestFilters(self):
    """Get test filters for a given platform and configuration."""
    filters = self._platform_config.GetTestFilters()
    app_filters = self._app_config.GetTestFilters()
    if app_filters:
      filters.extend(app_filters)
    # Regardless of our own platform, if we are Evergreen we also need to
    # filter the tests that are filtered by the underlying platform. For
    # example, the 'evergreen-arm-hardfp' needs to filter the 'raspi-2'
    # filtered tests when it is running on a Raspberry Pi 2.
    if self.loader_platform and self.loader_config:
      loader_platform_config = build.GetPlatformConfig(self.loader_platform)
      loader_app_config = loader_platform_config.GetApplicationConfiguration(
          self.application_name)
      for filter_ in (loader_platform_config.GetTestFilters() +
                      loader_app_config.GetTestFilters()):
        if filter_ not in filters:
          filters.append(filter_)
    return filters

  def _GetAllTestEnvVariables(self):
    """Gets all environment variables used for tests on the given platform."""
    env_variables = {}
    for test, test_env in self._app_config.GetTestEnvVariables().iteritems():
      if test in env_variables:
        env_variables[test].update(test_env)
      else:
        env_variables[test] = test_env
    return env_variables

  def _RunTest(self, target_name, test_name=None):
    """Runs a specific target or test and collects the output.

    Args:
      target_name: The name of the test suite to be run.
      test_name: The name of the specific test to be run. The entire test suite
        is run when this is empty.

    Returns:
      A tuple containing tests results (See "_CollectTestResults()").
    """

    # Get the environment variables for the test target
    env = {}
    env.update(self._platform_config.GetTestEnvVariables())
    env.update(self.test_env_vars.get(target_name, {}))

    # Set up a pipe for processing test output
    read_fd, write_fd = os.pipe()
    read_pipe = os.fdopen(read_fd, "r")
    write_pipe = os.fdopen(write_fd, "w")

    # Filter the specified tests for this platform, if any
    test_params = []
    gtest_filter_value = ""

    # If a specific test case was given, filter for the exact name given.
    if test_name:
      gtest_filter_value = test_name
    elif self.test_targets[target_name]:
      gtest_filter_value = "-" + ":".join(self.test_targets[target_name])
    if gtest_filter_value:
      test_params.append("--gtest_filter=" + gtest_filter_value)

    def MakeLauncher():
      return abstract_launcher.LauncherFactory(
          self.platform,
          target_name,
          self.config,
          device_id=self.device_id,
          target_params=test_params,
          output_file=write_pipe,
          out_directory=self.out_directory,
          coverage_directory=self.coverage_directory,
          env_variables=env,
          loader_platform=self.loader_platform,
          loader_config=self.loader_config,
          loader_out_directory=self.loader_out_directory,
          launcher_args=self.launcher_args)

    if self.log_xml_results:
      out_path = MakeLauncher().GetDeviceOutputPath()
      xml_filename = "{}_testoutput.xml".format(target_name)
      if out_path:
        xml_path = os.path.join(out_path, xml_filename)
      else:
        xml_path = xml_filename
      test_params.append("--gtest_output=xml:{}".format(xml_path))
      logging.info(("Xml results for this test will "
                    "be logged to '%s'."), xml_path)
    elif self.xml_output_dir:
      # Have gtest create and save a test result xml
      xml_output_subdir = os.path.join(self.xml_output_dir, target_name)
      try:
        os.makedirs(xml_output_subdir)
      except OSError:
        pass
      xml_output_path = os.path.join(xml_output_subdir, "sponge_log.xml")
      logging.info("Xml output for this test will be saved to: %s",
                   xml_output_path)
      test_params.append("--gtest_output=xml:%s" % (xml_output_path))

    # Turn off color codes from output to make it easy to parse
    test_params.append("--gtest_color=no")

    test_params.extend(self.target_params)
    if self.dry_run:
      test_params.extend(["--gtest_list_tests"])

    logging.info("Initializing launcher")
    launcher = MakeLauncher()
    logging.info("Launcher initialized")

    test_reader = TestLineReader(read_pipe)
    test_launcher = TestLauncher(launcher)

    self.threads.append(test_launcher)
    self.threads.append(test_reader)

    dump_params = " ARGS:" + " ".join(test_params) if test_params else ""
    dump_env = " ENV VARS: " + ";".join(
        "{}={}".format(k, v) for k, v in env.items()) if env else ""
    # Output either the name of the test target or the specific test case
    # being run.
    # pylint: disable=g-long-ternary
    sys.stdout.write("Starting {}{}{}".format(
        test_name if test_name else target_name, dump_params, dump_env))

    # Output a newline before running the test target / case.
    sys.stdout.write("\n")

    if test_params:
      sys.stdout.write(" {}\n".format(test_params))
    test_reader.Start()
    logging.info("Starting test launcher")
    test_launcher.Start()
    logging.info("Test launcher started")

    # Wait for the launcher to exit then close the write pipe, which will
    # cause the reader to exit.
    test_launcher.Join()
    write_pipe.close()

    # Only after closing the write pipe, wait for the reader to exit.
    test_reader.Join()
    read_pipe.close()

    output = test_reader.GetLines()

    self.threads = []
    return self._CollectTestResults(output, target_name,
                                    test_launcher.GetReturnCode())

  def _CollectTestResults(self, results, target_name, return_code):
    """Collects passing and failing tests for one test binary.

    Args:
      results: A list containing each line of test results.
      target_name: The name of the test target being run.
      return_code: The return code of the test binary,

    Returns:
      A tuple of length 6, of the format (target_name, number_of_total_tests,
        number_of_passed_tests, number_of_failed_tests, list_of_failed_tests,
        return_code).
    """

    total_count = 0
    passed_count = 0
    failed_count = 0
    failed_tests = []

    for idx, line in enumerate(results):
      total_tests_match = _TOTAL_TESTS_REGEX.search(line)
      if total_tests_match:
        total_count = int(total_tests_match.group(1))

      passed_match = _TESTS_PASSED_REGEX.search(line)
      if passed_match:
        passed_count = int(passed_match.group(1))

      failed_match = _TESTS_FAILED_REGEX.search(line)
      if failed_match:
        failed_count = int(failed_match.group(1))
        # Descriptions of all failed tests appear after this line
        failed_tests = self._CollectFailedTests(results[idx + 1:])

    return (target_name, total_count, passed_count, failed_count, failed_tests,
            return_code)

  def _CollectFailedTests(self, lines):
    """Collects the names of all failed tests.

    Args:
      lines: The lines of log output where failed test names are located.

    Returns:
      A list of failed test names.
    """
    failed_tests = []
    for line in lines:
      test_failed_match = _SINGLE_TEST_FAILED_REGEX.search(line)
      if test_failed_match:
        failed_tests.append(test_failed_match.group(1))
    return failed_tests

  def _GetFilteredTestList(self, target_name):
    return _FilterTests([target_name], self._GetTestFilters(),
                        self.config).get(target_name, [])

  def _ProcessAllTestResults(self, results):
    """Collects and returns output for all selected tests.

    Args:
      results: List containing tuples corresponding to test results

    Returns:
      True if the test run succeeded, False if not.
    """
    if self.dry_run:
      logging.info("")  # formatting newline.
      logging.info("%d TOTAL TEST TARGETS", len(results))
      return True

    total_run_count = 0
    total_passed_count = 0
    total_failed_count = 0
    total_flaky_failed_count = 0
    total_filtered_count = 0

    print()  # Explicit print for empty formatting line.
    logging.info("TEST RUN COMPLETE.")
    if results:
      print()  # Explicit print for empty formatting line.

    # If the number of run tests from a test binary cannot be
    # determined, assume an error occurred while running it.
    error = False

    failed_test_groups = []

    for result_set in results:
      target_name = result_set[0]
      run_count = result_set[1]
      passed_count = result_set[2]
      failed_count = result_set[3]
      failed_tests = result_set[4]
      return_code = result_set[5]
      actual_failed_tests = []
      flaky_failed_tests = []
      filtered_tests = self._GetFilteredTestList(target_name)

      for test_name in failed_tests:
        if ".FLAKY_" in test_name:
          flaky_failed_tests.append(test_name)
        else:
          actual_failed_tests.append(test_name)

      actual_failed_count = len(actual_failed_tests)
      flaky_failed_count = len(flaky_failed_tests)
      filtered_count = len(filtered_tests)

      # If our math does not agree with gtest...
      if actual_failed_count + flaky_failed_count != failed_count:
        logging.warning("Inconsistent count of actual and flaky failed tests.")
        logging.info("")  # formatting newline.

      # Retry the flaky test cases that failed, and mark them as passed if they
      # succeed within the retry limit.
      if flaky_failed_count > 0:
        logging.info("RE-RUNNING FLAKY TESTS.\n")
        flaky_passed_tests = []
        for test_case in flaky_failed_tests:
          for retry in range(_FLAKY_RETRY_LIMIT):
            # Sometimes the returned test "name" includes information about the
            # parameter that was passed to it. This needs to be stripped off.
            retry_result = self._RunTest(target_name, test_case.split(",")[0])
            print()  # Explicit print for empty formatting line.
            if retry_result[2] == 1:
              flaky_passed_tests.append(test_case)
              logging.info("%s succeeded on run #%d!\n", test_case, retry + 2)
              break
            else:
              logging.warning("%s failed. Re-running...\n", test_case)
        # Remove newly passing flaky tests from failing flaky test list.
        for test_case in flaky_passed_tests:
          flaky_failed_tests.remove(test_case)
        flaky_failed_count -= len(flaky_passed_tests)
        passed_count += len(flaky_passed_tests)
      else:
        logging.info("")  # formatting newline.

      test_status = "SUCCEEDED"

      # Always mark as FAILED if we have a non-zero return code, or failing
      # test.
      if return_code != 0 or actual_failed_count > 0 or flaky_failed_count > 0:
        error = True
        test_status = "FAILED"
        failed_test_groups.append(target_name)
        # Be specific about the cause of failure if it was caused due to crash
        # upon exit. Normal Gtest failures have return_code = 1; test crashes
        # yield different return codes (e.g. segfault has return_code = 11).
        if (return_code != 1 and actual_failed_count == 0 and
            flaky_failed_count == 0):
          test_status = "FAILED (CRASHED)"

      logging.info("%s: %s.", target_name, test_status)
      if return_code != 0 and run_count == 0 and filtered_count == 0:
        logging.info("  Results not available.  Did the test crash?")
        logging.info("")  # formatting newline.
        continue

      logging.info("  TESTS RUN: %d", run_count)
      total_run_count += run_count
      # Output the number of passed, failed, flaked, and filtered tests.
      if passed_count > 0:
        logging.info("  TESTS PASSED: %d", passed_count)
        total_passed_count += passed_count
      if actual_failed_count > 0:
        logging.info("  TESTS FAILED: %d", actual_failed_count)
        total_failed_count += actual_failed_count
      if flaky_failed_count > 0:
        logging.info("  TESTS FLAKED: %d", flaky_failed_count)
        total_flaky_failed_count += flaky_failed_count
      if filtered_count > 0:
        logging.info("  TESTS FILTERED: %d", filtered_count)
        total_filtered_count += filtered_count
      # Output the names of the failed, flaked, and filtered tests.
      if actual_failed_count > 0:
        logging.info("")  # formatting newline.
        logging.info("  FAILED TESTS:")
        for line in actual_failed_tests:
          logging.info("    %s", line)
      if flaky_failed_count > 0:
        logging.info("")  # formatting newline.
        logging.info("  FLAKED TESTS:")
        for line in flaky_failed_tests:
          logging.info("    %s", line)
      if filtered_count > 0:
        logging.info("")  # formatting newline.
        logging.info("  FILTERED TESTS:")
        for line in filtered_tests:
          logging.info("    %s", line)
      logging.info("")  # formatting newline.
      logging.info("  RETURN CODE: %d", return_code)
      logging.info("")  # formatting newline.

    overall_status = "SUCCEEDED"
    result = True

    # Any failing test or other errors will make the run a failure. This
    # includes flaky tests if they did not pass any of their retries.
    if error or total_failed_count > 0 or total_flaky_failed_count > 0:
      overall_status = "FAILED"
      result = False

    logging.info("TEST RUN %s.", overall_status)
    if failed_test_groups:
      failed_test_groups = list(set(failed_test_groups))
      logging.info("  FAILED TESTS GROUPS: %s", ", ".join(failed_test_groups))
    logging.info("  TOTAL TESTS RUN: %d", total_run_count)
    logging.info("  TOTAL TESTS PASSED: %d", total_passed_count)
    logging.info("  TOTAL TESTS FAILED: %d", total_failed_count)
    logging.info("  TOTAL TESTS FLAKED: %d", total_flaky_failed_count)
    logging.info("  TOTAL TESTS FILTERED: %d", total_filtered_count)

    return result

  def BuildAllTargets(self, ninja_flags):
    """Runs build step for all specified unit test binaries.

    Args:
      ninja_flags: Command line flags to pass to ninja.

    Returns:
      True if the build succeeds, False if not.
    """
    result = True

    try:
      if ninja_flags:
        extra_flags = [ninja_flags]
      else:
        extra_flags = []

      # The loader is not built with the same platform configuration as our
      # tests so we need to build it separately.
      if self.loader_platform:
        build_tests.BuildTargets(
            [_LOADER_TARGET, _CRASHPAD_TARGET], self.loader_out_directory,
            self.dry_run,
            extra_flags + [os.getenv("TEST_RUNNER_PLATFORM_BUILD_FLAGS", "")])
      build_tests.BuildTargets(
          self.test_targets, self.out_directory, self.dry_run,
          extra_flags + [os.getenv("TEST_RUNNER_BUILD_FLAGS", "")])

    except subprocess.CalledProcessError as e:
      result = False
      sys.stderr.write("Error occurred during building.\n")
      sys.stderr.write("{}\n".format(e))

    return result

  def RunAllTests(self):
    """Runs all specified test binaries.

    Returns:
      True if the test run succeeded, False if not.
    """
    results = []
    # Sort the targets so they are run in alphabetical order
    for test_target in sorted(self.test_targets.keys()):
      results.append(self._RunTest(test_target))

    return self._ProcessAllTestResults(results)

  def GenerateCoverageReport(self):
    """Generate the source code coverage report."""
    available_profraw_files = []
    available_targets = []
    for target in sorted(self.test_targets.keys()):
      profraw_file = os.path.join(self.coverage_directory, target + ".profraw")
      if os.path.isfile(profraw_file):
        available_profraw_files.append(profraw_file)
        available_targets.append(target)

    # If there are no profraw files, then there is no work to do.
    if not available_profraw_files:
      return

    report_name = "report"
    profdata_name = os.path.join(self.coverage_directory,
                                 report_name + ".profdata")
    merge_cmd_list = [
        "llvm-profdata", "merge", "-sparse=true", "-o", profdata_name
    ]
    merge_cmd_list += available_profraw_files

    self._Exec(merge_cmd_list)
    show_cmd_list = [
        "llvm-cov", "show", "-instr-profile=" + profdata_name, "-format=html",
        "-output-dir=" + os.path.join(self.coverage_directory, "html"),
        available_targets[0]
    ]
    show_cmd_list += ["-object=" + target for target in available_targets[1:]]
    self._Exec(show_cmd_list)

    report_cmd_list = [
        "llvm-cov", "report", "-instr-profile=" + profdata_name,
        available_targets[0]
    ]
    report_cmd_list += ["-object=" + target for target in available_targets[1:]]
    self._Exec(
        report_cmd_list,
        output_file=os.path.join(self.coverage_directory, report_name + ".txt"))
    return


def main():
  SetupDefaultLoggingConfig()
  arg_parser = argparse.ArgumentParser()
  command_line.AddLauncherArguments(arg_parser)
  arg_parser.add_argument(
      "-b",
      "--build",
      action="store_true",
      help="Specifies whether to build the tests.")
  arg_parser.add_argument(
      "-r",
      "--run",
      action="store_true",
      help="Specifies whether to run the tests."
      " If both the \"--build\" and \"--run\" flags are not"
      " provided, this is the default.")
  arg_parser.add_argument(
      "-n",
      "--dry_run",
      action="store_true",
      help="Specifies to show what would be done without actually doing it.")
  arg_parser.add_argument(
      "-t",
      "--target_name",
      action="append",
      help="Name of executable target. Repeatable for multiple targets.")
  arg_parser.add_argument(
      "--platform_tests_only",
      action="store_true",
      help="Runs only a small set of tests involved testing the platform.")
  arg_parser.add_argument(
      "-a",
      "--application_name",
      default="cobalt",  # TODO: Pass this in explicitly.
      help="Name of the application to run tests under, e.g. 'cobalt'.")
  arg_parser.add_argument(
      "--ninja_flags",
      help="Flags to pass to the ninja build system. Provide them exactly"
      " as you would on the command line between a set of double quotation"
      " marks.")
  arg_parser.add_argument(
      "-x",
      "--xml_output_dir",
      help="If defined, results will be saved as xml files in given directory."
      " Output for each test suite will be in it's own subdirectory and file:"
      " <xml_output_dir>/<test_suite_name>/sponge_log.xml")
  arg_parser.add_argument(
      "-l",
      "--log_xml_results",
      action="store_true",
      help="If set, results will be logged in xml format after all tests are"
      " complete. --xml_output_dir will be ignored.")
  arg_parser.add_argument(
      "-w",
      "--launcher_args",
      help="Pass space-separated arguments to control launcher behaviour. "
      "Arguments are platform specific and may not be implemented for all "
      "platforms. Common arguments are:\n\t'noinstall' - skip install steps "
      "before running the test\n\t'systools' - use system-installed tools.")
  args = arg_parser.parse_args()

  if (args.loader_platform and not args.loader_config or
      args.loader_config and not args.loader_platform):
    arg_parser.error(
        "You must specify both --loader_platform and --loader_config.")
    return 1

  # Extra arguments for the test target
  target_params = []
  if args.target_params:
    target_params = args.target_params.split(" ")

  launcher_args = []
  if args.launcher_args:
    launcher_args = args.launcher_args.split(" ")

  if args.dry_run:
    launcher_args.append(abstract_launcher.ARG_DRYRUN)

  logging.info("Initializing test runner")
  runner = TestRunner(args.platform, args.config, args.loader_platform,
                      args.loader_config, args.device_id, args.target_name,
                      target_params, args.out_directory,
                      args.loader_out_directory, args.platform_tests_only,
                      args.application_name, args.dry_run, args.xml_output_dir,
                      args.log_xml_results, launcher_args)
  logging.info("Test runner initialized")

  def Abort(signum, frame):
    del signum, frame  # Unused.
    sys.stderr.write("Killing threads\n")
    for active_thread in runner.threads:
      active_thread.Kill()
    sys.exit(1)

  signal.signal(signal.SIGINT, Abort)
  # If neither build nor run has been specified, assume the client
  # just wants to run.
  if not args.build and not args.run:
    args.run = True

  build_success = True
  run_success = True

  if args.dry_run:
    sys.stderr.write("=== Dry run ===\n")

  if args.build:
    build_success = runner.BuildAllTargets(args.ninja_flags)
    # If the build fails, don't try to run the tests.
    if not build_success:
      return 1

  if args.run:
    run_success = runner.RunAllTests()

  runner.GenerateCoverageReport()

  # If either step has failed, count the whole test run as failed.
  if not build_success or not run_success:
    return 1
  else:
    return 0


if __name__ == "__main__":
  sys.exit(main())
