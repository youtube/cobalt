#!/usr/bin/python2
"""Target for running all tests cases."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import importlib
import os
import sys
import unittest

# The parent directory is a module
sys.path.insert(0, os.path.dirname(os.path.dirname(
    os.path.realpath(__file__))))

# pylint: disable=C6204,C6203
import tv_testcase


# pylint: disable=unused-argument
def load_tests(loader, tests, pattern):
  """This is a Python unittest "load_tests protocol method."""
  s = unittest.TestSuite()

  for f in os.listdir(os.path.dirname(__file__)):
    if not f.endswith(".py"):
      continue
    if f.startswith("_"):
      continue
    if f == "all.py":
      continue
    s.addTest(unittest.TestLoader().loadTestsFromModule(
        importlib.import_module("tests." + f[:-3])))
  return s

if __name__ == "__main__":
  tv_testcase.main()
