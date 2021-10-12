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

import _env  # pylint: disable=unused-import
from cobalt.black_box_tests import black_box_cobalt_runner
from proxy_server import ProxyServer
from starboard.tools import abstract_launcher
from starboard.tools import build
from starboard.tools import command_line
from starboard.tools import log_level

_DISABLED_BLACKBOXTEST_CONFIGS = [
    'android-arm/devel',
    'android-arm64/devel',
    'android-x86/devel',
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
    'signal_handler_doesnt_crash',
    'suspend_visibility',
    'timer_hit_after_preload',
    'timer_hit_in_preload',
]
# These tests only need app launchers with webdriver.
_TESTS_NO_SIGNAL = [
    'allow_eval',
    'compression_test',
    'disable_eval_with_csp',
    'persistent_cookie',
    'web_debugger',
    'web_platform_tests',
]
# These tests can only be run on platforms whose app launcher can send deep
# links.
_TESTS_NEEDING_DEEP_LINK = [
    'deep_links',
]
# Location of test files.
_TEST_DIR_PATH = 'cobalt.black_box_tests.tests.'
# Platform configuration and device information parameters.
_launcher_params = None
# Binding address used to create the test server.
_binding_address = None
# Port used to create the web platform test http server.
_wpt_http_port = None


class BlackBoxTestCase(unittest.TestCase):
  """Base class for Cobalt black box test cases."""

  def __init__(self, *args, **kwargs):
    super(BlackBoxTestCase, self).__init__(*args, **kwargs)
    self.launcher_params = _launcher_params
    self.platform_config = build.GetPlatformConfig(_launcher_params.platform)
    self.cobalt_config = self.platform_config.GetApplicationConfiguration(
        'cobalt')

  @classmethod
  def setUpClass(cls):
    super(BlackBoxTestCase, cls).setUpClass()
    logging.info('Running %s', cls.__name__)

  @classmethod
  def tearDownClass(cls):
    super(BlackBoxTestCase, cls).tearDownClass()
    logging.info('Done %s', cls.__name__)

  def CreateCobaltRunner(self, url, target_params=None):
    all_target_params = list(target_params) if target_params else []
    if _launcher_params.target_params is not None:
      all_target_params += _launcher_params.target_params
    new_runner = black_box_cobalt_runner.BlackBoxCobaltRunner(
        launcher_params=_launcher_params,
        url=url,
        target_params=all_target_params)
    return new_runner

  def GetBindingAddress(self):
    return _binding_address

  def GetWptHttpPort(self):
    return _wpt_http_port


def LoadTests(launcher_params):
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

  test_targets = _TESTS_NO_SIGNAL

  if launcher.SupportsSuspendResume():
    test_targets += _TESTS_NEEDING_SYSTEM_SIGNAL

  if launcher.SupportsDeepLink():
    test_targets += _TESTS_NEEDING_DEEP_LINK

  test_suite = unittest.TestSuite()
  for test in test_targets:
    test_suite.addTest(unittest.TestLoader().loadTestsFromModule(
        importlib.import_module(_TEST_DIR_PATH + test)))
  return test_suite


class BlackBoxTests(object):
  """Helper class to run all black box tests and return results."""

  def __init__(self,
               server_binding_address,
               proxy_address=None,
               proxy_port=None,
               test_name=None,
               wpt_http_port=None,
               device_ips=None):
    # Setup global variables used by test cases.
    global _launcher_params
    _launcher_params = command_line.CreateLauncherParams()
    # Keep other modules from seeing these args.
    sys.argv = sys.argv[:1]
    global _binding_address
    _binding_address = server_binding_address
    # Port used to create the web platform test http server. If not specified,
    # a random free port is used.
    if wpt_http_port is None:
      wpt_http_port = str(self.GetUnusedPort([server_binding_address]))
    global _wpt_http_port
    _wpt_http_port = wpt_http_port
    _launcher_params.target_params.append(
        '--web-platform-test-server=http://web-platform.test:%s' %
        wpt_http_port)

    # Port used to create the proxy server. If not specified, a random free
    # port is used.
    if proxy_port is None:
      proxy_port = str(self.GetUnusedPort([server_binding_address]))
    if proxy_address is None:
      proxy_address = server_binding_address
    _launcher_params.target_params.append('--proxy=%s:%s' %
                                          (proxy_address, proxy_port))

    self.proxy_port = proxy_port
    self.test_name = test_name
    self.device_ips = device_ips

    # Test domains used in web platform tests to be resolved to the server
    # binding address.
    hosts = [
        'web-platform.test', 'www.web-platform.test', 'www1.web-platform.test',
        'www2.web-platform.test', 'xn--n8j6ds53lwwkrqhv28a.web-platform.test',
        'xn--lve-6lad.web-platform.test'
    ]
    self.host_resolve_map = {host: server_binding_address for host in hosts}

  def Run(self):
    if self.proxy_port == '-1':
      return 1

    # Temporary means to determine if we are running on CI
    # TODO: Update to IS_CI environment variable or similar
    out_dir = _launcher_params.out_directory
    is_ci = out_dir and 'mh_lab' in out_dir  # pylint: disable=unsupported-membership-test

    target = (_launcher_params.platform, _launcher_params.config)
    if is_ci and '{}/{}'.format(*target) in _DISABLED_BLACKBOXTEST_CONFIGS:
      logging.warning('Blackbox tests disabled for platform:%s config:%s',
                      *target)
      return 0

    logging.info('Using proxy port: %s', self.proxy_port)

    with ProxyServer(
        port=self.proxy_port,
        host_resolve_map=self.host_resolve_map,
        client_ips=self.device_ips):
      if self.test_name:
        suite = unittest.TestLoader().loadTestsFromModule(
            importlib.import_module(_TEST_DIR_PATH + self.test_name))
      else:
        suite = LoadTests(_launcher_params)
      return_code = not unittest.TextTestRunner(
          verbosity=0, stream=sys.stdout).run(suite).wasSuccessful()
      return return_code

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
      '--device_ips',
      default=None,
      nargs='*',
      help=('IPs of test devices that will be allowed to connect. If not'
            'specified, all IPs will be allowed to connect.'))
  args, _ = parser.parse_known_args()

  log_level.InitializeLogging(args)

  test_object = BlackBoxTests(args.server_binding_address, args.proxy_address,
                              args.proxy_port, args.test_name,
                              args.wpt_http_port, args.device_ips)
  sys.exit(test_object.Run())


if __name__ == '__main__':
  # Running this script on the command line and importing this file are
  # different and create two modules.
  # Import this module to ensure we are using the same module as the tests to
  # make module-owned variables like launcher_param accessible to the tests.
  main_module = importlib.import_module(
      'cobalt.black_box_tests.black_box_tests')
  sys.exit(main_module.main())
