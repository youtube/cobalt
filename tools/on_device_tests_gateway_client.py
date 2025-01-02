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
import os
import json

import grpc

import on_device_tests_gateway_pb2
import on_device_tests_gateway_pb2_grpc

_WORK_DIR = '/on_device_tests_gateway'

# Comment out the next three lines for local testing
#_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = (
#    'on-device-tests-gateway-service.on-device-tests.svc.cluster.local')
#_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '50052'

# Uncomment the next two lines for local testing
_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = ('localhost')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '12345'


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

  def run_trigger_command(self, workdir: str, args: argparse.Namespace, apk_tests = None):
    """Calls On-Device Tests service and passing given parameters to it.

    Args:
        workdir (str): Current script workdir.
        args (Namespace): Arguments passed in command line.
    """
    for response_line in self.stub.exec_command(
        on_device_tests_gateway_pb2.OnDeviceTestsCommand(
            workdir=workdir,
            token=args.token,
            platform=args.platform,
            archive_path=args.archive_path,
            labels=args.label,
            gcs_result_path=args.gcs_result_path,
            change_id=args.change_id,
            dry_run=args.dry_run,
            dimension=args.dimension or [],
            test_attempts=args.test_attempts,
            retry_level=args.retry_level,
            apk_tests=apk_tests,
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
  """
  Reads and parses data from a JSON configuration file.

  Args:
    filename: The name of the JSON configuration file.

  Returns:
    A list of dictionaries, where each dictionary represents a test configuration.
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

def _default_platform_json_file(platform):
  current_dir = os.path.dirname(os.path.abspath(__file__))
  relative_path = os.path.join("..", ".github", "config", platform + ".json")
  return os.path.join(current_dir, relative_path)

def _get_tests_filter_json_file(platform, gtest_target):
  current_dir = os.path.dirname(os.path.abspath(__file__))
  relative_path = os.path.join("..", "cobalt", "testing", platform, gtest_target + "_filter.json")
  return os.path.join(current_dir, relative_path)

def _process_apk_tests(args):
  apk_tests = []
  platform_json_file = args.platform_json if args.platform_json else _default_platform_json_file(args.platform)
  print(f" The platform_json_file is '{platform_json_file}'")
  platform_json_file_data = _read_json_config(platform_json_file)
  gtest_device = platform_json_file_data["gtest_device"]
  gtest_lab = platform_json_file_data["gtest_lab"]
  gtest_blaze_target = "//experimental/cobalt/chrobalt_poc:custom_chrobalt_unit_tests_" + gtest_device
  default_gtest_filters = "*"

  #print(' processing gtest_targets in json file')
  for gtest_target in platform_json_file_data["gtest_targets"]:
    apk_test = {
        "test_target": gtest_blaze_target,
        "device_model": gtest_device,
        "device_pool": gtest_lab
    }
    print(f"  gtest_target: {gtest_target}")
    gtest_filter_json_file = _get_tests_filter_json_file(args.platform, gtest_target)
    print(f"    gtest_filter_json_file = {gtest_filter_json_file}")
    gtest_filter_json_file_data = _read_json_config(gtest_filter_json_file)
    gtest_filters = default_gtest_filters
    if gtest_filter_json_file_data is None:
      print("   This gtest_target does not have gtest_filters specified");
    else:
      failing_tests_list = gtest_filter_json_file_data["failing_tests"]
      #passing_tests_list = gtest_filter_json_file_data["passing_tests"]
      failing_tests_string = ":".join(failing_tests_list)
      #passing_tests_string = ":".join(passing_tests_list)
      #if passing_tests_string:
      #  gtest_filters += ":" + passing_tests_string
      if failing_tests_string:
        gtest_filters += ":-" + failing_tests_string
      print(f"    gtest_filters = {gtest_filters}");

    #apk_test["apk_path"] = "/bigstore/yt-temp/" + gtest_target + "-debug.apk"
    apk_test["apk_path"] = "/bigstore/yt-temp/base_unittests-debug.apk"
    apk_test["gtest_filters"] = gtest_filters
    apk_tests.append(apk_test)

  print(f" apk_tests: {apk_tests}")
  return apk_tests

def main():
  """Main routine."""
  print('Starting main routine')

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
      '-p',
      '--platform',
      type=str,
      required=True,
      help='Platform this test was built for.')
  trigger_parser.add_argument(
      '-pf',
      '--platform_json',
      type=str,
      help='Platform-specific json file containing the list of target tests.')
  trigger_parser.add_argument(
      '-a',
      '--archive_path',
      type=str,
      required=True,
      help='Path to Chrobalt archive to be tested. Must be on gcs.')
  trigger_parser.add_argument(
      '-l',
      '--label',
      type=str,
      default=[],
      action='append',
      help='Additional labels to assign to the test.')
  trigger_parser.add_argument(
    '--gcs_result_path',
    type=str,
    help='GCS url where test result files should be uploaded.')
  trigger_parser.add_argument(
      '--dimension',
      type=str,
      action='append',
      help='On-Device Tests dimension used to select a device. '
      'Must have the following form: <dimension>=<value>.'
      ' E.G. "release_version=regex:10.*')
  trigger_parser.add_argument(
      '--test_attempts',
      type=str,
      default='1',
      required=False,
      help='The maximum number of times a test could retry.')
  trigger_parser.add_argument(
      '--retry_level',
      type=str,
      default='ERROR',
      required=False,
      help='The retry level of Mobile harness job. Either ERROR (to retry for '
      'MH errors) or FAIL (to retry for failing tests). Setting retry_level to '
      'FAIL will also retry for MH errors.')

  watch_parser = subparsers.add_parser('watch', help='Trigger On-Device tests')
  watch_parser.add_argument(
      'session_id',
      type=str,
      help='Session id of a previously triggered mobile '
      'harness test. If passed, the test will not be '
      'triggered, but will be watched until the exit '
      'status is reached.')

  args = parser.parse_args()
  apk_tests = _process_apk_tests(args)

  client = OnDeviceTestsGatewayClient()
  try:
    if args.action == 'trigger':
      client.run_trigger_command(workdir=_WORK_DIR, args=args, apk_tests=apk_tests)
    else:
      client.run_watch_command(workdir=_WORK_DIR, args=args)
  except grpc.RpcError as e:
    print(e)
    return e.code().value


if __name__ == '__main__':
  sys.exit(main())
