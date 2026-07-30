#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""TestSuiteMapper: Maps a file to its target test suite."""

import argparse
import ast
import json
import os
import pathlib
import re
import subprocess
import sys

SRC_ROOT = pathlib.Path(__file__).resolve().parents[2]
ISOLATE_MAP_PATH = (pathlib.Path('infra') / 'config' / 'generated' / 'testing' /
                    'gn_isolate_map.pyl')


def parse_isolate_map(content_str, source_name):
  """Parses isolate map content string."""
  try:
    data = ast.literal_eval(content_str)
    # Map label -> suite name, excluding additional_compile_target
    label_to_suite = {}
    if not isinstance(data, dict):
      print(f'Error parsing {source_name}: Expected a dictionary',
            file=sys.stderr)
      return None
    for suite, details in data.items():
      if isinstance(
          details, dict) and details.get('type') == 'additional_compile_target':
        continue
      if isinstance(details, dict):
        label = details.get('label')
        if label:
          label_to_suite[label] = suite
    return label_to_suite
  except Exception as e:
    print(f'Error parsing gn_isolate_map.pyl from {source_name}: {e}',
          file=sys.stderr)
    return None


def load_isolate_map(src_root):
  """Loads and parses the gn_isolate_map.pyl file from the workspace."""
  src_root = pathlib.Path(src_root)
  pyl_path = src_root / ISOLATE_MAP_PATH
  if not pyl_path.exists():
    print(
        f'Error: Isolate map not found at {pyl_path}. '
        f'Please run main.star or sync.',
        file=sys.stderr,
    )
    return None

  try:
    with open(pyl_path, 'r', encoding='utf-8') as f:
      return parse_isolate_map(f.read(), str(pyl_path))
  except Exception as e:
    print(f'Error reading {pyl_path}: {e}', file=sys.stderr)
    return None


def run_gn_refs(src_root, build_dir, target_file):
  """Executes `gn refs` to retrieve the dependent targets closure."""
  # Normalize target file to gn syntax
  gn_target = target_file.as_posix()
  if not gn_target.startswith('//'):
    gn_target = '//' + gn_target.lstrip('/')

  # Execute gn refs command
  cmd = ['gn', 'refs', build_dir, '--all', gn_target]
  try:
    res = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=src_root,
        check=False,
        encoding='utf-8',
    )
    if res.returncode != 0:
      return None, res.stderr.strip()
    return res.stdout.splitlines(), None
  except Exception as e:
    return None, str(e)


def common_dir_prefix_len(path1, path2):
  """Calculates the number of common leading directory components."""
  p1 = path1.strip('/').split('/')
  p2 = path2.strip('/').split('/')
  common = 0
  for c1, c2 in zip(p1, p2):
    if c1 == c2:
      common += 1
    else:
      break
  return common


def extract_test_suites(gn_refs_output, file_path, isolate_map):
  """Scans and filters transitive GN targets for executable test suites.

  Heuristic: Prefers test suites that are in a closer directory to the
  target file.
  """
  resolved_suites = {}

  # Normalize file_path to relative path without leading // for comparison
  file_path_str = file_path.as_posix()
  file_dir = str(pathlib.Path(file_path_str.lstrip('/')).parent)

  for target in gn_refs_output:
    target = target.strip()
    if not target:
      continue

    parts = target.split(':')
    target_dir = parts[0].lstrip('/') if len(parts) > 1 else ''

    # Look up in isolate map
    if target in isolate_map:
      suite_name = isolate_map[target]
      # Score based on how many directory components are shared.
      # The 'score' is the depth of the common directory prefix between the
      # file and the target.
      score = common_dir_prefix_len(file_dir, target_dir)
      resolved_suites[suite_name] = max(resolved_suites.get(suite_name, -1),
                                        score)

  if not resolved_suites:
    return []

  max_score = max(resolved_suites.values())

  # Filter: if we have any matches with score > 0, discard matches with
  # score == 0 (e.g., targets in the root directory).
  if max_score > 0:
    filtered_suites = [
        name for name, score in resolved_suites.items() if score > 0
    ]
  else:
    filtered_suites = list(resolved_suites.keys())

  return sorted(filtered_suites)


def verify_build_dir(src_root, build_dir):
  """Verifies that build_dir is configured to compile for Linux."""
  src_root = pathlib.Path(src_root)
  args_gn_path = src_root / build_dir / 'args.gn'
  if not args_gn_path.exists():
    return (
        False,
        f'args.gn not found in {build_dir}. Please run `gn gen`.',
    )

  target_os = None
  try:
    with open(args_gn_path, 'r', encoding='utf-8') as f:
      for line in f:
        line = line.split('#')[0].strip()
        if not line:
          continue
        parts = line.split('=', 1)
        if len(parts) == 2:
          key = parts[0].strip()
          val = parts[1].strip().strip('"\'')
          if key == 'target_os':
            target_os = val
            break
  except Exception as e:
    return False, f'Error reading {args_gn_path}: {e}'

  if target_os and target_os != 'linux':
    return (
        False,
        f'Unsupported target_os: {target_os}. '
        f"Only 'linux' is supported initially.",
    )

  if not target_os and not sys.platform.startswith('linux'):
    return (
        False,
        f'Host platform {sys.platform} is not Linux, '
        f"and target_os is not set to 'linux' in args.gn.",
    )

  return True, None


def main():
  parser = argparse.ArgumentParser(
      description='Maps a Chromium file path to its covering test suites.')
  parser.add_argument(
      'file_path',
      type=pathlib.Path,
      help='Relative path to the target file (e.g., chrome/browser/.../file.cc)'
  )
  parser.add_argument(
      '--build-dir',
      type=pathlib.Path,
      default=pathlib.Path('out/Default'),
      help=('Build directory (defaults to out/Default). Must contain args.gn '
            'and be configured for target_os="linux".'),
  )
  args = parser.parse_args()

  src_root = SRC_ROOT
  build_dir = args.build_dir

  # Verify build directory exists
  if not (src_root / build_dir).exists():
    print(
        f'Error: Build directory {build_dir} does not exist or is not '
        f'configured.',
        file=sys.stderr,
    )
    print(
        f'Please run `gn gen {build_dir}` or supply a valid --build-dir.',
        file=sys.stderr,
    )
    sys.exit(1)

  success, err = verify_build_dir(src_root, build_dir)
  if not success:
    print(f'Error: {err}', file=sys.stderr)
    sys.exit(1)

  isolate_map = load_isolate_map(src_root)
  if isolate_map is None:
    sys.exit(1)

  # Run references scan (uses current workspace GN graph)
  refs, err = run_gn_refs(src_root, build_dir, args.file_path)
  if err:
    print(f'Error running GN refs query: {err}', file=sys.stderr)
    sys.exit(1)

  if not refs:
    print(json.dumps([]))
    sys.exit(0)

  test_suites = extract_test_suites(refs, args.file_path, isolate_map)
  print(json.dumps(test_suites, indent=2))


if __name__ == '__main__':
  main()
