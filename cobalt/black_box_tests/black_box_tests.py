from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import importlib
import logging
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
    'cancel_sync_loads_when_suspended',
    'preload_font',
    'timer_hit_in_preload',
    'timer_hit_after_preload',
    'preload_visibility',
    'suspend_visibility',
]
# These tests only need app launchers with webdriver.
_TESTS_NO_SIGNAL = [
    # TODO: Re-enable cookie test after its flakiness is resovled.
    # 'persistent_cookie',
    'allow_eval',
    'disable_eval_with_csp',
]
# Location of test files.
_TEST_DIR_PATH = 'cobalt.black_box_tests.tests.'
# Platform dependent device parameters.
_device_params = None


def GetDeviceParams():

  global _device_params
  _device_params = cobalt_runner.GetDeviceParamsFromCommandLine()
  # Keep other modules from seeing these args
  sys.argv = sys.argv[:1]


class BlackBoxTestCase(unittest.TestCase):

  def __init__(self, *args, **kwargs):
    super(BlackBoxTestCase, self).__init__(*args, **kwargs)

  @classmethod
  def setUpClass(cls):
    print('Running ' + cls.__name__)

  @classmethod
  def tearDownClass(cls):
    print('Done ' + cls.__name__)

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
    logging.basicConfig(level=logging.DEBUG)
    GetDeviceParams()
    if self.test_name:
      suite = unittest.TestLoader().loadTestsFromModule(
          importlib.import_module(_TEST_DIR_PATH + self.test_name))
    else:
      suite = LoadTests(_device_params.platform, _device_params.config)
    return_code = not unittest.TextTestRunner(
        verbosity=0, stream=sys.stdout).run(suite).wasSuccessful()
    return return_code


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
