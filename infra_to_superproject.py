#!/usr/bin/env python3
# Copyright (c) 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Migrates an infra or infra_internal gclient to an infra_superproject one.

Does an in-place migration of the cwd's infra or infra_internal gclient
checkout to an infra_superproject checkout, preserving all repo branches
and commits. Should be called in the gclient root directory (the one that
contains the .gclient file).

By default creates a backup dir of the original gclient checkout in
`<dir>_backup`. If something goes wrong during the migration, this can
be used to restore your environment to its original state.
"""

import argparse
import subprocess
import os
import platform
import sys
import shutil
import json
from pathlib import Path


def main(argv):
  source = os.getcwd()

  parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0],
                                   epilog=' '.join(
                                       __doc__.strip().splitlines()[1:]))
  parser.add_argument('-n',
                      '--no-backup',
                      action='store_true',
                      help='NOT RECOMMENDED. Skips copying the current '
                      'checkout (which can take up to ~15 min) to '
                      'a backup before starting the migration.')
  args = parser.parse_args(argv)

  if not args.no_backup:
    backup = source + '_backup'
    print(f'Creating backup in {backup}')
    print('May take up to ~15 minutes...')
    shutil.copytree(source, backup, symlinks=True, dirs_exist_ok=True)
    print('backup complete')

  print(f'Deleting old {source}/.gclient file')
  gclient_file = os.path.join(source, '.gclient')
  with open(gclient_file, 'r') as file:
    data = file.read()
    internal = "infra_internal" in data
  os.remove(gclient_file)

  print('Migrating to infra/infra_superproject')
  cmds = ['fetch', '--force']
  if internal:
    cmds.append('infra_internal')
    print('including internal code in checkout')
  else:
    cmds.append('infra')
  shell = sys.platform == 'win32'
  fetch = subprocess.Popen(cmds, cwd=source, shell=shell)
  fetch.wait()


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
