#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Determines via gn analyze whether changed files affect a platform build."""

import argparse
import json
from pathlib import Path
import subprocess

_INFRA_PREFIXES = ('.github/', '.gn', 'DEPS', 'build/', 'testing/',
                   'docker-compose.yaml', 'cobalt/build/', 'cobalt/docker/',
                   'cobalt/tools/', 'starboard/build/')


def _resolve_labels(all_labels: set[str], raw_targets: list[str]) -> list[str]:
  resolved = set()
  for target in raw_targets:
    if not target or target == 'null':
      continue
    label = target if target.startswith('//') else f'//{target}'
    matched = [label] if label in all_labels else [
        l for l in all_labels if ':' not in target and l.endswith(f':{target}')
    ]
    if not matched:
      raise ValueError(f'Cannot resolve target label: {target}')
    resolved.update(matched)
  return sorted(resolved)


def should_skip_from_deps(deps_file: str, files: list[str]) -> bool:
  if not files or any(f.startswith(_INFRA_PREFIXES) for f in files):
    return False
  raw_deps = json.loads(Path(deps_file).read_text(encoding='utf-8'))
  if isinstance(raw_deps, dict):
    deps = {f for flist in raw_deps.values() for f in flist}
  elif isinstance(raw_deps, list):
    deps = set(raw_deps)
  else:
    raise ValueError(
        f'Unexpected deps format in {deps_file}: expected list or dict')

  normalized_deps = {d[2:] if d.startswith('//') else d for d in deps}
  for f in files:
    norm_f = f[2:] if f.startswith('//') else f
    if norm_f in normalized_deps:
      return False
  return True


def should_skip(out_dir: str, files: list[str], targets: list[str]) -> bool:
  if not files or any(f.startswith(_INFRA_PREFIXES) for f in files):
    return False
  all_labels = set(
      subprocess.check_output(['gn', 'ls', out_dir], text=True).splitlines())
  in_file = Path(out_dir) / 'analyze_in.json'
  out_file = Path(out_dir) / 'analyze_out.json'
  in_file.write_text(
      json.dumps({
          'files': [f'//{f}' for f in files],
          'additional_compile_targets': _resolve_labels(all_labels, targets),
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
      required=False,
      help='Path to GN output directory for live gn analyze.')
  parser.add_argument(
      '--deps-file',
      required=False,
      help=('Path to precomputed target dependencies JSON (e.g. for tvos on'
            ' Linux).'),
  )
  parser.add_argument('--changed-files', required=True)
  parser.add_argument('--targets', default='[]')
  args = parser.parse_args()
  if not args.out_dir and not args.deps_file:
    parser.error('Either --out-dir or --deps-file must be specified.')

  files = Path(args.changed_files).read_text(encoding='utf-8').split()
  if args.deps_file:
    skip = should_skip_from_deps(args.deps_file, files)
  else:
    targets = json.loads(args.targets)
    skip = should_skip(args.out_dir, files, targets)
  print('true' if skip else 'false')


if __name__ == '__main__':
  main()
