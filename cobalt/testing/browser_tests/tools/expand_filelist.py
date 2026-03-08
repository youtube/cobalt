#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Expands a file list with patterns into a flat list of files.

Supported patterns:
  //path/to/file          - Include a single file.
  //path/to/dir/**        - Include all files in a directory recursively.
  -//path/to/pattern      - Exclude files matching a pattern.
"""

import argparse
import fnmatch
import os
import sys


def main():
  parser = argparse.ArgumentParser(description='Expand file list patterns.')
  parser.add_argument('filelist', help='Input file list with patterns.')
  parser.add_argument('--root', default='.', help='Source root directory.')
  args = parser.parse_args()

  if not os.path.isfile(args.filelist):
    potential_path = os.path.join(args.root, args.filelist)
    if os.path.isfile(potential_path):
      args.filelist = potential_path
    else:
      script_dir = os.path.dirname(os.path.abspath(__file__))
      potential_path = os.path.join(script_dir, '..', args.filelist)
      if os.path.isfile(potential_path):
        args.filelist = potential_path
      else:
        print(
            f'Error: File list not found: {args.filelist} (CWD: {os.getcwd()})',
            file=sys.stderr)
        return 1

  included_files = set()

  with open(args.filelist, 'r', encoding='utf-8') as f:
    lines = [
        line.strip()
        for line in f
        if line.strip() and not line.strip().startswith('#')
    ]

  inclusions = [l for l in lines if not l.startswith('-')]
  exclusions = [l[1:] for l in lines if l.startswith('-')]

  for pattern in inclusions:
    if pattern.startswith('//'):
      pattern = pattern[2:]

    if pattern.endswith('**'):
      base_dir = pattern[:-2]
      full_base_dir = os.path.join(args.root, base_dir)
      if os.path.isdir(full_base_dir):
        for root, _, files in os.walk(full_base_dir):
          for name in files:
            full_path = os.path.join(root, name)
            rel_file = os.path.relpath(full_path, args.root)
            included_files.add('//' + rel_file)
    else:
      full_path = os.path.join(args.root, pattern)
      if os.path.isfile(full_path):
        included_files.add('//' + pattern)
      elif os.path.isdir(full_path):
        for root, _, files in os.walk(full_path):
          for name in files:
            full_path_f = os.path.join(root, name)
            rel_file = os.path.relpath(full_path_f, args.root)
            included_files.add('//' + rel_file)

  for pattern in exclusions:
    if pattern.startswith('//'):
      pattern = pattern[2:]

    # Handle zero or more directories with **
    # Try multiple fnmatch patterns to simulate ** behavior
    match_patterns = []
    if '/**/' in pattern:
      match_patterns.append(pattern.replace('/**/', '/'))  # Zero dirs
      match_patterns.append(pattern.replace('/**', '/*'))  # One or more
    elif pattern.startswith('**/'):
      match_patterns.append(pattern[3:])  # Zero dirs at start
      match_patterns.append('*' + pattern[2:])  # One or more at start
    else:
      match_patterns.append(pattern.replace('**', '*'))

    to_discard = set()
    for mp in match_patterns:
      to_discard |= {f for f in included_files if fnmatch.fnmatch(f[2:], mp)}

    included_files -= to_discard

  for f in sorted(list(included_files)):
    print(f)

  return 0


if __name__ == '__main__':
  sys.exit(main())
