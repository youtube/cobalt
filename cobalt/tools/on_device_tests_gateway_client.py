#!/usr/bin/env python3
#
# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""gRPC On-device Tests Gateway client."""

import argparse
import json
import logging
import sys

import grpc
import on_device_tests_gateway_pb2
import on_device_tests_gateway_pb2_grpc


_WORK_DIR = '/on_device_tests_gateway'

# For local testing, set: _ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = ('localhost')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = (
    'on-device-tests-gateway-service.on-device-tests.svc.cluster.local')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '50052'

_ON_DEVICE_OUTPUT_FILES_DIR = '/sdcard/Download'
_CHROMIUM_TEST_ROOT = '/sdcard/chromium_tests_root'

class OnDeviceTestsGatewayClient():
  """On-device tests Gateway Client class."""

  def __init__(self):
    self.channel = grpc.insecure_channel(
        target=f'{_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST}:{_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT}',  # pylint:disable=line-too-long
        # These options need to match server settings.
        options=[('grpc.keepalive_time_ms', 10000),
                 ('grpc.keepalive_timeout_ms', 5000),
                 ('grpc.keepalive_permit_without_calls', 1),
                 ('grpc.http2.max_pings_without_data', 0),
                 ('grpc.http2.min_time_between_pings_ms', 10000),
                 ('grpc.http2.min_ping_interval_without_data_ms', 5000)])
    self.stub = on_device_tests_gateway_pb2_grpc.on_device_tests_gatewayStub(
        self.channel)

  def run_trigger_command(self, args: argparse.Namespace, test_requests):
    """Calls On-Device Tests service and passing given parameters to it.

    Args:
        args (Namespace): Arguments passed in command line.
        test_requests (list): A list of test requests.
    """
    for response_line in self.stub.exec_command(
        on_device_tests_gateway_pb2.OnDeviceTestsCommand(
            token=args.token,
            labels=args.label,
            test_requests=test_requests,
        )):

      print(response_line.response)

  def run_watch_command(self, workdir: str, args: argparse.Namespace):
    """Calls On-Device Tests watch service and passing given parameters to it.

    Args:
        workdir (str): Current script workdir.
        args (Namespace): Arguments passed in command line.
    """
    for response_line in self.stub.exec_watch_command(
        on_device_tests_gateway_pb2.OnDeviceTestsWatchCommand(
            workdir=workdir,
            token=args.token,
            change_id=args.change_id,
            session_id=args.session_id,
        )):

      print(response_line.response)


def _read_json_config(filename):
  """Reads and parses data from a JSON configuration file.

  Args:
    filename: The name of the JSON configuration file.

  Returns:
    A list of dictionaries, where each dictionary represents a test
    configuration.
  """
  try:
    with open(filename, 'r') as f:
      data = json.load(f)
    return data
  except FileNotFoundError:
    print(f" Config file '{filename}' not found.")
    return None
  except json.JSONDecodeError:
    print(f" Invalid JSON format in '{filename}'.")
    return None


def _get_gtest_filters(filter_json_dir, gtest_target):
  """Retrieves gtest filters for a given target.

  Args:
      filter_json_dir: Directory containing filter JSON files.
      gtest_target: The name of the gtest target.

  Returns:
      A string containing the gtest filters.
  """
  gtest_filters = '*'
  filter_json_file = f'{filter_json_dir}/{gtest_target}_filter.json'
  print(f"  gtest_filter_json_file = {filter_json_file}")
  filter_data = _read_json_config(filter_json_file)
  if filter_data:
    print(f"  Loaded filter data: {filter_data}")
    failing_tests = ':'.join(filter_data.get('failing_tests', []))
    if failing_tests:
      gtest_filters += ':-' + failing_tests
    print(f"  gtest_filters = {gtest_filters}")
  else:
    print('  This gtest_target does not have gtest_filters specified')
  return gtest_filters


def _process_test_requests(args):
  """Processes test requests from the given arguments.

  Constructs a list of test requests based on the provided arguments,
  including test arguments, command arguments, files, parameters,
  and device information.

  Args:
      args: The parsed command-line arguments.

  Returns:
      A list of test request dictionaries.
  """
  test_requests = []
  platform_data = _read_json_config(args.platform_json)

  tests_args = [
      f'job_timeout_secs={args.job_timeout_secs}',
      f'test_timeout_secs={args.test_timeout_secs}',
      f'start_timeout_secs={args.start_timeout_secs}'
  ]

  if args.change_id:
    tests_args.append(f'change_id={args.change_id}')
  if args.test_attempts:
    tests_args.append(f'test_attempts={args.test_attempts}')
  if args.dimension:
    tests_args.extend(
        f'dimension_{dimension}' for dimension in args.dimension
    )

  base_params = [f'push_files=test_runtime_deps:{_CHROMIUM_TEST_ROOT}']

  if args.gcs_result_path:
    base_params.append(f'gcs_result_path={args.gcs_result_path}')

  for gtest_target in platform_data['gtest_targets']:
    print(f"  Processing gtest_target: {gtest_target}")

    apk_file = f"{args.gcs_archive_path}/{gtest_target}-debug.apk"
    test_runtime_deps = f"{args.gcs_archive_path}/{gtest_target}_deps.tar.gz"

    files = [
        f'test_apk={apk_file}',
        f'build_apk={apk_file}',
        f'test_runtime_deps={test_runtime_deps}'
    ]

    gtest_params = [
        f'gtest_xml_file_on_device={_ON_DEVICE_OUTPUT_FILES_DIR}/{gtest_target}_result.xml',
        f'gcs_result_filename={gtest_target}_result.xml',
        f'gcs_log_filename={gtest_target}_log.txt'
    ]

    gtest_filter = _get_gtest_filters(args.filter_json_dir, gtest_target)

    test_cmd_args = [
        f'command_line_args=--gtest_output=xml:{_ON_DEVICE_OUTPUT_FILES_DIR}/{gtest_target}_result.xml',
        f'--gtest_filter={gtest_filter}'
    ]

    test_request = {
        'test_args': tests_args,
        'test_cmd_args': test_cmd_args,
        'files': files,
        'params': base_params + gtest_params,
        'device_type': platform_data.get('test_dimensions', {}).get(
            'gtest_device'
        ),
        'device_pool': platform_data.get('test_dimensions', {}).get(
            'gtest_lab'
        ),
    }

    test_requests.append(test_request)
    print(f'  Created test_request: {test_request}')

  print(f'test_requests: {test_requests}')
  return test_requests


def main() -> int:  # Add a return type hint
  """Main routine for the on-device tests gateway client."""

  logging.basicConfig(
      level=logging.INFO, format='[%(filename)s:%(lineno)s] %(message)s'
  )
  print('Starting main routine')

  parser = argparse.ArgumentParser(
      description='Client for interacting with the On-Device Tests gateway.',
      epilog=(
          'Example:'
          'python3 -u cobalt/tools/on_device_tests_gateway_client.py'
          '--platform_json "${GITHUB_WORKSPACE}/src/.github/config/'
          '${{ matrix.platform}}.json"'
          '--filter_json_dir "${GITHUB_WORKSPACE}/src/cobalt/testing/'
          '${{ matrix.platform}}"'
          '--token ${GITHUB_TOKEN}'
          '--change_id ${GITHUB_PR_NUMBER:-postsubmit}'
          '--label builder-${{ matrix.platform }}'
          '--label builder_url-${GITHUB_RUN_URL}'
          '--label github'
          '--label ${GITHUB_EVENT_NAME}'
          '--label ${GITHUB_WORKFLOW}'
          '--label actor-${GITHUB_ACTOR}'
          '--label actor_id-${GITHUB_ACTOR_ID}'
          '--label triggering_actor-${GITHUB_TRIGGERING_ACTOR}'
          '--label sha-${GITHUB_SHA}'
          '--label repository-${GITHUB_REPO}'
          '--label author-${GITHUB_PR_HEAD_USER_LOGIN:-'
          '$GITHUB_COMMIT_AUTHOR_USERNAME}'
          '--label author_id-${GITHUB_PR_HEAD_USER_ID:-'
          '$GITHUB_COMMIT_AUTHOR_EMAIL}'
          '--dimension host_name=regex:maneki-mhserver-05.*'
          '${DIMENSION:+"--dimension" "$DIMENSION"}'
          '${ON_DEVICE_TEST_ATTEMPTS:+"--test_attempts" '
          '"$ON_DEVICE_TEST_ATTEMPTS"}'
          '--gcs_archive_path "${GCS_ARTIFACTS_PATH}"'
          '--gcs_result_path "${GCS_RESULTS_PATH}"'
          'trigger'
      ),
      formatter_class=argparse.RawDescriptionHelpFormatter,
  )

  # Authentication
  parser.add_argument(
      '-t',
      '--token',
      type=str,
      required=True,
      help='On Device Tests authentication token',
  )

  # General options
  parser.add_argument(
      '-i',
      '--change_id',
      type=str,
      help=(
          'ChangeId that triggered this test, if any. Saved with performance'
          ' test results.'
      ),
  )

  subparsers = parser.add_subparsers(
      dest='action', help='On-Device tests commands', required=True
  )

  # Trigger command
  trigger_parser = subparsers.add_parser(
      'trigger',
      help='Trigger On-Device tests',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  )

  # Group trigger arguments
  trigger_args = trigger_parser.add_argument_group('Trigger Arguments')
  trigger_args.add_argument(
      '-pf',
      '--platform_json',
      type=str,
      required=True,
      help='Platform-specific JSON file containing the list of target tests.',
  )
  trigger_parser.add_argument(
      '--filter_json_dir',
      type=str,
      required=True,
      help='Directory containing filter JSON files for test selection.',
  )
  trigger_parser.add_argument(
      '-l',
      '--label',
      type=str,
      action='append',
      help='Additional labels to assign to the test.',
  )
  trigger_parser.add_argument(
      '--dimension',
      type=str,
      action='append',
      help=(
          'On-Device Tests dimension used to select a device. '
          'Must have the following form: <dimension>=<value>. '
          'E.G. "release_version=regex:10.*"'
      ),
  )
  trigger_parser.add_argument(
      '--test_attempts',
      type=str,
      default='1',
      help='The maximum number of times a test can retry.',
  )
  trigger_args.add_argument(
      '-a',
      '--gcs_archive_path',
      type=str,
      required=True,
      help='Path to Chrobalt archive to be tested. Must be on GCS.',
  )
  trigger_parser.add_argument(
      '--gcs_result_path',
      type=str,
      help='GCS URL where test result files should be uploaded.',
  )
  trigger_parser.add_argument(
      '--job_timeout_secs',
      type=str,
      default='1800',
      help='Timeout in seconds for the job (default: 1800 seconds).',
  )
  trigger_parser.add_argument(
      '--test_timeout_secs',
      type=str,
      default='1800',
      help='Timeout in seconds for the test (default: 1800 seconds).',
  )
  trigger_parser.add_argument(
      '--start_timeout_secs',
      type=str,
      default='180',
      help='Timeout in seconds for the test to start (default: 180 seconds).',
  )

  # Watch command
  watch_parser = subparsers.add_parser(
      'watch', help='Watch a previously triggered On-Device test'
  )
  watch_parser.add_argument(
      'session_id',
      type=str,
      help=(
          'Session ID of a previously triggered Mobile Harness test. '
          'The test will be watched until it completes.'
      ),
  )

  args = parser.parse_args()

  if not args.platform_json:
    print(
        "Error: The '--platform_json' argument is required. "
        'Please provide the path to the Platform JSON file.'
    )
    exit(1)  # Exit with an error code
  if not args.filter_json_dir:
    print(
        "Error: The '--filter_json_dir' argument is required. "
        'Please provide the directory containing filter JSON files.'
    )
    exit(1)  # Exit with an error code
  if not args.gcs_archive_path:
    print(
        "Error: The '--gcs_archive_path' argument is required. "
        'Please provide the GCS archive path info.'
    )
    exit(1)  # Exit with an error code

  test_requests = _process_test_requests(args)
  client = OnDeviceTestsGatewayClient()

  try:
    if args.action == 'trigger':
      client.run_trigger_command(args, test_requests)
    else:
      client.run_watch_command(workdir=_WORK_DIR, args=args)
  except grpc.RpcError as e:
    logging.exception('gRPC error occurred:')  # Log the full traceback
    return e.code().value  # Return the error code

  return 0  # Indicate successful execution


if __name__ == '__main__':
  sys.exit(main())
