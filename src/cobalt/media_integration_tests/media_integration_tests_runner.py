# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""The media integration test runner.

Usage:
  To run all tests of a specific type:
      "python media_integration_tests_runner.py
      -p android-arm64 -c devel -w noinstall -log-level info
      --type functionality"
  To run a specific test (suspend_resume):
      python media_integration_tests_runner.py
      -p android-arm64 -c devel -w noinstall -log-level info
      --type functionality --test_name suspend_resume"

"""

import argparse
import importlib
import logging
import pkgutil
import sys
import unittest

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_app import Features
from cobalt.media_integration_tests.test_case import SetLauncherParams, SetSupportedFeatures
from starboard.tools import abstract_launcher
from starboard.tools import command_line
from starboard.tools import log_level

# Location of test files.
_TESTS_PATH = {
    'functionality': 'cobalt.media_integration_tests.functionality',
    'endurance': 'cobalt.media_integration_tests.endurance',
    'performance': 'cobalt.media_integration_tests.performance'
}


def GetSupportedFeatures(launcher_params):
  launcher = abstract_launcher.LauncherFactory(
      launcher_params.platform,
      'cobalt',
      launcher_params.config,
      device_id=launcher_params.device_id,
      target_params=None,
      output_file=None,
      out_directory=launcher_params.out_directory,
      loader_platform=launcher_params.loader_platform,
      loader_config=launcher_params.loader_config,
      loader_out_directory=launcher_params.loader_out_directory)
  return {
      Features.SUSPEND_AND_RESUME: launcher.SupportsSystemSuspendResume(),
      Features.SEND_KEYS: True,
  }


def GetTestPackagePath(test_type):
  if _TESTS_PATH.has_key(test_type):
    return _TESTS_PATH[test_type]
  return None


def GetAllTestNamesInPackage(package_path):
  test_names = []
  loader = pkgutil.get_loader(package_path)
  for sub_module in pkgutil.walk_packages([loader.filename]):
    _, sub_module_name, _ = sub_module
    # Filter '_env' and '__init__'.
    if sub_module_name[0] == '_':
      continue
    if not sub_module_name in test_names:
      test_names.append(sub_module_name)
  return test_names


def LoadAllTests(package_path, all_test_names):
  test_suite = unittest.TestSuite()
  for test_name in all_test_names:
    module = importlib.import_module(package_path + '.' + test_name)
    test_suite.addTest(unittest.TestLoader().loadTestsFromModule(module))
  return test_suite


def main():
  # TODO: Support filters.
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--type',
      required=True,
      type=str,
      help=('Type of the tests to run. It must be functionality, endurance or'
            ' performance.'))
  parser.add_argument(
      '--test_name',
      default=None,
      type=str,
      help=('Name of test to run. If not specified, it will run all tests in'
            'the directory.'))
  args, _ = parser.parse_known_args()

  package_path = GetTestPackagePath(args.type)
  if package_path is None:
    logging.error('{%s} is not a valid test type.', args.type)
    return 2

  all_test_names = GetAllTestNamesInPackage(package_path)
  specified_test_name = args.test_name
  if specified_test_name is not None:
    if not specified_test_name in all_test_names:
      logging.error('{%s} is not a valid test name.', specified_test_name)
      return 2
    else:
      all_test_names = [specified_test_name]

  log_level.InitializeLogging(args)
  launcher_params = command_line.CreateLauncherParams()
  supported_features = GetSupportedFeatures(launcher_params)

  # Update global variables in test case.
  SetLauncherParams(launcher_params)
  SetSupportedFeatures(supported_features)

  unittest.installHandler()

  test_suite = LoadAllTests(package_path, all_test_names)

  return not unittest.TextTestRunner(
      verbosity=0, stream=sys.stdout).run(test_suite).wasSuccessful()


if __name__ == '__main__':
  sys.exit(main())
