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
_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = (
    'on-device-tests-gateway-service.on-device-tests.svc.cluster.local')
_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '50052'

# Uncomment the next two lines for local testing
#_ON_DEVICE_TESTS_GATEWAY_SERVICE_HOST = ('localhost')
#_ON_DEVICE_TESTS_GATEWAY_SERVICE_PORT = '12345'


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

def _get_platform_json_file(platform):
  """Constructs the path to the platform JSON configuration file.

  Args:
    platform: The name of the platform.

  Returns:
    The absolute path to the platform JSON file.
  """
  return os.path.join(
      os.path.dirname(os.path.abspath(__file__)), 
      "..", 
      ".github", 
      "config", 
      f"{platform}.json"
  )

def _get_tests_filter_json_file(platform, gtest_target):
  """Constructs the path to the gtest filter JSON file.

  Args:
    platform: The name of the platform.
    gtest_target: The name of the gtest target.

  Returns:
    The absolute path to the gtest filter JSON file.
  """
  return os.path.join(
      os.path.dirname(os.path.abspath(__file__)), 
      "..", 
      "cobalt", 
      "testing", 
      platform, 
      f"{gtest_target}_filter.json"
  )

def _process_apk_tests(args):
    """Processes APK tests based on provided arguments.

    Args:
      args: An argparse.Namespace object containing the following attributes:
        platform: The platform for the tests.
        platform_json: Optional path to a platform JSON file.
        blaze_targets: A list of Blaze targets.
        archive_path: Path to the archive containing APKs.

    Returns:
      A list of dictionaries, where each dictionary represents an APK test 
      with keys like "test_target", "device_model", "device_pool", 
      "apk_path", and "gtest_filters".
    """

    apk_tests = []
    platform_json_file = args.platform_json or _default_platform_json_file(args.platform)
    print(f"The platform_json_file is '{platform_json_file}'")
    platform_data = _read_json_config(platform_json_file)
    print(f"Loaded platform data: {platform_data}") # Added verbosity

    for target in args.blaze_targets:
        print(f"Processing Blaze target: {target}") # Added verbosity
        for gtest_target in platform_data["gtest_targets"]:
            print(f"  Processing gtest_target: {gtest_target}") # Added verbosity
            apk_test = {
                "test_target": target,
                "device_model": platform_data["gtest_device"],
                "device_pool": platform_data["gtest_lab"],
                "apk_path": f"{args.archive_path}/{gtest_target}-debug.apk",
                "gtest_filters": _get_gtest_filters(args.platform, gtest_target)
            }
            apk_tests.append(apk_test)
            print(f"  Created apk_test: {apk_test}") # Added verbosity

    print(f"apk_tests: {apk_tests}")
    return apk_tests


def _get_gtest_filters(platform, gtest_target):
    """Retrieves gtest filters for a given target.

    Args:
      platform: The platform for the tests.
      gtest_target: The name of the gtest target.

    Returns:
      A string representing the gtest filters.
    """

    gtest_filters = "*"
    filter_json_file = _get_tests_filter_json_file(platform, gtest_target)
    print(f"  gtest_filter_json_file = {filter_json_file}")
    filter_data = _read_json_config(filter_json_file)
    if filter_data:
        print(f"  Loaded filter data: {filter_data}") # Added verbosity
        failing_tests = ":".join(filter_data.get("failing_tests", []))
        if failing_tests:
            gtest_filters += ":-" + failing_tests
        print(f"  gtest_filters = {gtest_filters}")
    else:
        print(f"  This gtest_target does not have gtest_filters specified")
    return gtest_filters


def main():
    """Main routine for the on-device tests gateway client."""

    logging.basicConfig(
        level=logging.INFO, 
        format='[%(filename)s:%(lineno)s] %(message)s'
    )
    print('Starting main routine')

    parser = argparse.ArgumentParser(
        description="Client for interacting with the On-Device Tests gateway.",
        epilog=(
            'Example: ./on_device_tests_gateway_client.py trigger '
            '--token token1 '
            '--platform android-arm '
            '--archive_path /bigstore/yt-temp '
            '--blaze_targets //experimental/cobalt/chrobalt_poc:chrobalt_unit_tests_maneki_sabrina //experimental/cobalt/chrobalt_poc:chrobalt_unit_tests_shared_boreal '
            '--dimension host_name=regex:maneki-mhserver-05.*'
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    # Authentication
    parser.add_argument(
        '-t',
        '--token',
        type=str,
        required=True,
        help='On Device Tests authentication token'
    )

    # General options
    parser.add_argument(
        '--dry_run',
        action='store_true',
        help='Show what would be done without actually doing it.'
    )
    parser.add_argument(
        '-i',
        '--change_id',
        type=str,
        help='ChangeId that triggered this test, if any. Saved with performance test results.'
    )

    subparsers = parser.add_subparsers(
        dest='action', 
        help='On-Device tests commands', 
        required=True
    )

    # Trigger command
    trigger_parser = subparsers.add_parser(
        'trigger', 
        help='Trigger On-Device tests',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    trigger_parser.add_argument(
        '-p',
        '--platform',
        type=str,
        required=True,
        help='Platform this test was built for.'
    )
    trigger_parser.add_argument(
        '-pf',
        '--platform_json',
        type=str,
        help='Platform-specific JSON file containing the list of target tests.'
    )
    trigger_parser.add_argument(
        '-a',
        '--archive_path',
        type=str,
        required=True,
        help='Path to Chrobalt archive to be tested. Must be on GCS.'
    )
    trigger_parser.add_argument(
        '-l',
        '--label',
        type=str,
        action='append',
        default=[],
        help='Additional labels to assign to the test.'
    )
    trigger_parser.add_argument(
        '--gcs_result_path',
        type=str,
        help='GCS URL where test result files should be uploaded.'
    )
    trigger_parser.add_argument(
        '--dimension',
        type=str,
        action='append',
        help=(
            'On-Device Tests dimension used to select a device. '
            'Must have the following form: <dimension>=<value>. '
            'E.G. "release_version=regex:10.*"'
        )
    )
    trigger_parser.add_argument(
        '--test_attempts',
        type=str,
        default='1',
        help='The maximum number of times a test could retry.'
    )
    trigger_parser.add_argument(
        '--blaze_targets',
        nargs="+",
        type=str,
        required=True,
        help='A list of Blaze targets to run.'
    )
    trigger_parser.add_argument(
        '--retry_level',
        type=str,
        default='ERROR',
        choices=['ERROR', 'FAIL'],
        help=(
            'The retry level of Mobile Harness job. '
            'ERROR to retry for MH errors, '
            'FAIL to retry for failing tests (and MH errors).'
        )
    )

    # Watch command
    watch_parser = subparsers.add_parser(
        'watch', 
        help='Watch a previously triggered On-Device test'
    )
    watch_parser.add_argument(
        'session_id',
        type=str,
        help=(
            'Session ID of a previously triggered Mobile Harness test. '
            'The test will be watched until it completes.'
        )
    )

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
