#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""A script to run Cobalt browser tests on an Android device."""

import argparse
import os
import subprocess
import sys
import tempfile


def run_command(command, env=None, exit_on_error=True):
  """Runs a command and checks for errors."""
  print(f"Executing: {' '.join(command)}")
  try:
    # By not capturing stdout/stderr, the subprocess's output will be streamed
    # directly to the console in real-time.
    subprocess.run(command, check=True, env=env)
  except (subprocess.CalledProcessError, FileNotFoundError) as e:
    # The command's output will have already been printed to stderr.
    print(f"Error executing command: {' '.join(command)}")
    if hasattr(e, 'returncode'):
      print(f"Return code: {e.returncode}")
    if exit_on_error:
      sys.exit(1)


def main():
  """Main function to run the adb commands."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--apk-path', required=True, help='Path to the test APK to install.')
  parser.add_argument(
      '--build-dir',
      required=True,
      help='Build directory containing test runner script runtime deps.')
  parser.add_argument(
      '--test-list', help='Path to a file containing a list of tests to run.')
  parser.add_argument(
      '--android-serial', help='Serial number of the device to run tests on.')
  parser.add_argument('--gtest_filter', help='Gtest filter for the tests.')
  args = parser.parse_args()

  env = os.environ.copy()
  if args.android_serial:
    env['ANDROID_SERIAL'] = args.android_serial

  try:
    # Install the APK
    run_command(['adb', 'install', '-r', '-t', '-d', '-g', args.apk_path],
                env=env)

    # Create and push dependencies
    deps_path = os.path.join(
        args.build_dir, 'gen.runtime/cobalt/testing/browser_tests/'
        'cobalt_browsertests__test_runner_script.runtime_deps')
    tar_command = ['tar', '-czf', 'deps.tar.gz', deps_path]
    run_command(tar_command, env=env)

    run_command([
        'adb', 'push', 'deps.tar.gz', '/sdcard/chromium_tests_root/deps.tar.gz'
    ],
                env=env)
    run_command(['rm', 'deps.tar.gz'], env=env)

    commands = [
        [
            'adb',
            'root',
        ],
        [
            'adb', 'shell', 'appops', 'set',
            'org.chromium.content_browsertests_apk', 'MANAGE_EXTERNAL_STORAGE',
            'allow'
        ],
        [
            'adb', 'shell', 'pm', 'grant',
            'org.chromium.content_browsertests_apk', 'android.permission.CAMERA'
        ],
        [
            'adb', 'shell', 'pm', 'grant',
            'org.chromium.content_browsertests_apk',
            'android.permission.READ_EXTERNAL_STORAGE'
        ],
        [
            'adb', 'shell', 'pm', 'grant',
            'org.chromium.content_browsertests_apk',
            'android.permission.RECORD_AUDIO'
        ],
        [
            'adb', 'shell', 'pm', 'grant',
            'org.chromium.content_browsertests_apk',
            'android.permission.WRITE_EXTERNAL_STORAGE'
        ],
        [
            'adb', 'shell', 'pm', 'grant',
            'org.chromium.content_browsertests_apk',
            'android.permission.ACCESS_COARSE_LOCATION'
        ],
        [
            'adb', 'shell', 'tar', '-xzf',
            '/sdcard/chromium_tests_root/deps.tar.gz', '-C',
            '/sdcard/chromium_tests_root/'
        ],
    ]

    for cmd in commands:
      run_command(cmd, env=env)

    # Create a temporary file for the test list
    test_list_file = None
    if args.gtest_filter:
      with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
        f.write(args.gtest_filter)
        test_list_file = f.name
    elif args.test_list:
      test_list_file = args.test_list

    # Push the test list to the device
    device_test_list_path = '/data/local/tmp/test_list.txt'
    if test_list_file:
      run_command(['adb', 'push', test_list_file, device_test_list_path],
                  env=env)
      if args.gtest_filter:
        os.remove(test_list_file)

    # Construct the instrument command
    instrument_command = [
        'am', 'instrument', '-w', '-e',
        'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.NativeTestActivity',
        'org.chromium.content_browsertests_apk.ContentBrowserTestsActivity',
        '-e', 'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.ShardSizeLimit', '1', '-e',
        'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.ShardNanoTimeout', '120000000000', '-e',
        'org.chromium.native_test.NativeTestInstrumentationTestRunner'
        '.StdoutFile', '/sdcard/Download/test_results.txt'
    ]
    if test_list_file:
      instrument_command.extend([
          '-e', 'org.chromium.native_test.NativeTestInstrumentationTestRunner'
          '.TestList', device_test_list_path
      ])

    instrument_command.extend([
        'org.chromium.content_browsertests_apk/org.chromium.build'
        '.gtest_apk.NativeTestInstrumentationTestRunner'
    ])

    # Create a shell script to run the instrument command.
    script_name = 'run_instrumentation_tests.sh'
    # The command to be executed inside the shell script on the device.
    device_command_str = ' '.join(instrument_command)
    # Redirect output to a temp file on the device
    device_command_str += ' > /data/local/tmp/instrument_output.txt 2>&1'

    with open(script_name, 'w', encoding='utf-8') as f:
      f.write('#!/system/bin/sh\n')
      f.write(device_command_str + '\n')

    print('Instrumentation command to be executed'
          f"on device: {device_command_str}")

    device_script_path = '/data/local/tmp/' + script_name
    run_command(['adb', 'push', script_name, device_script_path], env=env)
    run_command(['rm', script_name], env=env, exit_on_error=False)
    run_command(['adb', 'shell', 'chmod', '+x', device_script_path], env=env)
    run_command(['adb', 'shell', 'sh', device_script_path], env=env)
    run_command(['adb', 'shell', 'rm', '-f', device_script_path],
                env=env,
                exit_on_error=False)

    print('All commands executed successfully.')
  finally:
    print('Starting cleanup...')
    run_command(['adb', 'uninstall', 'org.chromium.content_browsertests_apk'],
                env=env,
                exit_on_error=False)
    run_command(['adb', 'shell', 'rm', '-rf', '/sdcard/chromium_tests_root'],
                env=env,
                exit_on_error=False)
    run_command(['adb', 'pull', '/sdcard/Download/test_results.txt', '.'],
                env=env,
                exit_on_error=False)
    run_command(
        ['adb', 'shell', 'rm', '-f', '/sdcard/Download/test_results.txt'],
        env=env,
        exit_on_error=False)
    if test_list_file:
      run_command(['adb', 'shell', 'rm', '-f', device_test_list_path],
                  env=env,
                  exit_on_error=False)
    print('Cleanup finished.')


if __name__ == '__main__':
  main()
