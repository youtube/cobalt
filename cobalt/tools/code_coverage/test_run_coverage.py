#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights. Reserved.
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
"""
Tests for the run_coverage.py script.
"""

import argparse
import os
import subprocess
import sys
import unittest
from unittest import mock
from cobalt.tools.code_coverage import run_coverage


class RunCoverageTest(unittest.TestCase):
  """
  Tests for the end-to-end code coverage script.
  """

  @mock.patch('os.makedirs')
  @mock.patch('subprocess.check_call')
  @mock.patch('argparse.ArgumentParser')
  def test_run_coverage_success_with_targets(self, mock_arg_parser,
                                             mock_check_call, mock_makedirs):
    """
    Tests successful execution when targets are provided as arguments.
    """
    platform = 'android-x86'
    output_dir = 'out/coverage_report'
    filters = 'cobalt/browser'
    target = 'cobalt:unittests'
    sanitized_target = 'cobalt_unittests'
    executable_name = 'unittests'
    build_dir = f'out/{platform}_devel-coverage'
    expected_command = os.path.join(build_dir, executable_name)
    target_output_dir = os.path.join(output_dir, sanitized_target)

    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform=platform,
        output_dir=output_dir,
        targets=[target],
        filters=[filters])

    result = run_coverage.main()

    mock_makedirs.assert_called_once_with(target_output_dir, exist_ok=True)
    expected_gn_command = [
        sys.executable, run_coverage.GN_PY_PATH, '-p', platform, '--coverage'
    ]
    expected_coverage_command = [
        sys.executable,
        run_coverage.COVERAGE_TOOL_PATH,
        '-b',
        build_dir,
        '-o',
        target_output_dir,
        '-c',
        expected_command,
        '-f',
        filters,
        '--format=lcov',
        executable_name,
    ]
    mock_check_call.assert_has_calls(
        [mock.call(expected_gn_command),
         mock.call(expected_coverage_command)])
    self.assertEqual(result, 0)

  @mock.patch('os.makedirs')
  @mock.patch(
      'cobalt.tools.code_coverage.run_coverage.discover_targets',
      return_value=['target1', 'target2'])
  @mock.patch('subprocess.check_call')
  @mock.patch('argparse.ArgumentParser')
  def test_run_coverage_success_no_targets(self, mock_arg_parser,
                                           mock_check_call,
                                           mock_discover_targets,
                                           mock_makedirs):
    """
    Tests successful execution when targets are auto-discovered.
    """
    del mock_makedirs  # Unused argument.
    platform = 'android-x86'
    output_dir = 'out/coverage_report'
    build_dir = f'out/{platform}_devel-coverage'

    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform=platform, output_dir=output_dir, targets=[], filters=None)

    result = run_coverage.main()

    mock_discover_targets.assert_called_once_with('android-arm')
    expected_gn_command = [
        sys.executable, run_coverage.GN_PY_PATH, '-p', platform, '--coverage'
    ]
    expected_calls = [mock.call(expected_gn_command)]
    for target in ['target1', 'target2']:
      target_output_dir = os.path.join(output_dir, target)
      expected_command = os.path.join(build_dir, target)
      expected_coverage_command = [
          sys.executable,
          run_coverage.COVERAGE_TOOL_PATH,
          '-b',
          build_dir,
          '-o',
          target_output_dir,
          '-c',
          expected_command,
          '--format=lcov',
          target,
      ]
      expected_calls.append(mock.call(expected_coverage_command))

    mock_check_call.assert_has_calls(expected_calls)
    self.assertEqual(result, 0)

  @mock.patch(
      'cobalt.tools.code_coverage.run_coverage.discover_targets',
      return_value=[])
  @mock.patch('builtins.print')
  @mock.patch('argparse.ArgumentParser')
  def test_no_targets_found(self, mock_arg_parser, mock_print,
                            mock_discover_targets):
    """
    Tests that the script exits gracefully if no targets are found.
    """
    platform = 'android-x86'
    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform=platform, output_dir='out/report', targets=[], filters=None)

    result = run_coverage.main()

    mock_discover_targets.assert_called_once_with('android-arm')
    mock_print.assert_any_call(
        f'No test targets found for platform {platform}.')
    self.assertEqual(result, 1)

  @mock.patch(
      'cobalt.tools.code_coverage.run_coverage.discover_targets',
      return_value=['target1'])
  @mock.patch(
      'subprocess.check_call',
      side_effect=subprocess.CalledProcessError(1, 'gn.py'))
  @mock.patch('builtins.print')
  @mock.patch('argparse.ArgumentParser')
  def test_gn_py_failure(self, mock_arg_parser, mock_print, mock_check_call,
                         mock_discover_targets):
    """
    Tests that the script exits gracefully if gn.py fails.
    """
    platform = 'android-x86'
    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform=platform, output_dir='out/report', targets=[], filters=None)

    result = run_coverage.main()

    mock_discover_targets.assert_called_once_with('android-arm')
    mock_check_call.assert_called_once()
    mock_print.assert_any_call(
        f"Error running gn.py: {subprocess.CalledProcessError(1, 'gn.py')}")
    self.assertEqual(result, 1)

  @mock.patch('os.makedirs')
  @mock.patch(
      'subprocess.check_call',
      side_effect=[
          0, subprocess.CalledProcessError(1, 'code_coverage_tool.py')
      ])
  @mock.patch('builtins.print')
  @mock.patch('argparse.ArgumentParser')
  def test_coverage_tool_failure(self, mock_arg_parser, mock_print,
                                 mock_check_call, mock_makedirs):
    """
    Tests that the script exits gracefully if code_coverage_tool.py fails.
    """
    del mock_makedirs  # Unused argument.
    target = 'target'
    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform='android-x86',
        output_dir='out/report',
        targets=[target],
        filters=None)

    result = run_coverage.main()

    self.assertEqual(mock_check_call.call_count, 2)
    mock_print.assert_any_call(
        'Error running code_coverage_tool.py for target target: '
        f"{subprocess.CalledProcessError(1, 'code_coverage_tool.py')}")
    self.assertEqual(result, 0)  # Should continue to next target

  @mock.patch('os.path.exists')
  @mock.patch('builtins.open', new_callable=mock.mock_open)
  def test_discover_targets_android_platforms(self, mock_open, mock_exists):
    """
    Tests that discover_targets correctly finds test_targets.json
    for Android platforms.
    """
    # Simulate the existence of test_targets.json for
    # android-arm and android-arm64
    mock_exists.side_effect = lambda path: (
        'android-arm/test_targets.json' in path or
        'android-arm64/test_targets.json' in path)

    # Mock the content of test_targets.json for android-arm
    mock_arm_json_content = (
        '{"test_targets": ["android_arm_target1", "android_arm_target2"]}')
    # Mock the content of test_targets.json for android-arm64
    mock_arm64_json_content = (
        '{"test_targets": ["android_arm64_target1", "android_arm64_target2"]}')

    def open_side_effect(file_path, *args, **kwargs):
      del args, kwargs  # unused
      if 'android-arm/test_targets.json' in file_path:
        mock_file = mock.mock_open(read_data=mock_arm_json_content).return_value
      elif 'android-arm64/test_targets.json' in file_path:
        mock_file = mock.mock_open(
            read_data=mock_arm64_json_content).return_value
      else:
        mock_file = mock.DEFAULT
      return mock_file

    mock_open.side_effect = open_side_effect

    # Test for android-arm (remapped from android-x86)
    targets_arm = run_coverage.discover_targets('android-arm')
    self.assertEqual(targets_arm,
                     ['android_arm_target1', 'android_arm_target2'])
    mock_exists.assert_any_call(
        os.path.join(run_coverage.TEST_TARGETS_DIR, 'android-arm',
                     'test_targets.json'))

    # Test for android-arm64 (remapped from android-x64)
    targets_arm64 = run_coverage.discover_targets('android-arm64')
    self.assertEqual(targets_arm64,
                     ['android_arm64_target1', 'android_arm64_target2'])
    mock_exists.assert_any_call(
        os.path.join(run_coverage.TEST_TARGETS_DIR, 'android-arm64',
                     'test_targets.json'))

  @mock.patch(
      'cobalt.tools.code_coverage.run_coverage.discover_targets',
      return_value=['target1', 'target2'])
  @mock.patch('subprocess.check_call')
  @mock.patch('argparse.ArgumentParser')
  def test_platform_reuse(self, mock_arg_parser, mock_check_call,
                          mock_discover_targets):
    """
    Tests that android-x86 and android-x64 platforms reuse android-arm and
    android-arm64 test targets respectively.
    """
    del mock_check_call  # Unused argument.
    # Test android-x86 reuses android-arm
    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform='android-x86',
        output_dir='out/report',
        targets=[],
        filters=None)
    run_coverage.main()
    mock_discover_targets.assert_called_with('android-arm')

    # Test android-x64 reuses android-arm64
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform='android-x64',
        output_dir='out/report',
        targets=[],
        filters=None)
    run_coverage.main()
    mock_discover_targets.assert_called_with('android-arm64')

  @mock.patch('os.makedirs')
  @mock.patch('os.path.exists', return_value=True)
  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch('subprocess.check_call')
  @mock.patch('argparse.ArgumentParser')
  def test_run_coverage_with_test_filters(  # pylint: disable=too-many-positional-arguments
      self, mock_arg_parser, mock_check_call, mock_open, mock_exists,
      mock_makedirs):
    """
    Tests successful execution when test filters are applied.
    """
    del mock_exists  # unused
    platform = 'android-x86'
    output_dir = 'out/coverage_report'
    target = 'cobalt:unittests'
    sanitized_target = 'cobalt_unittests'
    executable_name = 'unittests'
    build_dir = f'out/{platform}_devel-coverage'
    base_command = os.path.join(build_dir, executable_name)
    target_output_dir = os.path.join(output_dir, sanitized_target)
    filter_file_path = os.path.join(run_coverage.SRC_ROOT_PATH, 'cobalt',
                                    'testing', 'filters', 'android-arm',
                                    f'{executable_name}_filter.json')
    mock_filter_content = '{"failing_tests": ["Test.Fails", "Test.Crashes"]}'
    mock_open.side_effect = lambda path, *args, **kwargs: mock.mock_open(
        read_data=mock_filter_content
    ).return_value if path == filter_file_path else mock.DEFAULT

    mock_parser = mock_arg_parser.return_value
    mock_parser.parse_args.return_value = argparse.Namespace(
        platform=platform,
        output_dir=output_dir,
        targets=[target],
        filters=None)

    result = run_coverage.main()

    mock_makedirs.assert_called_once_with(target_output_dir, exist_ok=True)
    expected_gn_command = [
        sys.executable, run_coverage.GN_PY_PATH, '-p', platform, '--coverage'
    ]
    expected_coverage_command = [
        sys.executable,
        run_coverage.COVERAGE_TOOL_PATH,
        '-b',
        build_dir,
        '-o',
        target_output_dir,
        '-c',
        f'{base_command} --gtest_filter=-Test.Fails.Test.Crashes',
        '--format=lcov',
        executable_name,
    ]
    mock_check_call.assert_has_calls(
        [mock.call(expected_gn_command),
         mock.call(expected_coverage_command)])
    self.assertEqual(result, 0)
