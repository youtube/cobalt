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
import os
import sys
from typing import List

import grpc
import on_device_tests_gateway_pb2
import on_device_tests_gateway_pb2_grpc

_WORK_DIR = '/on_device_tests_gateway'

# For local testing, set: _ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = ('localhost')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = (
    'on-device-tests-gateway-service.on-device-tests.svc.cluster.local')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '50052'

# These paths are hardcoded in various places. DO NOT CHANGE!
_DIR_ON_DEVICE = '/sdcard/Download'
_DEPS_ARCHIVE = '/sdcard/chromium_tests_root/deps.tar.gz'


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

  def run_trigger_command(self, token: str, labels: List[str], test_requests):
    """Calls On-Device Tests service and passing given parameters to it.

    Args:
        args (Namespace): Arguments passed in command line.
        test_requests (list): A list of test requests.
    """
    for response_line in self.stub.exec_command(
        on_device_tests_gateway_pb2.OnDeviceTestsCommand(
            token=token,
            labels=labels,
            test_requests=test_requests,
        )):

      print(response_line.response)

  def run_watch_command(self, token: str, session_id: str):
    """Calls On-Device Tests watch service and passing given parameters to it.

    Args:
        args (Namespace): Arguments passed in command line.
    """
    for response_line in self.stub.exec_watch_command(
        on_device_tests_gateway_pb2.OnDeviceTestsWatchCommand(
            token=token,
            session_id=session_id,
        )):

      print(response_line.response)


def _get_gtest_filters(filter_json_dir, target_name):
  """Retrieves gtest filters for a given target.

  Args:
      filter_json_dir: Directory containing filter JSON files.
      target_name: The name of the gtest target.

  Returns:
      A string containing the gtest filters.
  """
  gtest_filters = '*'
  filter_json_file = os.path.join(filter_json_dir, f'{target_name}_filter.json')
  print(f'  gtest_filter_json_file = {filter_json_file}')
  if os.path.exists(filter_json_file):
    with open(filter_json_file, 'r', encoding='utf-8') as f:
      filter_data = json.load(f)
      print(f'  Loaded filter data: {filter_data}')
      failing_tests = ':'.join(filter_data.get('failing_tests', []))
      if failing_tests:
        gtest_filters = '-' + failing_tests
  print(f'  gtest_filters = {gtest_filters}')
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

  for gtest_target in args.targets.split(','):
    _, target_name = gtest_target.split(':')
    print(f'  Processing gtest_target: {gtest_target}')

    tests_args = [
        f'job_timeout_sec={args.job_timeout_sec}',
        f'test_timeout_sec={args.test_timeout_sec}',
        f'start_timeout_sec={args.start_timeout_sec}'
    ]
    if args.test_attempts:
      tests_args.append(f'test_attempts={args.test_attempts}')

    if args.dimensions:
      dimensions = json.loads(args.dimensions)
      # Pop mandatory dimensions for special handling.
      device_type = dimensions.pop('device_type')
      device_pool = dimensions.pop('device_pool')
      tests_args += [f'dimension_{key}={value}' for key, value in dimensions]
    else:
      raise RuntimeError('Dimensions not specified: device_type, device_pool')

    gtest_filter = _get_gtest_filters(args.filter_json_dir, target_name)
    command_line_args = ' '.join([
        f'--gtest_output=xml:{_DIR_ON_DEVICE}/{target_name}_result.xml',
        f'--gtest_filter={gtest_filter}',
    ])
    test_cmd_args = [f'command_line_args={command_line_args}']

    files = [
        f'test_apk={args.gcs_archive_path}/{target_name}-debug.apk',
        f'build_apk={args.gcs_archive_path}/{target_name}-debug.apk',
        f'test_runtime_deps={args.gcs_archive_path}/{target_name}_deps.tar.gz',
    ]

    params = []
    if args.gcs_result_path:
      params.append(f'gcs_result_path={args.gcs_result_path}')
    params += [
        f'push_files=test_runtime_deps:{_DEPS_ARCHIVE}',
        f'gtest_xml_file_on_device={_DIR_ON_DEVICE}/{target_name}_result.xml',
        f'gcs_result_filename={target_name}_result.xml',
        f'gcs_log_filename={target_name}_log.txt'
    ]

    test_requests.append({
        'test_args': tests_args,
        'test_cmd_args': test_cmd_args,
        'files': files,
        'params': params,
        'device_type': device_type,
        'device_pool': device_pool,
    })
  return test_requests


def main() -> int:
  """Main routine for the on-device tests gateway client."""

  logging.basicConfig(
      level=logging.INFO, format='[%(filename)s:%(lineno)s] %(message)s')
  print('Starting main routine')

  parser = argparse.ArgumentParser(
      description='Client for interacting with the On-Device Tests gateway.',
      epilog=('Example:'
              'python3 -u cobalt/tools/on_device_tests_gateway_client.py'
              '--platform_json "${GITHUB_WORKSPACE}/src/.github/config/'
              '${{ matrix.platform}}.json"'
              '--filter_json_dir "${GITHUB_WORKSPACE}/src/cobalt/testing/'
              '${{ matrix.platform}}"'
              '--token ${GITHUB_TOKEN}'
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
              'trigger'),
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
  subparsers = parser.add_subparsers(
      dest='action', help='On-Device tests commands', required=True)

  # Trigger command
  trigger_parser = subparsers.add_parser(
      'trigger',
      help='Trigger On-Device tests',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  )

  # Group trigger arguments
  trigger_args = trigger_parser.add_argument_group('Trigger Arguments')
  trigger_parser.add_argument(
      '--targets',
      type=str,
      required=True,
      help='List of targets to test, comma separated. Must be fully qualified '
      'ninja target.',
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
      '--dimensions',
      type=str,
      help='On-Device Tests dimensions as JSON key-values.',
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
      '--job_timeout_sec',
      type=str,
      default='1800',
      help='Timeout in seconds for the job (default: 1800 seconds).',
  )
  trigger_parser.add_argument(
      '--test_timeout_sec',
      type=str,
      default='1800',
      help='Timeout in seconds for the test (default: 1800 seconds).',
  )
  trigger_parser.add_argument(
      '--start_timeout_sec',
      type=str,
      default='900',
      help='Timeout in seconds for the test to start (default: 900 seconds).',
  )

  # Watch command
  watch_parser = subparsers.add_parser(
      'watch', help='Watch a previously triggered On-Device test')
  watch_parser.add_argument(
      'session_id',
      type=str,
      help=('Session ID of a previously triggered Mobile Harness test. '
            'The test will be watched until it completes.'),
  )

  args = parser.parse_args()
  test_requests = _process_test_requests(args)

  client = OnDeviceTestsGatewayClient()
  try:
    if args.action == 'trigger':
      client.run_trigger_command(args.token, args.label, test_requests)
    else:
      client.run_watch_command(args.token, args.session_id)
  except grpc.RpcError as e:
    logging.exception('gRPC error occurred:')  # Log the full traceback
    return e.code().value  # Return the error code

  return 0  # Indicate successful execution


if __name__ == '__main__':
  sys.exit(main())
