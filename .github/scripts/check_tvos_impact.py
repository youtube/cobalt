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
import time
from typing import Any, Iterable, List, Optional, Sequence, Set

PLATFORM = 'tvos-arm64-simulator'
CONFIG = 'devel'

# GitHub computes refs/pull/<num>/merge asynchronously, so right after a push it
# may still point to the merge commit of the previous PR head.
MERGE_REF_FETCH_ATTEMPTS = 6
MERGE_REF_RETRY_DELAY_SECONDS = 10

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

SOURCE_EXTENSIONS = frozenset({
    '.c',
    '.cc',
    '.cpp',
    '.cxx',
    '.m',
    '.mm',
})


def _normalize_path(path: str) -> str:
  norm = path.strip().lstrip('/')
  while norm.startswith('./'):
    norm = norm[2:]
  return norm


def _write_github_output(github_output: Optional[pathlib.Path], **kwargs: str):
  for key, value in kwargs.items():
    print(f'{key}={value}')
  output_path = github_output or (pathlib.Path(os.environ['GITHUB_OUTPUT'])
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


def _find_including_sources(
    src_dir: pathlib.Path,
    rel_path: str,
) -> Set[str]:
  """Finds source files (.cc/.mm/.c) that `#include` or `#import` `rel_path`.

  Before compilation produces `.d` depfiles, `gn analyze` only knows about
  files explicitly listed in `BUILD.gn` (`sources` / `public` / `inputs`). If a
  `BUILD.gn` target omits a header from `sources` and a PR modifies only that
  header, `gn analyze` alone would miss the dependency.
  """
  includers: Set[str] = set()
  filename = pathlib.Path(rel_path).name
  parent = (src_dir / rel_path).parent
  if parent.is_dir():
    for entry in parent.iterdir():
      if not (entry.is_file() and entry.suffix.lower() in SOURCE_EXTENSIONS):
        continue
      try:
        content = entry.read_text(encoding='utf-8', errors='ignore')
      except OSError:
        continue
      if f'"{filename}"' in content or f'/{filename}"' in content:
        includers.add(entry.relative_to(src_dir).as_posix())

  if (src_dir / '.git').exists():
    proc = subprocess.run(
        [
            'git',
            'grep',
            '-l',
            '-F',
            f'"{rel_path}"',
            '--',
            '*.c',
            '*.cc',
            '*.cpp',
            '*.m',
            '*.mm',
        ],
        cwd=src_dir,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode == 0 and proc.stdout:
      for line in proc.stdout.splitlines():
        if line.strip():
          includers.add(_normalize_path(line))
  return includers


def expand_gn_files(src_dir: pathlib.Path, files: Iterable[str]) -> List[str]:
  """Converts repo-relative paths to `//` GN source paths.

  Because `gn analyze` runs on a freshly generated GN build directory without
  compiler-generated `.d` header dependency files, any header omitted from
  `BUILD.gn`'s `sources` list is invisible to GN. To prevent false negatives on
  header-only PRs, we also include the `.cc`/`.mm`/`.c` source files that
  `#include` or `#import` each modified header so `gn analyze` can check if
  those translation units are in the tvOS targets' `deps`.
  """
  gn_files: Set[str] = set()
  for raw_path in files:
    rel_path = _normalize_path(raw_path)
    if not rel_path:
      continue
    gn_files.add(f'//{rel_path}')
    suffix = pathlib.Path(rel_path).suffix.lower()
    if suffix in HEADER_EXTENSIONS:
      for src_file in _find_including_sources(src_dir, rel_path):
        gn_files.add(f'//{src_file}')
  return sorted(gn_files)


def _normalize_gn_target(target: str) -> str:
  target = target.strip()
  if not target.startswith('//'):
    return f"//{target.lstrip('/')}"
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


class MergeCommitUnavailableError(Exception):
  """Raised when the PR's up-to-date merge commit can't be fetched."""


def _git(src_dir: pathlib.Path, *args: str) -> str:
  return subprocess.run(
      ['git', *args],
      cwd=src_dir,
      capture_output=True,
      text=True,
      check=True,
  ).stdout.strip()


def fetch_pr_merge_commit(
    src_dir: pathlib.Path,
    pr_number: str,
    head_sha: Optional[str],
    attempts: int = MERGE_REF_FETCH_ATTEMPTS,
    retry_delay_seconds: float = MERGE_REF_RETRY_DELAY_SECONDS,
) -> str:
  """Fetches the PR's merge commit and returns its SHA.

  `refs/pull/<num>/merge` is GitHub's merge of the PR head into the current tip
  of the base branch: the same commit that `pull_request` workflows build and
  test. When `head_sha` is given, retries until the merge commit's second parent
  is `head_sha`, since GitHub updates the ref asynchronously after a push.

  Raises MergeCommitUnavailableError if no up-to-date merge commit is available,
  e.g., because the PR has merge conflicts.
  """
  merge_ref = f'refs/pull/{pr_number}/merge'
  for attempt in range(1, attempts + 1):
    proc = subprocess.run(
        ['git', 'fetch', '--no-tags', '--depth=2', 'origin', merge_ref],
        cwd=src_dir,
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode == 0:
      merge_sha = _git(src_dir, 'rev-parse', 'FETCH_HEAD')
      parents = _git(src_dir, 'rev-list', '--parents', '-n', '1',
                     merge_sha).split()[1:]
      if len(parents) == 2 and (head_sha is None or parents[1] == head_sha):
        print(f'Using merge commit {merge_sha} of PR #{pr_number}.')
        return merge_sha
      print(f'{merge_ref} ({merge_sha}) is not the merge of PR head '
            f'{head_sha} yet (parents: {parents}).')
    else:
      print(f'Failed to fetch {merge_ref}: {proc.stderr.strip()}')
    if attempt < attempts:
      time.sleep(retry_delay_seconds)
  raise MergeCommitUnavailableError(
      f'No up-to-date {merge_ref} after {attempts} attempts. The PR may have '
      'merge conflicts.')


def get_merge_commit_changed_files(
    src_dir: pathlib.Path,
    merge_sha: str,
) -> List[str]:
  """Returns the files the PR changes on top of the base branch tip.

  That is the diff between the merge commit and its first parent. Renames are
  listed as both the old and the new path.
  """
  output = _git(src_dir, 'diff', '--name-only', '--no-renames',
                f'{merge_sha}^1', merge_sha)
  return [line.strip() for line in output.splitlines() if line.strip()]


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
  resolved_test_targets = ([
      _normalize_gn_target(t) for t in test_targets
  ] if test_targets is not None else load_test_targets(src_dir))
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

    wrapper_script = (src_dir /
                      'cobalt/build/tvos_linux_script_wrapper.py').resolve()
    script_exec_args = ([f'--script-executable={wrapper_script}']
                        if sys.platform.startswith('linux') else [])

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
        *script_exec_args,
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
        *script_exec_args,
    ]
    print('Running:', ' '.join(gn_analyze_cmd))
    subprocess.run(gn_analyze_cmd, cwd=src_dir, check=True)

    result = json.loads(out_path.read_text(encoding='utf-8'))
    print('gn analyze result:', json.dumps(result, indent=2))

    if 'error' in result:
      print(
          f"gn analyze returned an error ({result['error']}); "
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
      '--pr-number',
      type=str,
      default=None,
      help=('PR number to analyze. Checks out the PR\'s merge commit into the '
            'base branch tip (refs/pull/<num>/merge) in --src-dir, and '
            'analyzes the files it changes.'),
  )
  parser.add_argument(
      '--pr-head-sha',
      type=str,
      default=None,
      help=('Expected head SHA of --pr-number, used to make sure the merge '
            'commit is up to date.'),
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
    if args.pr_number:
      merge_sha = fetch_pr_merge_commit(src_dir, args.pr_number,
                                        args.pr_head_sha)
      files.extend(get_merge_commit_changed_files(src_dir, merge_sha))
      # Analyzes the build graph that `pull_request` workflows would build.
      _git(src_dir, 'checkout', '--detach', merge_sha)

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
