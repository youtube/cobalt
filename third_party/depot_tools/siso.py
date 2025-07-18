#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is a wrapper around the siso binary that is pulled to
third_party as part of gclient sync. It will automatically find the siso
binary when run inside a gclient source tree, so users can just type
"siso" on the command line."""

import os
import subprocess
import sys

import gclient_paths


def main(args):
  # On Windows the siso.bat script passes along the arguments enclosed in
  # double quotes. This prevents multiple levels of parsing of the special '^'
  # characters needed when compiling a single file.  When this case is detected,
  # we need to split the argument. This means that arguments containing actual
  # spaces are not supported by siso.bat, but that is not a real limitation.
  if sys.platform.startswith('win') and len(args) == 2:
    args = args[:1] + args[1].split()

  # macOS's python sets CPATH, LIBRARY_PATH, SDKROOT implicitly.
  # https://openradar.appspot.com/radar?id=5608755232243712
  #
  # Removing those environment variables to avoid affecting clang's behaviors.
  if sys.platform == 'darwin':
    os.environ.pop("CPATH", None)
    os.environ.pop("LIBRARY_PATH", None)
    os.environ.pop("SDKROOT", None)

  environ = os.environ.copy()

  # Get gclient root + src.
  primary_solution_path = gclient_paths.GetPrimarySolutionPath()
  gclient_root_path = gclient_paths.FindGclientRoot(os.getcwd())
  gclient_src_root_path = None
  if gclient_root_path:
    gclient_src_root_path = os.path.join(gclient_root_path, 'src')

  for base_path in set(
      [primary_solution_path, gclient_root_path, gclient_src_root_path]):
    if not base_path:
      continue
    env = environ.copy()
    sisoenv_path = os.path.join(base_path, 'build', 'config', 'siso',
                                '.sisoenv')
    if os.path.exists(sisoenv_path):
      with open(sisoenv_path) as f:
        for line in f.readlines():
          k, v = line.rstrip().split('=', 1)
          env[k] = v
    siso_path = os.path.join(base_path, 'third_party', 'siso',
                             'siso' + gclient_paths.GetExeSuffix())
    if os.path.isfile(siso_path):
      return subprocess.call([siso_path] + args[1:], env=env)

  print(
      'depot_tools/siso.py: Could not find Siso in the third_party of '
      'the current project.',
      file=sys.stderr)
  return 1


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    sys.exit(1)
