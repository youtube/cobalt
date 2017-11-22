from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import importlib
import logging
import os
import socket
import subprocess
import sys
import unittest

import _env  # pylint: disable=unused-import
from cobalt.black_box_tests import black_box_cobalt_runner
from cobalt.tools.automated_testing import cobalt_runner
from starboard.tools import abstract_launcher

_SERVER_EXIT_TIMEOUT_SECONDS = 30
# These tests can only be run on platforms whose app launcher can send suspend/
# resume signals.
_TESTS_NEEDING_SYSTEM_SIGNAL = [
    'preload_font',
    'timer_hit_in_preload',
    'timer_hit_after_preload',
    'preload_visibility',
    'suspend_visibility',
]
# These tests only need app launchers with webdriver.
_TESTS_NO_SIGNAL = [
    'persistent_cookie',
    'allow_eval',
    'disable_eval_with_csp',
]
# Port number of the HTTP server serving test data.
_DEFAULT_TEST_DATA_SERVER_PORT = 8000
# Location of test files.
_TEST_DIR_PATH = 'cobalt.black_box_tests.tests.'
# Platform dependent device parameters.
_device_params = None


def GetDeviceParams():

  global _device_params
  _device_params = cobalt_runner.GetDeviceParamsFromCommandLine()
  # Keep other modules from seeing these args
  sys.argv = sys.argv[:1]


def GetDefaultBlackBoxTestDataAddress():
  """Gets the ip address with port for the server hosting test data."""
  # We are careful to choose this method that allows external device to connect
  # to the host running the web server hosting test data.
  address_pack_list = socket.getaddrinfo(socket.gethostname(),
                                         _DEFAULT_TEST_DATA_SERVER_PORT)
  first_address_pack = address_pack_list[0]
  ip_address, port = first_address_pack[4]
  return 'http://{}:{}/'.format(ip_address, port)


class BlackBoxTestCase(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(BlackBoxTestCase, self).__init__(*args, **kwargs)

  @classmethod
  def setUpClass(cls):
    print('Running ' + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    print('Done ' + cls.__name__)

  def GetURL(self, file_name):
    return GetDefaultBlackBoxTestDataAddress() + file_name

  def CreateCobaltRunner(self, url, target_params=None):
    new_runner = black_box_cobalt_runner.BlackBoxCobaltRunner(
        device_params=_device_params, url=url, target_params=target_params)
    return new_runner


def LoadTests(platform, config):

  launcher = abstract_launcher.LauncherFactory(
      platform,
      'cobalt',
      config,
      device_id=None,
      target_params=None,
      output_file=None,
      out_directory=None)

  if launcher.SupportsSuspendResume():
    test_targets = _TESTS_NEEDING_SYSTEM_SIGNAL + _TESTS_NO_SIGNAL
  else:
    test_targets = _TESTS_NO_SIGNAL

  test_suite = unittest.TestSuite()
  for test in test_targets:
    test_suite.addTest(unittest.TestLoader().loadTestsFromModule(
        importlib.import_module(_TEST_DIR_PATH + test)))
  return test_suite


class BlackBoxTests(object):
  """Helper class to run all black box tests and return results."""

  def __init__(self, test_name=None):

    self.test_name = test_name

  def Run(self):

    self._StartTestdataServer()
    logging.basicConfig(level=logging.DEBUG)
    GetDeviceParams()
    if self.test_name:
      suite = unittest.TestLoader().loadTestsFromModule(
          importlib.import_module(_TEST_DIR_PATH + self.test_name))
    else:
      suite = LoadTests(_device_params.platform, _device_params.config)
    return_code = not unittest.TextTestRunner(
        verbosity=0, stream=sys.stdout).run(suite).wasSuccessful()
    self._KillTestdataServer()
    return return_code

  def _StartTestdataServer(self):
    """Start a local server to serve test data."""
    # Some tests like preload_font requires server feature support.
    # Using HTTP URL instead of file URL also saves the trouble to
    # deploy test data to device.
    self.default_test_data_server_process = subprocess.Popen(
        [
            'python', '-m', 'SimpleHTTPServer',
            '{}'.format(_DEFAULT_TEST_DATA_SERVER_PORT)
        ],
        cwd=os.path.join(
            os.path.dirname(os.path.realpath(__file__)), 'testdata'),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    print('Starting HTTP server on port: {}'.format(
        _DEFAULT_TEST_DATA_SERVER_PORT))

  def _KillTestdataServer(self):
    """Exit black_box_test_runner with test result."""
    self.default_test_data_server_process.kill()


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--test_name')
  args, _ = parser.parse_known_args()

  test_object = BlackBoxTests(args.test_name)
  sys.exit(test_object.Run())


if __name__ == '__main__':
  # Running this script on the command line and importing this file are
  # different and create two modules.
  # Import this module to ensure we are using the same module as the tests to
  # make module-owned variables like device_param accessible to the tests.
  main_module = importlib.import_module(
      'cobalt.black_box_tests.black_box_tests')
  main_module.main()
