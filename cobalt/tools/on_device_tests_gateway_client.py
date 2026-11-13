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
from typing import Any, Dict, List, Optional, Tuple

import grpc

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
if _REPO_ROOT not in sys.path:
  sys.path.insert(0, _REPO_ROOT)
_DEVINFRA_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', 'devinfra', 'github'))
if _DEVINFRA_DIR not in sys.path:
  sys.path.insert(0, _DEVINFRA_DIR)

# pylint: disable=wrong-import-position
try:
  from cobalt.devinfra.github.test_filter import get_gtest_filter
except ImportError:
  try:
    from cobalt.tools.test_filter import get_gtest_filter
  except ImportError:
    from test_filter import get_gtest_filter

import on_device_tests_gateway_pb2
import on_device_tests_gateway_pb2_grpc
# pylint: enable=wrong-import-position

_WORK_DIR = '/on_device_tests_gateway'

_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = (
    'on-device-tests-gateway-service.on-device-tests.svc.cluster.local')

# When testing with local gateway, uncomment:
#_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = 'localhost'
_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '50052'

# These paths are hardcoded in various places. DO NOT CHANGE!
_DIR_ON_DEV_MAP = {
    'android': '/sdcard/Download',
    'rdk': '/data/test/results',
}

_DEPS_ARCH_MAP = {
    'android': '/sdcard/chromium_tests_root/deps.tar.gz',
    'rdk': '/data/test/',
}
_GCS_ARCHIVE_DEVICE_FAMILIES = ('rdk',)
_MAX_SHARDS = 10


def _is_android_gtest_target(args: argparse.Namespace,
                             target_name: str) -> bool:
  """Returns True when target_name is a non-JUnit Android GTest suite."""
  return args.device_family == 'android' and 'junit' not in target_name.lower()


class OnDeviceTestsGatewayClient:
  """On-device tests Gateway Client class."""

  def __init__(self):
    self.channel = grpc.insecure_channel(
        target=(f'{_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST}:'
                f'{_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT}'),
        # These options need to match server settings.
        options=[
            ('grpc.keepalive_time_ms', 10000),
            ('grpc.keepalive_timeout_ms', 5000),
            ('grpc.keepalive_permit_without_calls', 1),
            ('grpc.http2.max_pings_without_data', 0),
            ('grpc.http2.min_time_between_pings_ms', 10000),
            ('grpc.http2.min_ping_interval_without_data_ms', 5000),
        ],
    )
    self.stub = on_device_tests_gateway_pb2_grpc.on_device_tests_gatewayStub(
        self.channel)

  def run_trigger_command(self, token: str, labels: List[str],
                          test_requests: List[Dict[str, Any]]) -> None:
    """Calls On-Device Tests service and passing given parameters to it.

    Args:
        token: Authentication token.
        labels: List of labels to assign to the test.
        test_requests (list): A list of test requests.

    Returns:
        None.
    """
    for response_line in self.stub.exec_command(
        on_device_tests_gateway_pb2.OnDeviceTestsCommand(
            token=token,
            labels=labels,
            test_requests=test_requests,
        )):

      print(response_line.response)

  def run_watch_command(self, token: str, session_id: str) -> None:
    """Calls On-Device Tests watch service and passing given parameters to it.

    Args:
        token: Authentication token.
        session_id: Session ID of a previously triggered Mobile Harness test.

    Returns:
        None.
    """
    for response_line in self.stub.exec_watch_command(
        on_device_tests_gateway_pb2.OnDeviceTestsWatchCommand(
            token=token,
            session_id=session_id,
        )):

      print(response_line.response)


def _get_test_args_and_dimensions(
    args: argparse.Namespace,
) -> Tuple[List[str], Optional[str], Optional[str]]:
  """Prepares test_args, device_type and device_pool based on args."""

  test_args = [
      f'job_timeout_sec={args.job_timeout_sec}',
      f'test_timeout_sec={args.test_timeout_sec}',
      f'start_timeout_sec={args.start_timeout_sec}',
  ]

  if args.retry_level:
    test_args.extend([
        f'retry_level={args.retry_level}',
    ])

  if args.test_attempts:
    test_args.extend([
        f'test_attempts={args.test_attempts}',
    ])

  device_type = None
  device_pool = None

  if args.dimensions:
    try:
      dimensions = json.loads(args.dimensions)
    except json.JSONDecodeError as e:
      raise ValueError(f'--dimensions is not in JSON format: {e}') from e
    device_type = dimensions.pop('device_type', None)
    device_pool = dimensions.pop('device_pool', None)

    test_args.extend(
        [f'dimension_{key}={value}' for key, value in dimensions.items()])

  return test_args, device_type, device_pool


def _unit_test_files(args: argparse.Namespace, target_name: str) -> List[str]:
  """Builds the list of files for a unit test request."""
  # TODO: b/432536319 - Use flag to determine file ending.

  if args.device_family == 'android':
    res = [
        f'test_apk={args.gcs_archive_path}/{target_name}-debug.apk',
        f'build_apk={args.gcs_archive_path}/{target_name}-debug.apk',
        f'test_runtime_deps={args.gcs_archive_path}/{target_name}_deps.tar.gz',
    ]

    if _is_android_gtest_target(args, target_name):
      res.append(f'host_deps={args.gcs_archive_path}/'
                 'android_test_host_deps.tar.gz')
    return res

  elif args.device_family in _GCS_ARCHIVE_DEVICE_FAMILIES:
    return [
        f'bin={args.gcs_archive_path}/{target_name}.py',
        f'test_runtime_deps={args.gcs_archive_path}/{target_name}_deps.tar.gz',
    ]
  else:
    raise ValueError(f'Unsupported device family: {args.device_family}')


def _unit_test_params(
    args: argparse.Namespace,
    target_name: str,
    dir_on_device: str,
    gcs_result_path: Optional[str] = None,
    test_attempts: Optional[str] = None,
) -> List[str]:
  """Builds the list of params for a unit test request."""
  runtime_deps = _DEPS_ARCH_MAP.get(args.device_family, '')
  params = [
      f'push_files=test_runtime_deps:{runtime_deps}',
      f'gtest_xml_file_on_device={dir_on_device}/{target_name}_testoutput.xml',
      f'gcs_result_filename={target_name}_testoutput.xml',
      f'gcs_log_filename={target_name}_log.txt',
  ]
  resolved_gcs_result_path = (
      gcs_result_path if gcs_result_path is not None else args.gcs_result_path)
  if resolved_gcs_result_path:
    params.append(f'gcs_result_path={resolved_gcs_result_path}')
  effective_test_attempts = (
      test_attempts if test_attempts is not None else args.test_attempts)
  if effective_test_attempts:
    try:
      if int(effective_test_attempts) > 1:
        # Must delete existing results when retries are enabled.
        params.append('gcs_delete_before_upload=true')
    except ValueError:
      logging.warning('Invalid test_attempts value: %s',
                      effective_test_attempts)

  return params


def _with_target_test_attempts(base_test_args: List[str],
                               target_test_attempts: str) -> List[str]:
  """Returns a copy of base_test_args with optional target test_attempts."""
  if not target_test_attempts:
    return list(base_test_args)
  filtered_args = [
      arg for arg in base_test_args if not arg.startswith('test_attempts=')
  ]
  filtered_args.append(f'test_attempts={target_test_attempts}')
  return filtered_args


def _parse_num_shards(target_data: Dict[str, Any], test_target: str) -> int:
  """Parses and validates the optional per-target shard count."""
  raw_shards = target_data.get('shards')
  if raw_shards is None:
    return 1
  if isinstance(raw_shards, (bool, float)):
    raise ValueError(f'Invalid shards value for {test_target}: {raw_shards}')
  try:
    num_shards = int(raw_shards)
  except (TypeError, ValueError) as exc:
    raise ValueError(
        f'Invalid shards value for {test_target}: {raw_shards}') from exc
  if not 1 <= num_shards <= _MAX_SHARDS:
    raise ValueError(f'shards for {test_target} must be between 1 and '
                     f'{_MAX_SHARDS}, got {num_shards}')
  return num_shards


def _has_shard_retry_filter(filter_json_dir: Optional[str], target_name: str,
                            shard_index: int, gtest_filter: str) -> bool:
  """Returns True when retrying explicit failed tests from a shard filter."""
  if not filter_json_dir:
    return False
  shard_filter_file = os.path.join(filter_json_dir,
                                   f'{target_name}_{shard_index}_filter.json')
  return (os.path.exists(shard_filter_file) and
          not gtest_filter.startswith('-') and gtest_filter != '*')


def _shard_cmd_args(test_type: str,
                    shard_index: int,
                    num_shards: int,
                    *,
                    use_host_runner: bool = False) -> List[str]:
  """Builds test runner sharding flags for a shard."""
  if test_type != 'browser_test' and not use_host_runner:
    raise ValueError(
        'Sharding is only supported for browser_test or Android host runner '
        f'targets, got {test_type}')
  return [
      f'--test-launcher-shard-index={shard_index}',
      f'--test-launcher-total-shards={num_shards}',
  ]


def process_test_requests(args: argparse.Namespace) -> List[Dict[str, Any]]:
  """Builds the list of test requests based on the test type."""
  base_test_args, device_type, device_pool = _get_test_args_and_dimensions(args)
  test_requests = []

  try:
    targets = json.loads(args.targets)
  except json.JSONDecodeError as e:
    raise ValueError(f'--targets is not in JSON format: {e}') from e

  for target_data in targets:
    test_type = args.test_type

    if test_type in ('unit_test', 'browser_test'):
      if not device_type or not device_pool:
        raise ValueError('Dimensions not specified: device_type, device_pool')
      target_gtest_filter = ''
      target_test_attempts = ''
      num_shards = 1
      if isinstance(target_data, dict):
        test_target = target_data['target']
        target_test_attempts = str(target_data.get('test_attempts', '') or '')
        target_gtest_filter = target_data.get('gtest_filter', '')
        num_shards = _parse_num_shards(target_data, test_target)
      else:
        test_target = target_data

      target_test_args = _with_target_test_attempts(base_test_args,
                                                    target_test_attempts)
      effective_test_attempts = target_test_attempts or args.test_attempts
      target_name = test_target.split(':')[-1]
      use_host_runner = _is_android_gtest_target(args, target_name)
      if 'cobalt_browsertests' in target_name:
        test_type = 'browser_test'
      if num_shards > 1 and test_type != 'browser_test' and not use_host_runner:
        raise ValueError(
            'Sharding is only supported for browser_test or Android host '
            f'runner targets, got {test_type} for {test_target}')
      dir_on_device = _DIR_ON_DEV_MAP.get(args.device_family, '')
      files = _unit_test_files(args, target_name)

      if num_shards == 1:
        if args.gtest_filter:
          gtest_filter = args.gtest_filter
        elif target_gtest_filter:
          gtest_filter = target_gtest_filter
        else:
          gtest_filter = get_gtest_filter(args.filter_json_dir, target_name)
          if gtest_filter == '-*':
            print(f'Skipping {target_name} due to test filter.')
            continue
        cmd_args = [
            f'--gtest_output=xml:{dir_on_device}/{target_name}_testoutput.xml',
            f'--gtest_filter={gtest_filter}',
            '--single-process-tests',
            '--num-retries=0',
        ] + (['--disable-test-server'] if use_host_runner else [])
        command_line_args = ' '.join(cmd_args)
        params = _unit_test_params(
            args,
            target_name,
            dir_on_device,
            test_attempts=effective_test_attempts,
        )
        test_requests.append({
            'device_type': device_type,
            'device_pool': device_pool,
            'test_cmd_args': [f'command_line_args={command_line_args}'],
            'test_args': list(target_test_args),
            'files': list(files),
            'params': params,
            'test_target': test_target,
            'test_type': test_type,
        })
      else:
        base_result_path = (
            args.gcs_result_path.rstrip('/') if args.gcs_result_path else '')
        for shard_index in range(num_shards):
          use_shard_retry_filter = False
          if args.gtest_filter:
            gtest_filter = args.gtest_filter
          elif target_gtest_filter:
            gtest_filter = target_gtest_filter
          else:
            gtest_filter = get_gtest_filter(args.filter_json_dir, target_name,
                                            shard_index)
            if gtest_filter == '-*':
              print(f'Skipping {target_name} (shard {shard_index} of '
                    f'{num_shards}) due to test filter.')
              continue
            use_shard_retry_filter = _has_shard_retry_filter(
                args.filter_json_dir, target_name, shard_index, gtest_filter)

          xml_path_on_device = f'{dir_on_device}/{target_name}_testoutput.xml'
          cmd_args = [
              f'--gtest_output=xml:{xml_path_on_device}',
              f'--gtest_filter={gtest_filter}',
              '--single-process-tests',
              '--num-retries=0',
          ] + (['--disable-test-server'] if use_host_runner else [])
          if not use_shard_retry_filter:
            cmd_args.extend(
                _shard_cmd_args(
                    test_type,
                    shard_index,
                    num_shards,
                    use_host_runner=use_host_runner))
          command_line_args = ' '.join(cmd_args)

          shard_gcs_result_path = (f'{base_result_path}/shard_{shard_index}'
                                   if base_result_path else None)
          params = _unit_test_params(
              args,
              target_name,
              dir_on_device,
              gcs_result_path=shard_gcs_result_path,
              test_attempts=effective_test_attempts,
          )
          # Avoid '/' in the shard suffix because the gateway service derives
          # the session display label via test_target.split('/')[-1].
          shard_label = f'{test_target} (shard {shard_index} of {num_shards})'
          test_requests.append({
              'device_type': device_type,
              'device_pool': device_pool,
              'test_cmd_args': [f'command_line_args={command_line_args}'],
              'test_args': list(target_test_args),
              'files': list(files),
              'params': params,
              'test_target': shard_label,
              'test_type': test_type,
          })
      continue

    elif test_type in ('e2e_test', 'yts_test', 'yts_wpt_test'):
      if isinstance(target_data, dict):
        test_target = target_data.get('target', '')
        target_test_attempts = str(target_data.get('test_attempts', '') or '')
        num_shards = _parse_num_shards(target_data, test_target)
        if num_shards > 1:
          raise ValueError(
              f'Sharding is only supported for browser_test targets, got '
              f'{test_type} for {test_target}')
      else:
        test_target = target_data
        target_test_attempts = ''
      target_test_args = _with_target_test_attempts(base_test_args,
                                                    target_test_attempts)
      test_cmd_args = []
      files = []
      if test_type == 'yts_wpt_test':
        test_type = 'e2e_test'
      yt_binary_name = os.path.splitext(os.path.basename(args.artifact_name))[0]
      params = [f'yt_binary_name={yt_binary_name}']
      if args.device_family in _GCS_ARCHIVE_DEVICE_FAMILIES:
        params.append(f'gcs_cobalt_archive=gs://{args.cobalt_path}.zip')
      else:
        bigstore_path = f'/bigstore/{args.cobalt_path}/{args.artifact_name}'
        if test_type == 'yts_test':
          files.append(f'build_apk={bigstore_path}')
          params.append('app=dev.cobalt.coat')
        else:
          files.append(f'cobalt_path={bigstore_path}')

    else:
      raise ValueError(f'Unsupported test type: {test_type}')

    test_requests.append({
        'device_type': device_type,
        'device_pool': device_pool,
        'test_cmd_args': test_cmd_args,
        'test_args': target_test_args,
        'files': files,
        'params': params,
        'test_target': test_target,
        'test_type': test_type,
    })

  return test_requests


def main() -> int:
  """Main routine for the on-device tests gateway client."""

  logging.basicConfig(
      level=logging.INFO, format='[%(filename)s:%(lineno)s] %(message)s')

  print('Starting main routine')

  parser = argparse.ArgumentParser(
      description='Client for interacting with the On-Device Tests gateway.',
      formatter_class=argparse.RawDescriptionHelpFormatter,
  )

  # Authentication
  parser.add_argument(
      '-t',
      '--token',
      type=str,
      required=True,
      help='On Device Tests authentication (GitHub) token. Use GitHub cli '
      'command `gh auth token` to generate.',
  )
  subparsers = parser.add_subparsers(
      dest='action', help='On-Device tests commands', required=True)

  # Trigger command
  trigger_parser = subparsers.add_parser(
      'trigger',
      help='Trigger On-Device tests',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  )

  # --- Common Trigger Arguments ---
  trigger_args = trigger_parser.add_argument_group('Common Trigger Arguments')
  trigger_args.add_argument(
      '--test_type',
      type=str,
      required=True,
      choices=[
          'unit_test', 'e2e_test', 'yts_test', 'browser_test', 'yts_wpt_test'
      ],
      help='Type of test to run.',
  )
  trigger_args.add_argument(
      '--device_family',
      type=str,
      help='Family of device to run tests on.',
  )
  trigger_args.add_argument(
      '--targets',
      type=str,
      help='List of targets to test in JSON format.',
      required=True,
  )
  trigger_args.add_argument(
      '-l',
      '--label',
      type=str,
      action='append',
      help='Additional labels to assign to the test.',
  )
  trigger_args.add_argument(
      '--test_attempts',
      type=str,
      default='1',
      help='The maximum number of times a test can retry.',
  )
  trigger_args.add_argument(
      '--retry_level',
      type=str,
      choices=['ERROR', 'FAIL'],
      help='Retry level for failed tests.',
  )
  trigger_args.add_argument(
      '--dimensions',
      type=str,
      help='On-Device Tests dimensions as JSON key-values.',
  )
  trigger_args.add_argument(
      '--job_timeout_sec',
      type=str,
      default='2700',
      help='Timeout in seconds for the job. Must be set higher and '
      'start_timeout_sec and test_timeout_sec combined.',
  )
  trigger_args.add_argument(
      '--test_timeout_sec',
      type=str,
      default='1800',
      help='Timeout in seconds for the test.',
  )
  trigger_args.add_argument(
      '--start_timeout_sec',
      type=str,
      default='900',
      help='Timeout in seconds for the test to start.',
  )

  # --- Unit Test Arguments ---
  unit_test_group = trigger_parser.add_argument_group('Unit Test Arguments')
  unit_test_group.add_argument(
      '--filter_json_dir',
      type=str,
      help='Directory containing filter JSON files for test selection.',
  )
  unit_test_group.add_argument(
      '--gtest_filter',
      type=str,
      help='Explicit gtest filter string to run specific test cases.',
  )
  unit_test_group.add_argument(
      '-a',
      '--gcs_archive_path',
      type=str,
      help='Path to Cobalt archive to be tested. Must be on GCS.',
  )
  unit_test_group.add_argument(
      '--gcs_result_path',
      type=str,
      help='GCS URL where test result files should be uploaded.',
  )

  # --- E2E Test Arguments ---
  e2e_test_group = trigger_parser.add_argument_group('E2E Test Arguments')
  e2e_test_group.add_argument(
      '--cobalt_path',
      type=str,
      help='Path to Cobalt apk.',
  )
  e2e_test_group.add_argument(
      '--artifact_name',
      type=str,
      default='Cobalt.apk',
      help=('Artifact name, used to specify the cobalt path in non-evergreen'
            ' workflows'),
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

  client = OnDeviceTestsGatewayClient()

  try:
    if args.action == 'trigger':
      # TODO(b/428961033): Let argparse handle these checks as required
      # arguments.
      if args.test_type in ('e2e_test', 'yts_test'):
        if not args.cobalt_path:
          raise ValueError('--cobalt_path is required for e2e_test or yts_test')
      elif args.test_type in ('unit_test', 'browser_test'):
        if not args.device_family:
          raise ValueError(f'--device_family is required for {args.test_type}')
        if not args.gcs_archive_path:
          raise ValueError(
              f'--gcs_archive_path is required for {args.test_type}')
        if not args.gcs_result_path:
          raise ValueError(
              f'--gcs_result_path is required for {args.test_type}')
        if not args.filter_json_dir:
          raise ValueError(
              f'--filter_json_dir is required for {args.test_type}')

      test_requests = process_test_requests(args)
      if not test_requests:
        print('No tests to run.')
        return 0

      client.run_trigger_command(args.token, args.label, test_requests)
    else:
      client.run_watch_command(args.token, args.session_id)
  except grpc.RpcError as e:
    logging.error('gRPC error occurred: %s', e)
    return 1

  return 0  # Indicate successful execution


if __name__ == '__main__':
  sys.exit(main())
