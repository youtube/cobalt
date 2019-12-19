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
from cobalt.tools.automated_testing import cobalt_runner
from proxy_server import ProxyServer
from starboard.tools import abstract_launcher
from starboard.tools import build

_PORT_SELECTION_RETRY_LIMIT = 10
_PORT_SELECTION_RANGE = [5000, 7000]
_SERVER_EXIT_TIMEOUT_SECONDS = 30
# These tests can only be run on platforms whose app launcher can send suspend/
# resume signals.
_TESTS_NEEDING_SYSTEM_SIGNAL = [
    'cancel_sync_loads_when_suspended',
    'preload_font',
    'preload_visibility',
    'signal_handler_doesnt_crash',
    'suspend_visibility',
    'timer_hit_after_preload',
    'timer_hit_in_preload',
]
# These tests only need app launchers with webdriver.
_TESTS_NO_SIGNAL = [
    'allow_eval',
    'disable_eval_with_csp',
    'persistent_cookie',
    'web_debugger',
    'web_platform_tests',
]
# These tests can only be run on platforms whose app launcher can send deep
# links.
_TESTS_NEEDING_DEEP_LINK = [
    'fire_deep_link_before_load',
]
# Location of test files.
_TEST_DIR_PATH = 'cobalt.black_box_tests.tests.'
# Platform dependent device parameters.
_device_params = None
# Binding address used to create the test server.
_binding_address = None


def GetDeviceParams():

  global _device_params
  _device_params = cobalt_runner.GetDeviceParamsFromCommandLine()
  # Keep other modules from seeing these args
  sys.argv = sys.argv[:1]


class BlackBoxTestCase(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(BlackBoxTestCase, self).__init__(*args, **kwargs)
    self.device_params = _device_params
    self.platform_config = build.GetPlatformConfig(_device_params.platform)
    self.cobalt_config = self.platform_config.GetApplicationConfiguration(
        'cobalt')

  @classmethod
  def setUpClass(cls):
    super(BlackBoxTestCase, cls).setUpClass()
    print('Running ' + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    super(BlackBoxTestCase, cls).tearDownClass()
    print('Done ' + cls.__name__)

  def CreateCobaltRunner(self, url, target_params=None):
    all_target_params = list(target_params) if target_params else []
    if _device_params.target_params is not None:
      all_target_params += _device_params.target_params
    new_runner = black_box_cobalt_runner.BlackBoxCobaltRunner(
        device_params=_device_params, url=url, target_params=all_target_params)
    return new_runner

  def GetBindingAddress(self):
    return _binding_address


def LoadTests(platform, config, device_id, out_directory):

  launcher = abstract_launcher.LauncherFactory(
      platform,
      'cobalt',
      config,
      device_id=device_id,
      target_params=None,
      output_file=None,
      out_directory=out_directory)

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

  def __init__(self, test_name=None, proxy_port_number=None):
    logging.basicConfig(level=logging.DEBUG)
    GetDeviceParams()

    # Port number used to create the proxy server. If the --proxy target param
    # is not set, a random free port is used.
    if proxy_port_number is None:
      proxy_port_number = str(self.GetUnusedPort(_binding_address))
      _device_params.target_params.append('--proxy=%s:%s' % (_binding_address,
                                                             proxy_port_number))

    self.test_name = test_name
    self.proxy_port_number = proxy_port_number

    # Test domains used in web platform tests to be resolved to the server
    # binding address.
    hosts = [
        'web-platform.test',
        'www.web-platform.test',
        'www1.web-platform.test',
        'www2.web-platform.test',
        'xn--n8j6ds53lwwkrqhv28a.web-platform.test',
        'xn--lve-6lad.web-platform.test'
    ]
    self.host_resolve_map = dict([(host, _binding_address) for host in hosts])

  def Run(self):
    if self.proxy_port_number == '-1':
      return 1
    logging.info('Using proxy port number: %s', self.proxy_port_number)

    with ProxyServer(port=self.proxy_port_number,
                     host_resolve_map=self.host_resolve_map):
      if self.test_name:
        suite = unittest.TestLoader().loadTestsFromModule(
            importlib.import_module(_TEST_DIR_PATH + self.test_name))
      else:
        suite = LoadTests(_device_params.platform, _device_params.config,
                          _device_params.device_id,
                          _device_params.out_directory)
      return_code = not unittest.TextTestRunner(
          verbosity=0, stream=sys.stdout).run(suite).wasSuccessful()
      return return_code

  def GetUnusedPort(self, machine_address):
    """Find a free port on the machine address by pinging with socket."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
      for i in range(1, _PORT_SELECTION_RETRY_LIMIT):
        port_number = random.randint(_PORT_SELECTION_RANGE[0],
                                     _PORT_SELECTION_RANGE[1])
        result = sock.connect_ex((machine_address, port_number))
        if result != 0:
          return port_number
        if i == _PORT_SELECTION_RETRY_LIMIT - 1:
          logging.error(
              'Can not find unused port on target machine within %s attempts.',
              _PORT_SELECTION_RETRY_LIMIT)
          return -1
    finally:
      sock.close()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--test_name',
                      help=('Name of test to be run. If not specified, all '
                            'tests are run.'))
  parser.add_argument('--server_binding_address',
                      default='127.0.0.1',
                      help='Binding address used to create the test server.')
  parser.add_argument('--proxy_port_number',
                      help=('Port number used to create the proxy http server'
                            'that all black box tests are run through. If not'
                            'specified, a random free port is used.'))
  args, _ = parser.parse_known_args()

  global _binding_address
  _binding_address = args.server_binding_address
  test_object = BlackBoxTests(args.test_name, args.proxy_port_number)
  sys.exit(test_object.Run())


if __name__ == '__main__':
  # Running this script on the command line and importing this file are
  # different and create two modules.
  # Import this module to ensure we are using the same module as the tests to
  # make module-owned variables like device_param accessible to the tests.
  main_module = importlib.import_module(
      'cobalt.black_box_tests.black_box_tests')
  sys.exit(main_module.main())
