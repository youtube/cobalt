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
import logging
import sys

import grpc

import on_device_tests_gateway_pb2
import on_device_tests_gateway_pb2_grpc

# All tests On-Device tests support
_TEST_TYPES = [
    'black_box_test',
    'evergreen_test',
    'unit_test',
]

# All test configs On-Device tests support
_TEST_CONFIGS = [
    'devel',
    'staging',
    'production',
]

_WORK_DIR = '/on_device_tests_gateway'
_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = (
    'on-device-tests-gateway-service.on-device-tests.svc.cluster.local')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '50052'


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

  def run_trigger_command(self, workdir: str, args: argparse.Namespace):
    """Calls On-Device Tests service and passing given parameters to it.

    Args:
        workdir (str): Current script workdir.
        args (Namespace): Arguments passed in command line.
    """
    for response_line in self.stub.exec_command(
        on_device_tests_gateway_pb2.OnDeviceTestsCommand(
            workdir=workdir,
            token=args.token,
            test_type=args.test_type,
            platform=args.platform,
            archive_path=args.archive_path,
            config=args.config,
            tag=args.tag,
            labels=args.label,
            builder_name=args.builder_name,
            change_id=args.change_id,
            build_number=args.build_number,
            loader_platform=args.loader_platform,
            loader_config=args.loader_config,
            version=args.version,
            dry_run=args.dry_run,
            dimension=args.dimension or [],
            unittest_shard_index=args.unittest_shard_index,
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


def main():
  """Main routine."""
  logging.basicConfig(
      level=logging.INFO, format='[%(filename)s:%(lineno)s] %(message)s')

  parser = argparse.ArgumentParser(
      epilog=('Example: ./on_device_tests_gateway_client.py '
              '--test_type=unit_test '
              '--remote_archive_path=gs://my_bucket/path/to/artifacts.tar '
              '--platform=raspi-2 '
              '--config=devel'),
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '-t',
      '--token',
      type=str,
      required=True,
      help='On Device Tests authentication token')
  parser.add_argument(
      '--dry_run',
      action='store_true',
      help='Specifies to show what would be done without actually doing it.')
  parser.add_argument(
      '-i',
      '--change_id',
      type=str,
      help='ChangeId that triggered this test, if any. '
      'Saved with performance test results.')

  subparsers = parser.add_subparsers(
      dest='action', help='On-Device tests commands', required=True)
  trigger_parser = subparsers.add_parser(
      'trigger', help='Trigger On-Device tests')

  trigger_parser.add_argument(
      '-e',
      '--test_type',
      type=str,
      choices=_TEST_TYPES,
      required=True,
      help='Type of test to run.')
  trigger_parser.add_argument(
      '-p',
      '--platform',
      type=str,
      required=True,
      help='Platform this test was built for.')
  trigger_parser.add_argument(
      '-c',
      '--config',
      type=str,
      choices=_TEST_CONFIGS,
      required=True,
      help='Cobalt config being tested.')
  trigger_parser.add_argument(
      '-a',
      '--archive_path',
      type=str,
      required=True,
      help='Path to Cobalt archive to be tested. Must be on gcs.')
  trigger_parser.add_argument(
      '-g',
      '--tag',
      type=str,
      help='Value saved with performance results. '
      'Indicates why this test was triggered.')
  trigger_parser.add_argument(
      '-l',
      '--label',
      type=str,
      default=[],
      action='append',
      help='Additional labels to assign to the test.')
  trigger_parser.add_argument(
      '-b',
      '--builder_name',
      type=str,
      help='Name of the builder that built the artifacts, '
      'if any. Saved with performance test results')
  trigger_parser.add_argument(
      '-n',
      '--build_number',
      type=str,
      help='Build number associated with the build, if any. '
      'Saved with performance test results.')
  trigger_parser.add_argument(
      '--loader_platform',
      type=str,
      help='Platform of the loader to run the test. Only '
      'applicable in Evergreen mode.')
  trigger_parser.add_argument(
      '--loader_config',
      type=str,
      help='Cobalt config of the loader to run the test. Only '
      'applicable in Evergreen mode.')
  trigger_parser.add_argument(
      '--dimension',
      type=str,
      action='append',
      help='On-Device Tests dimension used to select a device. '
      'Must have the following form: <dimension>=<value>.'
      ' E.G. "release_version=regex:10.*')
  trigger_parser.add_argument(
      '--version',
      type=str,
      default='COBALT',
      help='Cobalt version being tested.')
  trigger_parser.add_argument(
      '--unittest_shard_index',
      type=str,
      required=False,
      help='Optional argument to specify which unit testing shard to run in '
      'the On-Device Tests Job. Defaults behavior is to run all tests without '
      'sharding enabled.')

  watch_parser = subparsers.add_parser('watch', help='Trigger On-Device tests')
  watch_parser.add_argument(
      'session_id',
      type=str,
      help='Session id of a previously triggered mobile '
      'harness test. If passed, the test will not be '
      'triggered, but will be watched until the exit '
      'status is reached.')

  args = parser.parse_args()

  client = OnDeviceTestsGatewayClient()
  try:
    if args.action == 'trigger':
      client.run_trigger_command(workdir=_WORK_DIR, args=args)
    else:
      client.run_watch_command(workdir=_WORK_DIR, args=args)
  except grpc.RpcError as e:
    print(e)
    return e.code().value


if __name__ == '__main__':
  sys.exit(main())
