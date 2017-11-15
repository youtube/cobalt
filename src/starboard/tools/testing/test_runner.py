#!/usr/bin/python
#
# Copyright 2017 Google Inc. All Rights Reserved.
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

import cStringIO
import os
import re
import signal
import subprocess
import sys
import threading
import traceback

import _env  # pylint: disable=unused-import
from starboard.tools import abstract_launcher
from starboard.tools import command_line
from starboard.tools import environment
from starboard.tools.testing import test_filter


_TOTAL_TESTS_REGEX = (r"\[==========\] (.*) tests? from .*"
                      r"test cases? ran. \(.* ms total\)")
_TESTS_PASSED_REGEX = r"\[  PASSED  \] (.*) tests?"
_TESTS_FAILED_REGEX = r"\[  FAILED  \] (.*) tests?, listed below:"
_SINGLE_TEST_FAILED_REGEX = r"\[  FAILED  \] (.*)"


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

  def _ReadLines(self):
    """Continuously reads and stores lines of test output."""
    while not self.stop_event.is_set():
      line = self.read_pipe.readline()
      if line:
        sys.stdout.write(line)
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
    self.reader_thread.join()
    self.read_pipe.close()

  def GetLines(self):
    """Stops file reading, then returns stored output as a list of lines."""
    return self.output_lines.getvalue().split("\n")


class TestLauncher(object):
  """Manages the thread that the test object runs in.

  A separate thread is used to make it easier for the runner and reader to
  communicate, and for the main thread to shut them down.
  """

  # The write end of the pipe is provided here because if the launcher
  # errors out and fails to close it, it needs to be closed anyway.
  def __init__(self, launcher, write_pipe):
    self.launcher = launcher
    self.write_pipe = write_pipe
    self.runner_thread = threading.Thread(target=self._Run)

    self.return_code_lock = threading.Lock()
    self.return_code = 1

  def Start(self):
    self.runner_thread.start()

  def Kill(self):
    """Kills the running launcher."""
    try:
      self.launcher.Kill()
    except Exception:
      sys.stderr.write("Error while killing {}:\n".format(
          self.launcher.target_name))
      traceback.print_exc(file=sys.stderr)
      # Close the write end of the pipe if the launcher errored out
      # before closing it.
      if not self.write_pipe.closed:
        self.write_pipe.close()

  def Join(self):
    self.runner_thread.join()

  def _Run(self):
    """Runs the launcher, and assigns a return code."""
    return_code = 1
    try:
      return_code = self.launcher.Run()
    except Exception:
      sys.stderr.write("Error while running {}:\n".format(
          self.launcher.target_name))
      traceback.print_exc(file=sys.stderr)

    self.return_code_lock.acquire()
    self.return_code = return_code
    self.return_code_lock.release()

  def GetReturnCode(self):
    self.return_code_lock.acquire()
    return_code = self.return_code
    self.return_code_lock.release()
    return return_code


class TestRunner(object):
  """Runs unit tests."""

  def __init__(self, platform, config, device_id, single_target,
               target_params, out_directory):
    self.platform = platform
    self.config = config
    self.device_id = device_id
    self.target_params = target_params
    self.out_directory = out_directory
    self._platform_config = abstract_launcher.GetGypModuleForPlatform(
        platform).CreatePlatformConfig()
    self.threads = []

    # If a particular test binary has been provided, configure only that one.
    if single_target:
      self.test_targets = self._GetSingleTestTarget(single_target)
    else:
      self.test_targets = self._GetTestTargets()

    self.test_env_vars = self._GetAllTestEnvVariables()

  def _GetSingleTestTarget(self, single_target):
    """Sets up a single test target for a given platform and configuration.

    Args:
      single_target:  The name of a test target to run.

    Returns:
      A mapping from the test binary name to a list of filters for that binary.
      If the test has no filters, its list is empty.

    Raises:
      RuntimeError:  The specified test binary has been disabled for the given
        platform and configuration.
    """
    platform_filters = self._platform_config.GetTestFilters()

    final_targets = {}
    final_targets[single_target] = []

    for platform_filter in platform_filters:
      if platform_filter == test_filter.DISABLE_TESTING:
        return {}
      if platform_filter.target_name == single_target:
        # Only filter the tests specifying our config or all configs.
        if platform_filter.config == self.config or not platform_filter.config:
          if platform_filter.test_name == test_filter.FILTER_ALL:
            # If the provided target name has been filtered,
            # nothing will be run.
            sys.stderr.write(
                "\"{}\" has been filtered; no tests will be run.\n".format(
                    platform_filter.target_name))
            del final_targets[platform_filter.target_name]
          else:
            final_targets[single_target].append(
                platform_filter.test_name)

    return final_targets

  def _GetTestTargets(self):
    """Collects all test targets for a given platform and configuration.

    Returns:
      A mapping from names of test binaries to lists of filters for
        each test binary.  If a test binary has no filters, its list is
        empty.
    """
    platform_filters = self._platform_config.GetTestFilters()

    final_targets = {}

    all_targets = environment.GetTestTargets()
    for target in all_targets:
      final_targets[target] = []

    for platform_filter in platform_filters:
      if platform_filter == test_filter.DISABLE_TESTING:
        return {}
      # Only filter the tests specifying our config or all configs.
      if platform_filter.config == self.config or not platform_filter.config:
        if platform_filter.test_name == test_filter.FILTER_ALL:
          # Filter the whole test binary
          del final_targets[platform_filter.target_name]
        else:
          final_targets[platform_filter.target_name].append(
              platform_filter.test_name)

    return final_targets

  def _GetAllTestEnvVariables(self):
    """Gets all environment variables used for tests on the given platform."""
    return self._platform_config.GetTestEnvVariables()

  def _BuildTests(self, ninja_flags):
    """Builds all specified test binaries.

    Args:
      ninja_flags: Command line flags to pass to ninja.
    """
    if not self.test_targets:
      return

    if self.out_directory:
      build_dir = self.out_directory
    else:
      build_dir = abstract_launcher.DynamicallyBuildOutDirectory(
          self.platform, self.config)

    args_list = ["ninja", "-C", build_dir]
    args_list.extend([
        "{}_deploy".format(test_name) for test_name in self.test_targets])
    if ninja_flags:
      args_list.append(ninja_flags)
    if "TEST_RUNNER_BUILD_FLAGS" in os.environ:
      args_list.append(os.environ["TEST_RUNNER_BUILD_FLAGS"])
    sys.stderr.write("{}\n".format(args_list))
    # We set shell=True because otherwise Windows doesn't recognize
    # PATH properly.
    #   https://bugs.python.org/issue15451
    # We flatten the arguments to a string because with shell=True, Linux
    # doesn't parse them properly.
    #   https://bugs.python.org/issue6689
    subprocess.check_call(" ".join(args_list), shell=True)

  def _RunTest(self, target_name):
    """Runs a single unit test binary and collects all of the output.

    Args:
      target_name: The name of the test target being run.

    Returns:
      A tuple containing tests results (See "_CollectTestResults()").
    """

    # Get the environment variables for the test target
    env = self.test_env_vars.get(target_name, {})

    # Set up a pipe for processing test output
    read_fd, write_fd = os.pipe()
    read_pipe = os.fdopen(read_fd, "r")
    write_pipe = os.fdopen(write_fd, "w")

    # Filter the specified tests for this platform, if any
    test_params = []
    if self.test_targets[target_name]:
      test_params.append("--gtest_filter=-{}".format(":".join(
          self.test_targets[target_name])))
    test_params.extend(self.target_params)

    launcher = abstract_launcher.LauncherFactory(
        self.platform, target_name, self.config,
        device_id=self.device_id, target_params=test_params,
        output_file=write_pipe, out_directory=self.out_directory,
        env_variables=env)

    test_reader = TestLineReader(read_pipe)
    test_launcher = TestLauncher(launcher, write_pipe)

    self.threads.append(test_launcher)
    self.threads.append(test_reader)

    sys.stdout.write("Starting {}\n".format(target_name))

    test_reader.Start()
    test_launcher.Start()

    # If there are actives threads during a ctrl+c exit, they will join here.
    test_launcher.Join()
    test_reader.Join()

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
      total_tests_match = re.search(_TOTAL_TESTS_REGEX, line)
      if total_tests_match:
        total_count = int(total_tests_match.group(1))

      passed_match = re.search(_TESTS_PASSED_REGEX, line)
      if passed_match:
        passed_count = int(passed_match.group(1))

      failed_match = re.search(_TESTS_FAILED_REGEX, line)
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
      test_failed_match = re.search(_SINGLE_TEST_FAILED_REGEX, line)
      if test_failed_match:
        failed_tests.append(test_failed_match.group(1))
    return failed_tests

  def _ProcessAllTestResults(self, results):
    """Collects and returns output for all selected tests.

    Args:
      results: List containing tuples corresponding to test results

    Returns:
      True if the test run succeeded, False if not.
    """
    total_run_count = 0
    total_passed_count = 0
    total_failed_count = 0

    # If the number of run tests from a test binary cannot be
    # determined, assume an error occured while running it.
    error = False

    print "\nTEST RUN COMPLETE. RESULTS BELOW:\n"

    for result_set in results:

      target_name = result_set[0]
      run_count = result_set[1]
      passed_count = result_set[2]
      failed_count = result_set[3]
      failed_tests = result_set[4]
      return_code = result_set[5]

      test_status = "SUCCEEDED"
      if return_code != 0:
        error = True
        test_status = "FAILED"

      print "{}: {}.".format(target_name, test_status)
      if run_count == 0:
        print"  Results not available.  Did the test crash?\n"
        continue

      print "  TOTAL TESTS RUN: {}".format(run_count)
      total_run_count += run_count
      print "  PASSED: {}".format(passed_count)
      total_passed_count += passed_count
      if failed_count > 0:
        print "  FAILED: {}".format(failed_count)
        total_failed_count += failed_count
        print "\n  FAILED TESTS:"
        for line in failed_tests:
          print "    {}".format(line)
      # Print a single newline to separate results from each test run
      print

    overall_status = "SUCCEEDED"
    result = True

    if error or total_failed_count > 0:
      overall_status = "FAILED"
      result = False

    print "TEST RUN {}.".format(overall_status)
    print "  TOTAL TESTS RUN: {}".format(total_run_count)
    print "  TOTAL TESTS PASSED: {}".format(total_passed_count)
    print "  TOTAL TESTS FAILED: {}".format(total_failed_count)

    return result

  def BuildAllTests(self, ninja_flags):
    """Runs build step for all specified unit test binaries.

    Args:
      ninja_flags: Command line flags to pass to ninja.

    Returns:
      True if the build succeeds, False if not.
    """
    result = True

    try:
      self._BuildTests(ninja_flags)
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


def main():

  arg_parser = command_line.CreateParser()
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
      "-t",
      "--target_name",
      help="Name of executable target.")
  arg_parser.add_argument(
      "--ninja_flags",
      help="Flags to pass to the ninja build system. Provide them exactly"
      " as you would on the command line between a set of double quotation"
      " marks.")

  args = arg_parser.parse_args()

  # Extra arguments for the test target
  target_params = []
  if args.target_params:
    target_params = args.target_params.split(" ")

  runner = TestRunner(args.platform, args.config, args.device_id,
                      args.target_name, target_params, args.out_directory)

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

  if args.build:
    build_success = runner.BuildAllTests(args.ninja_flags)
    # If the build fails, don't try to run the tests.
    if not build_success:
      return 1

  if args.run:
    run_success = runner.RunAllTests()

  # If either step has failed, count the whole test run as failed.
  if not build_success or not run_success:
    return 1
  else:
    return 0

if __name__ == "__main__":
  sys.exit(main())
