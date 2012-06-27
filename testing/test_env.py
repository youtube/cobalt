#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sets environment variables needed to run a chromium unit test."""

import os
import subprocess
import sys

# This is hardcoded to be src/ relative to this script.
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def run_executable(cmd, env):
  """Runs an executable with:
    - environment variable LANGUAGE to en_US.UTF-8.
    - Reuses sys.executable automatically.
  """
  # Many tests assume a English interface...
  env['LANGUAGE'] = 'en_US.UTF-8'
  # Ensure paths are correctly separated on windows.
  cmd[0] = cmd[0].replace('/', os.path.sep)
  cmd = fix_python_path(cmd)
  try:
    return subprocess.call(cmd, env=env)
  except OSError:
    print >> sys.stderr, 'Failed to start %s' % cmd
    raise


def main():
  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == "__main__":
  sys.exit(main())
