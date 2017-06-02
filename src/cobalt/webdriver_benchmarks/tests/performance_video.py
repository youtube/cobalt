#!/usr/bin/python2
"""Target for running video performance test cases."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import importlib
import os
import sys
import unittest

# The parent directory is a module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

# pylint: disable=C6204,C6203
import tv_testcase


def _add_test(test_suite, dir_path, test_name):
  if os.path.isfile(
      os.path.join(dir_path, "performance", "video", test_name + ".py")):
    print("Adding test: {}".format(test_name))
    test_suite.addTest(unittest.TestLoader().loadTestsFromModule(
        importlib.import_module("tests.performance.video." + test_name)))


# pylint: disable=unused-argument
def load_tests(loader, tests, pattern):
  """This is a Python unittest "load_tests protocol method."""
  test_suite = unittest.TestSuite()
  dir_path = os.path.dirname(__file__)

  _add_test(test_suite, dir_path, "browse_to_watch")

  return test_suite


if __name__ == "__main__":
  tv_testcase.main()
