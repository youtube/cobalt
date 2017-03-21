#!/usr/bin/python

# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides read access to buildbot's global_variables.json .
"""


import HTMLParser
import json
import re
import retrieve_from_googlesource
import svn
import sys


_global_vars = None


SKIABOT_REPO = 'https://skia.googlesource.com/buildbot'
_GLOBAL_VARS_PATH = 'site_config/global_variables.json'


class GlobalVarsRetrievalError(Exception):
  """Exception which is raised when the global_variables.json file cannot be
  retrieved from the Skia buildbot repository."""
  pass


class JsonDecodeError(Exception):
  """Exception which is raised when the global_variables.json file cannot be
  interpreted as JSON. This may be due to the file itself being incorrectly
  formatted or due to an incomplete or corrupted downloaded version of the file.
  """
  pass


class NoSuchGlobalVariable(KeyError):
  """Exception which is raised when a given variable is not found in the
  global_variables.json file."""
  pass


def Get(var_name):
  """Return the value associated with this name in global_variables.json.

  Args:
      var_name: string; the variable to look up.
  Returns:
      The value of the variable.
  Raises:
      NoSuchGlobalVariable if there is no variable with that name.
  """
  global _global_vars
  if not _global_vars:
    try:
      global_vars_text = retrieve_from_googlesource.get(SKIABOT_REPO,
                                                        _GLOBAL_VARS_PATH)
    except Exception as e:
      raise GlobalVarsRetrievalError('Failed to retrieve %s from %s:\n%s' %
                                     (_GLOBAL_VARS_PATH, SKIABOT_REPO, str(e)))
    try:
      _global_vars = json.loads(global_vars_text)
    except ValueError as e:
      raise JsonDecodeError(e.message + '\n' + global_vars_text)
  try:
    return _global_vars[var_name]['value']
  except KeyError:
    raise NoSuchGlobalVariable(var_name)


if __name__ == '__main__':
  print Get(sys.argv[1])
