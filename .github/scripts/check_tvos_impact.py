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
"""Checks whether a PR's modified files affect the tvOS Cobalt build."""

import argparse
import contextlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Iterable, List, Optional, Sequence, Set

PLATFORM = 'tvos-arm64-simulator'
CONFIG = 'devel'

TEST_TARGETS_JSON_RELPATH = (
    f'cobalt/build/testing/targets/{PLATFORM}/test_targets.json')

ALWAYS_TRIGGER_EXACT_FILES = frozenset({
    'DEPS',
    '.gn',
    '.github/workflows/tvos.yaml',
    '.github/workflows/auto_label_kokoro.yaml',
    '.github/scripts/check_tvos_impact.py',
})

ALWAYS_TRIGGER_PREFIXES = (
    'build/',
    'build_overrides/',
    'cobalt/build/',
    'cobalt/devinfra/kokoro/',
    f'cobalt/testing/filters/{PLATFORM}/',
)

HEADER_EXTENSIONS = frozenset({
    '.h',
    '.hh',
    '.hpp',
    '.hxx',
    '.inc',
    '.inl',
    '.def',
})


def _normalize_path(path: str) -> str:
  norm = path.strip().lstrip('/')
  while norm.startswith('./'):
    norm = norm[2:]
  return norm


def _write_github_output(github_output: Optional[pathlib.Path], **kwargs: str):
  for key, value in kwargs.items():
    print(f'{key}={value}')
  output_path = github_output or (
      pathlib.Path(os.environ['GITHUB_OUTPUT'])
      if os.environ.get('GITHUB_OUTPUT') else None)
  if output_path:
    with output_path.open('a', encoding='utf-8') as f:
      for key, value in kwargs.items():
        f.write(f'{key}={value}\n')


def matches_always_trigger(files: Iterable[str]) -> Optional[str]:
  """Returns the first file path that unconditionally triggers a tvOS build."""
  for raw_path in files:
    path = _normalize_path(raw_path)
    if not path:
      continue
    if path in ALWAYS_TRIGGER_EXACT_FILES:
      return path
    if any(path.startswith(prefix) for prefix in ALWAYS_TRIGGER_PREFIXES):
      return path
  return None


def _is_mentioned_in_dir_gn(file_path: pathlib.Path) -> bool:
  """Checks if `file_path.name` appears in any .gn/.gni file in its folder."""
  parent = file_path.parent
  if not parent.is_dir():
    return False
  filename = file_path.name
  for gn_file in list(parent.glob('*.gn')) + list(parent.glob('*.gni')):
    try:
      if filename in gn_file.read_text(encoding='utf-8', errors='ignore'):
        return True
    except OSError:
      continue
  return False


def _find_nearest_build_gn(src_dir: pathlib.Path,
                           rel_path: str) -> Optional[str]:
  """Finds the nearest ancestor BUILD.gn relative to `src_dir`."""
  curr = (src_dir / rel_path).parent
  src_dir_resolved = src_dir.resolve()
  while True:
    try:
      curr_resolved = curr.resolve()
    except OSError:
      break
    if (curr_resolved != src_dir_resolved and
        src_dir_resolved not in curr_resolved.parents):
      break
    candidate = curr / 'BUILD.gn'
    if candidate.is_file():
      return candidate.relative_to(src_dir).as_posix()
    if curr_resolved == src_dir_resolved:
      break
    curr = curr.parent
  return None


def expand_gn_files(src_dir: pathlib.Path, files: Iterable[str]) -> List[str]:
  """Converts repo-relative paths to `//` GN source paths.

  For header files that are not explicitly listed in a same-directory `.gn` or
  `.gni` file, also includes the nearest ancestor `BUILD.gn` so unlisted headers
  in tvOS targets are still detected by `gn analyze`.
  """
  gn_files: Set[str] = set()
  for raw_path in files:
    rel_path = _normalize_path(raw_path)
    if not rel_path:
      continue
    gn_files.add(f'//{rel_path}')
    suffix = pathlib.Path(rel_path).suffix.lower()
    if suffix in HEADER_EXTENSIONS:
      abs_path = src_dir / rel_path
      if not _is_mentioned_in_dir_gn(abs_path):
        nearest_gn = _find_nearest_build_gn(src_dir, rel_path)
        if nearest_gn:
          gn_files.add(f'//{nearest_gn}')
  return sorted(gn_files)


def _normalize_gn_target(target: str) -> str:
  target = target.strip()
  if not target.startswith('//'):
    return f'//{target.lstrip("/")}'
  return target


def _parse_targets_data(data: Any) -> List[str]:
  if not isinstance(data, list):
    raise ValueError('Test targets JSON must decode to a list.')
  targets: List[str] = []
  for item in data:
    if isinstance(item, str) and item.strip():
      targets.append(_normalize_gn_target(item))
    elif isinstance(item, dict) and item.get('target'):
      targets.append(_normalize_gn_target(str(item['target'])))
  return targets


def load_test_targets(
    src_dir: pathlib.Path,
    test_targets_json: Optional[str] = None,
) -> List[str]:
  """Loads tvOS test targets from CLI JSON or test_targets.json."""
  if test_targets_json:
    return _parse_targets_data(json.loads(test_targets_json))
  json_path = src_dir / TEST_TARGETS_JSON_RELPATH
  data = json.loads(json_path.read_text(encoding='utf-8'))
  return _parse_targets_data(data)


def _find_gn_binary(src_dir: pathlib.Path) -> str:
  gn_in_path = shutil.which('gn')
  if gn_in_path:
    return gn_in_path
  bundled_gn = src_dir / 'buildtools/linux64/gn'
  if bundled_gn.is_file():
    return str(bundled_gn)
  raise FileNotFoundError('Could not find `gn` binary in PATH or buildtools.')


def get_git_changed_files(src_dir: pathlib.Path, base_sha: str) -> List[str]:
  """Returns modified file paths between `base_sha` and `HEAD` using git."""
  check_commit = subprocess.run(
      ['git', 'cat-file', '-e', f'{base_sha}^{{commit}}'],
      cwd=src_dir,
      check=False,
  )
  if check_commit.returncode != 0:
    subprocess.run(
        ['git', 'fetch', '--depth=1', 'origin', base_sha],
        cwd=src_dir,
        check=True,
    )
  proc = subprocess.run(
      ['git', 'diff', '--name-only', base_sha, 'HEAD'],
      cwd=src_dir,
      capture_output=True,
      text=True,
      check=True,
  )
  return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def check_tvos_impact(
    src_dir: pathlib.Path,
    files: Sequence[str],
    out_dir: Optional[pathlib.Path] = None,
    test_targets: Optional[Sequence[str]] = None,
    compile_targets: Optional[Sequence[str]] = None,
) -> bool:
  """Returns True if any file in `files` affects the tvOS Cobalt build."""
  normalized = [_normalize_path(f) for f in files if _normalize_path(f)]
  if not normalized:
    print('No modified files provided. Skipping tvOS build.')
    return False

  always_match = matches_always_trigger(normalized)
  if always_match:
    print(f'File "{always_match}" unconditionally affects tvOS build.')
    return True

  gn_files = expand_gn_files(src_dir, normalized)
  resolved_test_targets = (
      [_normalize_gn_target(t) for t in test_targets]
      if test_targets is not None else load_test_targets(src_dir))
  resolved_compile_targets = [
      _normalize_gn_target(t) for t in (compile_targets or ())
  ]
  analyze_input = {
      'files': gn_files,
      'additional_compile_targets': resolved_compile_targets,
      'test_targets': resolved_test_targets,
  }
  print('Running gn analyze with input:', json.dumps(analyze_input, indent=2))

  with contextlib.ExitStack() as stack:
    if out_dir is None:
      tmp_dir = stack.enter_context(
          tempfile.TemporaryDirectory(prefix='tvos_gn_analyze_'))
      build_dir = pathlib.Path(tmp_dir) / 'out'
    else:
      build_dir = out_dir
      build_dir.mkdir(parents=True, exist_ok=True)

    gn_py = src_dir / 'cobalt/build/gn.py'
    gn_gen_cmd = [
        sys.executable,
        str(gn_py),
        str(build_dir),
        '-p',
        PLATFORM,
        '-C',
        CONFIG,
        '--no-rbe',
        '--no-check',
    ]
    print('Running:', ' '.join(gn_gen_cmd))
    subprocess.run(gn_gen_cmd, cwd=src_dir, check=True)

    in_path = build_dir / 'analyze_in.json'
    out_path = build_dir / 'analyze_out.json'
    in_path.write_text(json.dumps(analyze_input), encoding='utf-8')

    gn_bin = _find_gn_binary(src_dir)
    gn_analyze_cmd = [
        gn_bin,
        'analyze',
        str(build_dir),
        str(in_path),
        str(out_path),
    ]
    if sys.platform != 'darwin':
      gn_analyze_cmd.append('--script-executable=tvos_linux_script_wrapper.py')
    print('Running:', ' '.join(gn_analyze_cmd))
    env = os.environ.copy()
    env['PATH'] = f'{build_dir.resolve()}{os.pathsep}{env.get("PATH", "")}'
    subprocess.run(gn_analyze_cmd, cwd=src_dir, env=env, check=True)

    result = json.loads(out_path.read_text(encoding='utf-8'))
    print('gn analyze result:', json.dumps(result, indent=2))

    if 'error' in result:
      print(
          f'gn analyze returned an error ({result["error"]}); '
          'defaulting to running tvOS build.',
          file=sys.stderr,
      )
      return True

    status = result.get('status')
    return status != 'No dependency'


def main(argv: Optional[Sequence[str]] = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--src-dir',
      type=pathlib.Path,
      default=pathlib.Path('.'),
      help='Path to the Cobalt src directory.',
  )
  parser.add_argument(
      '--out-dir',
      type=pathlib.Path,
      default=None,
      help='Optional directory for gn gen output.',
  )
  parser.add_argument(
      '--base-sha',
      type=str,
      default=None,
      help='Base commit SHA to diff against HEAD using git.',
  )
  parser.add_argument(
      '--files-json',
      type=str,
      default=None,
      help='JSON array of modified file paths.',
  )
  parser.add_argument(
      '--test-targets-json',
      type=str,
      default=None,
      help='JSON array of test targets (defaults to reading test_targets.json)',
  )
  parser.add_argument(
      '--compile-targets',
      nargs='*',
      default=None,
      help='Additional compile targets for gn analyze (e.g. //cobalt:cobalt).',
  )
  parser.add_argument(
      '--github-output',
      type=pathlib.Path,
      default=None,
      help='Optional path to GITHUB_OUTPUT file.',
  )
  parser.add_argument(
      'files',
      nargs='*',
      help='Modified file paths relative to src_dir.',
  )
  args = parser.parse_args(argv)

  src_dir = args.src_dir.resolve()
  try:
    files: List[str] = list(args.files)
    if args.files_json:
      parsed = json.loads(args.files_json)
      if not isinstance(parsed, list):
        raise ValueError('--files-json must decode to a JSON list of strings.')
      files.extend(str(item) for item in parsed)
    if args.base_sha:
      files.extend(get_git_changed_files(src_dir, args.base_sha))

    test_targets = (
        load_test_targets(src_dir, args.test_targets_json)
        if args.test_targets_json else None)
    affects = check_tvos_impact(
        src_dir,
        files,
        out_dir=args.out_dir,
        test_targets=test_targets,
        compile_targets=args.compile_targets,
    )
  except Exception as exc:  # pylint: disable=broad-exception-caught
    print(
        f'Error while checking tvOS impact ({exc}); '
        'defaulting to affects_tvos=true.',
        file=sys.stderr,
    )
    affects = True

  value = 'true' if affects else 'false'
  _write_github_output(args.github_output, affects_tvos=value)
  return 0


if __name__ == '__main__':
  sys.exit(main())
