#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is a wrapper around the ninja binary that is pulled to
third_party as part of gclient sync. It will automatically find the ninja
binary when run inside a gclient source tree, so users can just type
"ninja" on the command line."""

import os
import subprocess
import sys

import gclient_paths


def findNinjaInPath():
  env_path = os.getenv('PATH')
  if not env_path:
    return
  exe = 'ninja'
  if sys.platform in ['win32', 'cygwin']:
    exe += '.exe'
  for bin_dir in env_path.split(os.pathsep):
    if bin_dir.rstrip(os.sep).endswith('depot_tools'):
      # skip depot_tools to avoid calling ninja.py infitely.
      continue
    ninja_path = os.path.join(bin_dir, exe)
    if os.path.isfile(ninja_path):
      return ninja_path


def fallback(ninja_args):
  # Try to find ninja in PATH.
  ninja_path = findNinjaInPath()
  if ninja_path:
    return subprocess.call([ninja_path] + ninja_args)

  print(
      'depot_tools/ninja.py: Could not find Ninja in the third_party of '
      'the current project, nor in your PATH.\n'
      'Please take one of the following actions to install Ninja:\n'
      '- If your project has DEPS, add a CIPD Ninja dependency to DEPS.\n'
      '- Otherwise, add Ninja to your PATH *after* depot_tools.',
      file=sys.stderr)
  return 1


def main(args):
  # On Windows the ninja.bat script passes along the arguments enclosed in
  # double quotes. This prevents multiple levels of parsing of the special '^'
  # characters needed when compiling a single file.  When this case is detected,
  # we need to split the argument. This means that arguments containing actual
  # spaces are not supported by ninja.bat, but that is not a real limitation.
  if (sys.platform.startswith('win') and len(args) == 2):
    args = args[:1] + args[1].split()

  # macOS's python sets CPATH, LIBRARY_PATH, SDKROOT implicitly.
  # https://openradar.appspot.com/radar?id=5608755232243712
  #
  # Removing those environment variables to avoid affecting clang's behaviors.
  if sys.platform == 'darwin':
    os.environ.pop("CPATH", None)
    os.environ.pop("LIBRARY_PATH", None)
    os.environ.pop("SDKROOT", None)

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
    ninja_path = os.path.join(base_path, 'third_party', 'ninja',
                              'ninja' + gclient_paths.GetExeSuffix())
    if os.path.isfile(ninja_path):
      return subprocess.call([ninja_path] + args[1:])

  return fallback(args[1:])


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv))
  except KeyboardInterrupt:
    sys.exit(1)
