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


def run_executable(cmd, env):
  """Runs an executable with:
    - environment variable CR_SOURCE_ROOT set to the root directory.
    - environment variable LANGUAGE to en_US.UTF-8.
    - Reuses sys.executable automatically.
  """
  # Many tests assume a English interface...
  env['LANGUAGE'] = 'en_US.UTF-8'
  # Used by base/base_paths_linux.cc
  env['CR_SOURCE_ROOT'] = os.path.abspath(ROOT_DIR).encode('utf-8')
  if cmd[0].endswith('.py'):
    cmd.insert(0, sys.executable)
  return subprocess.call(cmd, env=env)


def main():
  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == "__main__":
  sys.exit(main())
