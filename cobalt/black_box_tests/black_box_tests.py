#!/usr/bin/env python3
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Contains utilities to create black box tests and run them."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import importlib
import logging
import random
import socket
import sys
import unittest

from cobalt.black_box_tests import black_box_cobalt_runner
from cobalt.black_box_tests import bbt_settings
from cobalt.black_box_tests.proxy_server import ProxyServer
from starboard.tools import abstract_launcher
from starboard.tools import build
from starboard.tools import command_line
from starboard.tools import log_level

_DISABLED_BLACKBOXTEST_CONFIGS = [
    'android-arm64/devel',
    'android-x86/devel',
    'evergreen-arm/devel',
    'evergreen-x64/devel',
    'raspi-0/devel',
]

_PORT_SELECTION_RETRY_LIMIT = 10
_PORT_SELECTION_RANGE = [5000, 7000]
# List of blocked ports.
_RESTRICTED_PORTS = [6000, 6665, 6666, 6667, 6668, 6669, 6697]
_SERVER_EXIT_TIMEOUT_SECONDS = 30
_SOCKET_SUCCESS = 0
# These tests can only be run on platforms whose app launcher can send suspend/
# resume signals.
_TESTS_NEEDING_SYSTEM_SIGNAL = [
    'cancel_sync_loads_when_suspended',
    'pointer_test',
    'preload_font',
    'preload_visibility',
    'preload_launch_parameter',
    # TODO(b/254502632): Investigate the cause of the flakiness from this test.
    # 'signal_handler_doesnt_crash',
    'suspend_visibility',
    'timer_hit_after_preload',
    'timer_hit_in_preload',
]
# These tests only need app launchers with webdriver.
_TESTS_NO_SIGNAL = [
    'allow_eval',
    'compression_test',
    'cpu_usage_tracker_test',
    # Turned off because the server refuses to load.
    #'default_site_can_load',
    'disable_eval_with_csp',
    'h5vcc_settings_test',
    'h5vcc_storage_write_verify_test',
    # TODO(b/346882263): Disabled, it's suddenly flaky
    # 'h5vcc_watchdog_api_test',
    'http_cache',
    'javascript_profiler',
    'mediasource_worker_play',
    'navigator_test',
    'performance_resource_timing_test',
    'persistent_cookie',
    'pointer_event_on_fixed_element_test',
    'pointer_event_on_cropped_element_test',
    'scroll',
    'scroll_with_none_pointer_events',
    'service_worker_add_to_cache_test',
    'service_worker_cache_keys_test',
    'service_worker_controller_activation_test',
    'service_worker_get_registrations_test',
    'service_worker_fetch_main_resource_test',
    'service_worker_fetch_test',
    'service_worker_message_test',
    'service_worker_post_message_test',
    'service_worker_test',
    'service_worker_persist_test',
    'soft_mic_platform_service_test',
    'telemetry_test',
    'text_encoding_test',
    'wasm_basic_test',
    'web_debugger',
    'web_worker_test',
    'worker_csp_test',
    'worker_load_test',
    'worker_post_message_test',
]
# These are very different and require a custom config + proxy
_WPT_TESTS = [
    'web_platform_tests',
]
# These tests can only be run on platforms whose app launcher can send deep
# links.
_TESTS_NEEDING_DEEP_LINK = [
    'deep_links',
]
# Location of test files.
_TEST_DIR_PATH = 'cobalt.black_box_tests.tests.'

_LAUNCH_TARGET = 'cobalt'

# Platform configuration and device information parameters.
_launcher_params = None
# Binding address used to create the test server.
_server_binding_address = None
# Port used to create the web platform test http server.
_wpt_http_port = None


class BlackBoxTestCase(unittest.TestCase):
  """Base class for Cobalt black box test cases."""

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self.launcher_params = _launcher_params
    self.platform_config = build.GetPlatformConfig(_launcher_params.platform)
    self.cobalt_config = self.platform_config.GetApplicationConfiguration(
        'cobalt')

  @classmethod
  def setUpClass(cls):
    super(BlackBoxTestCase, cls).setUpClass()
    logging.info('\n\n\n%s\n\n', '=' * 40)
    logging.info('Running %s', cls.__name__)

  @classmethod
  def tearDownClass(cls):
    super(BlackBoxTestCase, cls).tearDownClass()
    logging.info('Done %s', cls.__name__)

  def CreateCobaltRunner(self, url=None, target_params=None, **kwargs):
    all_target_params = list(target_params) if target_params else []
    if _launcher_params.target_params is not None:
      all_target_params += _launcher_params.target_params

    return black_box_cobalt_runner.BlackBoxCobaltRunner(
        launcher_params=_launcher_params,
        url=url,
        target_params=all_target_params,
        web_server_port=bbt_settings.default_web_server_port,
        **kwargs)

  def GetBindingAddress(self):
    return _server_binding_address

  def GetWptHttpPort(self):
    return _wpt_http_port


def LoadTests(launcher_params, test_set):
  launcher = abstract_launcher.LauncherFactory(
      launcher_params.platform,
      _LAUNCH_TARGET,
      launcher_params.config,
      device_id=launcher_params.device_id,
      target_params=None,
      output_file=None,
      out_directory=launcher_params.out_directory,
      loader_platform=launcher_params.loader_platform,
      loader_config=launcher_params.loader_config,
      loader_out_directory=launcher_params.loader_out_directory)

  test_targets = []
  test_filters = build.GetPlatformConfig(
      _launcher_params.platform).GetApplicationConfiguration(
          'cobalt').GetTestFilters()

  if test_set in ['all', 'blackbox']:
    test_targets = _TESTS_NO_SIGNAL

    if launcher.SupportsSuspendResume():
      test_targets += _TESTS_NEEDING_SYSTEM_SIGNAL

    if launcher.SupportsDeepLink():
      test_targets += _TESTS_NEEDING_DEEP_LINK

  if test_set in ['all', 'wpt']:
    test_targets += _WPT_TESTS

  test_suite = unittest.TestSuite()
  for test in test_targets:
    filter_hit = 0
    for filtered_test in test_filters:
      if test == filtered_test.test_name:
        filter_hit = 1
        continue
    if filter_hit == 0:
      test_suite.addTest(unittest.TestLoader().loadTestsFromModule(
          importlib.import_module(_TEST_DIR_PATH + test)))
  return test_suite


class BlackBoxTests(object):
  """Helper class to run all black box tests and return results."""

  def __init__(self, args):

    self.args = args

    #TODO(b/137905502): These globals should be refactored
    # Setup global variables used by test cases.
    global _launcher_params
    _launcher_params = command_line.CreateLauncherParams()
    # Keep other modules from seeing these args.
    sys.argv = sys.argv[:1]
    global _server_binding_address
    _server_binding_address = args.server_binding_address

    # Port used to create the web platform test http server. If not specified,
    # a random free port is used.
    global _wpt_http_port
    _wpt_http_port = args.wpt_http_port or str(
        self.GetUnusedPort([_server_binding_address]))

    # Port used to set up tunneling between host and device for testdata server.
    sock = socket.socket()
    sock.bind(('', 0))
    bbt_settings.default_web_server_port = sock.getsockname()[1]

    # Proxy is only needed for WPT
    self.use_proxy = args.test_set in ['all', 'wpt']

    # TODO: Remove generation of --dev_servers_listen_ip once executable will
    # be able to bind correctly with incomplete support of IPv6
    if _launcher_params.platform not in ['android-arm', 'android-arm64']:
      if args.device_id and IsValidIpAddress(args.device_id):
        _launcher_params.target_params.append(
            f'--dev_servers_listen_ip={args.device_id}')
      elif IsValidIpAddress(_server_binding_address):
        _launcher_params.target_params.append(
            f'--dev_servers_listen_ip={_server_binding_address}')
    _launcher_params.target_params.append(
        f'--web-platform-test-server=http://web-platform.test:{_wpt_http_port}')

    # Port used to create the proxy server. If not specified, a random free
    # port is used.
    if self.use_proxy:
      self.proxy_port = args.proxy_port or str(
          self.GetUnusedPort([_server_binding_address]))
      proxy_address = args.proxy_address or _server_binding_address
      proxy_url = f'{proxy_address}:{self.proxy_port}'
      _launcher_params.target_params.append(f'--proxy={proxy_url}')

    _launcher_params.target_params.append(
        '--unsafely-treat-insecure-origin-as-secure=*web-platform.test')

    self.device_ips = args.device_ips

    # Test domains used in web platform tests to be resolved to the server
    # binding address.
    hosts = [
        'web-platform.test', 'www.web-platform.test', 'www1.web-platform.test',
        'www2.web-platform.test', 'xn--n8j6ds53lwwkrqhv28a.web-platform.test',
        'xn--lve-6lad.web-platform.test'
    ]
    self.host_resolve_map = {host: _server_binding_address for host in hosts}

  def Run(self):
    if self.use_proxy and self.proxy_port == '-1':
      return 1

    launch_config = f'{_launcher_params.platform}/{_launcher_params.config}'
    # TODO(b/135549281): Configuring this in Python is superfluous, the on/off
    # flags can be in Github Actions code
    if launch_config in _DISABLED_BLACKBOXTEST_CONFIGS:
      logging.warning('Blackbox tests disabled for platform:%s config:%s',
                      _launcher_params.platform, _launcher_params.config)
      return 0

    def LoadAndRunTests():
      if self.args.test_name:
        suite = unittest.TestLoader().loadTestsFromName(_TEST_DIR_PATH +
                                                        self.args.test_name)
      else:
        suite = LoadTests(_launcher_params, self.args.test_set)
      # Using verbosity=2 to log individual test function names and results.
      return_code = not unittest.TextTestRunner(
          verbosity=2, stream=sys.stdout).run(suite).wasSuccessful()
      return return_code

    if self.use_proxy:
      logging.info('Using proxy port: %s', self.proxy_port)
      with ProxyServer(
          port=self.proxy_port,
          host_resolve_map=self.host_resolve_map,
          client_ips=self.args.device_ips):
        return LoadAndRunTests()
    else:
      return LoadAndRunTests()

  def GetUnusedPort(self, addresses):
    """Find a free port on the list of addresses by pinging with sockets."""

    if not addresses:
      logging.error('Can not find unused port on invalid addresses.')
      return -1

    socks = []
    for address in addresses:
      socks.append((address, socket.socket(socket.AF_INET, socket.SOCK_STREAM)))
    try:
      for _ in range(_PORT_SELECTION_RETRY_LIMIT):
        port = random.choice([
            i for i in range(_PORT_SELECTION_RANGE[0], _PORT_SELECTION_RANGE[1])
            if i not in _RESTRICTED_PORTS
        ])
        unused = True
        for sock in socks:
          result = sock[1].connect_ex((sock[0], port))
          if result == _SOCKET_SUCCESS:
            unused = False
            break
        if unused:
          return port
      logging.error('Can not find unused port on addresses within %s attempts.',
                    _PORT_SELECTION_RETRY_LIMIT)
      return -1
    finally:
      for sock in socks:
        sock[1].close()


def IsValidIpAddress(address):
  """Checks if address is valid IP address."""
  return IsValidIpv4Address(address) or IsValidIpv6Address(address)


def IsValidIpv4Address(address):
  """Checks if address is valid IPv4 address."""
  try:
    socket.inet_pton(socket.AF_INET, address)
    return True
  except socket.error:  # not a valid address
    return False


def IsValidIpv6Address(address):
  """Checks if address is valid IPv6 address."""
  try:
    socket.inet_pton(socket.AF_INET6, address)
    return True
  except socket.error:  # not a valid address
    return False


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-v', '--verbose', required=False, action='store_true')
  parser.add_argument(
      '--server_binding_address',
      default='127.0.0.1',
      help='Binding address used to create the test server.')
  parser.add_argument(
      '--proxy_address',
      default=None,
      help=('Address to the proxy server that all black box'
            'tests are run through. If not specified, the'
            'server binding address is used.'))
  parser.add_argument(
      '--proxy_port',
      default=None,
      help=('Port used to create the proxy server that all'
            'black box tests are run through. If not'
            'specified, a random free port is used.'))
  parser.add_argument(
      '--test_name',
      default=None,
      help=('Name of test to be run. If not specified, all '
            'tests are run.'))
  parser.add_argument(
      '--wpt_http_port',
      default=None,
      help=('Port used to create the web platform test http'
            'server. If not specified, a random free port is'
            'used.'))
  parser.add_argument(
      '--device_id',
      default=None,
      help=('ID of test device to connect. If specified, it will be passed '
            'as --dev_servers_listen_ip param on the test device.'))
  parser.add_argument(
      '--device_ips',
      default=None,
      nargs='*',
      help=('IPs of test devices that will be allowed to connect. If not '
            'specified, all IPs will be allowed to connect.'))
  parser.add_argument(
      '--test_set', choices=['all', 'wpt', 'blackbox'], default='all')
  args, _ = parser.parse_known_args()

  log_level.InitializeLogging(args)

  test_object = BlackBoxTests(args)
  sys.exit(test_object.Run())


if __name__ == '__main__':
  # Running this script on the command line and importing this file are
  # different and create two modules.
  # Import this module to ensure we are using the same module as the tests to
  # make module-owned variables like launcher_param accessible to the tests.
  main_module = importlib.import_module(
      'cobalt.black_box_tests.black_box_tests')
  sys.exit(main_module.main())
