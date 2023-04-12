"""This module provides webdriver-based utility functions."""
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import importlib
import sys


def import_selenium_module(submodule=None):
  """Dynamically imports a selenium.webdriver submodule.

  This is done because selenium 3.* is not commonly pre-installed
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
  # that's what we will target here as well.
  try:
    module = importlib.import_module(module_path)
    if submodule is None:
      # Only the top-level module has __version__
      if not module.__version__.startswith('3.'):
        raise ImportError('Not version 3.x.x')
  except ImportError:
    sys.stderr.write(f'Could not import {module_path}\n'
                     'Please install selenium >= 3.0.0b2.\n'
                     'Commonly: \"sudo pip install \'selenium>=3.0.0b2\'\"\n')
    sys.exit(1)
  return module
