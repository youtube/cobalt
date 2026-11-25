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
"""Prepares input JSON and checks fast-path patterns for gn analyze."""

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Optional, Sequence


def _gn_label(item: str) -> str:
  return item if item.startswith('//') else f'//{item}'


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--out-dir', required=True, type=Path)
  parser.add_argument('--changed-files', required=True, type=Path)
  parser.add_argument('--targets', required=True)
  parser.add_argument('--always-run', default='')
  return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
  args = parse_args(argv)
  if not args.changed_files.is_file():
    print('true')
    return 0

  changed_files = [
      line.strip()
      for line in args.changed_files.read_text(encoding='utf-8').splitlines()
      if line.strip()
  ]
  if not changed_files:
    print('true')
    return 0

  pattern = re.sub(r'\s+', '', args.always_run) if args.always_run else ''
  if pattern:
    regex = re.compile(pattern)
    if any(regex.search(f) for f in changed_files):
      print('true')
      return 0

  targets = [_gn_label(t) for t in json.loads(args.targets) if t]
  in_path = args.out_dir / 'analyze_in.json'
  payload = {
      'files': sorted({_gn_label(f) for f in changed_files}),
      'test_targets': [],
      'additional_compile_targets': targets,
  }
  in_path.write_text(json.dumps(payload, indent=2), encoding='utf-8')
  return 0


if __name__ == '__main__':
  sys.exit(main())
