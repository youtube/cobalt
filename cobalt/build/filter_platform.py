#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Determines via gn analyze whether changed files affect a platform build."""

import argparse
import json
from pathlib import Path
import subprocess

_INFRA_PREFIXES = (
    '.github/',
    '.gn',
    'DEPS',
    'build/',
    'build_overrides/',
    'testing/',
    'docker-compose.yaml',
    'cobalt/build/',
    'cobalt/docker/',
    'cobalt/tools/',
    'starboard/build/',
)


def should_skip(out_dir: str, files: list[str], targets: list[str]) -> bool:
  if not files or any(f.startswith(_INFRA_PREFIXES) for f in files):
    return False
  clean_targets = [
      t if t.startswith('//') else f'//{t}'
      for t in targets
      if t and t != 'null'
  ]
  in_file = Path(out_dir) / 'analyze_in.json'
  out_file = Path(out_dir) / 'analyze_out.json'
  in_file.write_text(
      json.dumps({
          'files': [f if f.startswith('//') else f'//{f}' for f in files],
          'additional_compile_targets': clean_targets,
          'test_targets': [],
      }),
      encoding='utf-8')
  subprocess.check_call(['gn', 'analyze', out_dir, in_file, out_file])
  result = json.loads(out_file.read_text(encoding='utf-8'))
  if 'error' in result:
    raise RuntimeError(result['error'])
  return result.get('status') == 'No dependency'


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--out-dir',
      required=True,
      help='Path to GN output directory for live gn analyze.')
  parser.add_argument('--changed-files', required=True)
  parser.add_argument('--targets', default='[]')
  args = parser.parse_args()

  files = Path(args.changed_files).read_text(encoding='utf-8').split()
  targets = json.loads(args.targets)
  skip = should_skip(args.out_dir, files, targets)
  print('true' if skip else 'false')


if __name__ == '__main__':
  main()
