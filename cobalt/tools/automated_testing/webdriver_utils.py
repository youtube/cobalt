"""This module provides webdriver-based utility functions."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import importlib
import sys


def import_selenium_module(submodule=None):
  """Dynamically imports a selenium.webdriver submodule.

  This is done because selenium 3.0 is not commonly pre-installed
  on workstations, and we want to have a friendly error message for that
  case.

  Args:
    submodule: module subpath underneath "selenium.webdriver"
  Returns:
    appropriate module
  """
  if submodule:
    module_path = '.'.join(('selenium', submodule))
  else:
    module_path = 'selenium'
  # As of this writing, Google uses selenium 3.0.0b2 internally, so
  # thats what we will target here as well.
  try:
    module = importlib.import_module(module_path)
    if submodule is None:
      # Only the top-level module has __version__
      if not module.__version__.startswith('3.0'):
        raise ImportError('Not version 3.0.x')
  except ImportError:
    sys.stderr.write('Could not import {}\n'
                     'Please install selenium >= 3.0.0b2.\n'
                     'Commonly: \"sudo pip install \'selenium>=3.0.0b2\'\"\n'
                     .format(module_path))
    sys.exit(1)
  return module
