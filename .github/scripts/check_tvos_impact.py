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
"""Checks whether a PR affects the tvOS Cobalt build and manages CI labels."""

import argparse
import contextlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Any, Iterable, List, Optional, Sequence, Set, Tuple
import urllib.error
import urllib.parse
import urllib.request

PLATFORM = 'tvos-arm64-simulator'
CONFIG = 'devel'
KOKORO_RUN_LABEL = 'kokoro:run'
KOKORO_FORCE_RUN_LABEL = 'kokoro:force-run'

DEFAULT_COMPILE_TARGETS = ('//cobalt:cobalt',)
DEFAULT_TEST_TARGETS = (
    '//base:base_unittests',
    '//cobalt/testing/browser_tests:cobalt_browsertests',
    '//cobalt:cobalt_unittests',
    '//starboard:starboard_unittests_wrapper',
)
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

ALLOWED_AUTHOR_ASSOCIATIONS = frozenset({
    'OWNER',
    'MEMBER',
    'COLLABORATOR',
})
ALLOWED_COLLABORATOR_PERMISSIONS = frozenset({
    'admin',
    'write',
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


def load_test_targets(src_dir: pathlib.Path) -> List[str]:
  """Loads tvOS test targets from test_targets.json."""
  json_path = src_dir / TEST_TARGETS_JSON_RELPATH
  if not json_path.is_file():
    return list(DEFAULT_TEST_TARGETS)
  try:
    data = json.loads(json_path.read_text(encoding='utf-8'))
    targets = [
        _normalize_gn_target(entry['target'])
        for entry in data
        if entry.get('target')
    ]
    return targets or list(DEFAULT_TEST_TARGETS)
  except (OSError, ValueError, KeyError, TypeError):
    return list(DEFAULT_TEST_TARGETS)


def _find_gn_binary(src_dir: pathlib.Path) -> str:
  gn_in_path = shutil.which('gn')
  if gn_in_path:
    return gn_in_path
  bundled_gn = src_dir / 'buildtools/linux64/gn'
  if bundled_gn.is_file():
    return str(bundled_gn)
  raise FileNotFoundError('Could not find `gn` binary in PATH or buildtools.')


def check_tvos_impact(
    src_dir: pathlib.Path,
    files: Sequence[str],
    out_dir: Optional[pathlib.Path] = None,
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
  test_targets = load_test_targets(src_dir)
  analyze_input = {
      'files': gn_files,
      'additional_compile_targets': list(DEFAULT_COMPILE_TARGETS),
      'test_targets': test_targets,
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


def _github_api_request(
    method: str,
    endpoint: str,
    payload: Optional[dict] = None,
) -> Any:
  """Performs a GitHub REST API request using urllib."""
  api_base = os.environ.get('GITHUB_API_URL', 'https://api.github.com').rstrip(
      '/')
  url = f'{api_base}/{endpoint.lstrip("/")}'
  token = os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
  headers = {
      'Accept': 'application/vnd.github+json',
      'User-Agent': 'cobalt-check-tvos-impact',
      'X-GitHub-Api-Version': '2022-11-28',
  }
  if token:
    headers['Authorization'] = f'Bearer {token}'
  data = None
  if payload is not None:
    data = json.dumps(payload).encode('utf-8')
    headers['Content-Type'] = 'application/json'

  req = urllib.request.Request(url, data=data, headers=headers, method=method)
  with urllib.request.urlopen(req, timeout=30) as resp:
    body = resp.read().decode('utf-8')
    return json.loads(body) if body else None


def _resolve_repo_and_pr(
    pr_url: Optional[str] = None) -> Optional[Tuple[str, str]]:
  url = pr_url or os.environ.get('PR_URL', '')
  if url:
    match = re.search(r'github\.com/([^/]+/[^/]+)/pull/(\d+)', url)
    if match:
      return match.group(1), match.group(2)
  repo = os.environ.get('REPO', '')
  pr_number = os.environ.get('PR_NUMBER', '')
  if repo and pr_number:
    return repo, pr_number
  return None


def _get_pr_info(repo: str, pr_number: str) -> dict:
  try:
    data = _github_api_request('GET', f'repos/{repo}/pulls/{pr_number}')
    return data if isinstance(data, dict) else {}
  except (urllib.error.URLError, ValueError):
    return {}


def _extract_labels(pr_info: dict) -> Set[str]:
  return {
      item['name']
      for item in pr_info.get('labels', [])
      if isinstance(item, dict) and 'name' in item
  }


def _set_kokoro_label(repo: str, pr_number: str, should_have_label: bool):
  labels = _extract_labels(_get_pr_info(repo, pr_number))
  has_label = KOKORO_RUN_LABEL in labels
  if should_have_label and not has_label:
    print(f'Applying {KOKORO_RUN_LABEL} label to PR #{pr_number}...')
    _github_api_request(
        'POST',
        f'repos/{repo}/issues/{pr_number}/labels',
        {'labels': [KOKORO_RUN_LABEL]},
    )
  elif not should_have_label and has_label:
    print(f'Removing {KOKORO_RUN_LABEL} label from PR #{pr_number}...')
    encoded = urllib.parse.quote(KOKORO_RUN_LABEL, safe='')
    try:
      _github_api_request(
          'DELETE', f'repos/{repo}/issues/{pr_number}/labels/{encoded}')
    except urllib.error.HTTPError as exc:
      if exc.code != 404:
        raise
  else:
    state = 'already present' if has_label else 'not present'
    print(f'Label {KOKORO_RUN_LABEL} is {state}; no change needed.')


def _is_author_authorized(repo: str, author: str, author_assoc: str) -> bool:
  print(f"Author '{author}' association: {author_assoc}")
  if author_assoc in ALLOWED_AUTHOR_ASSOCIATIONS:
    return True
  try:
    data = _github_api_request(
        'GET', f'repos/{repo}/collaborators/{author}/permission')
    permission = (data or {}).get('permission', 'none')
  except (urllib.error.URLError, ValueError):
    permission = 'none'
  print(f"Author '{author}' permission: {permission}")
  return permission in ALLOWED_COLLABORATOR_PERMISSIONS


def _fetch_pr_changed_files(repo: str, pr_number: str) -> List[str]:
  files: List[str] = []
  page = 1
  while True:
    items = _github_api_request(
        'GET',
        f'repos/{repo}/pulls/{pr_number}/files?per_page=100&page={page}',
    )
    if not isinstance(items, list) or not items:
      break
    for item in items:
      if isinstance(item, dict) and item.get('filename'):
        files.append(item['filename'])
    if len(items) < 100:
      break
    page += 1
  return files


def cmd_precheck(github_output: Optional[pathlib.Path] = None) -> int:
  """Prechecks PR draft state, permissions, and fast-path files."""
  resolved = _resolve_repo_and_pr()
  if not resolved:
    raise ValueError('REPO and PR_NUMBER (or PR_URL) must be set.')
  repo, pr_number = resolved
  is_draft = os.environ.get('IS_DRAFT', '').lower() == 'true'
  event_action = os.environ.get('EVENT_ACTION', '')
  author_assoc = os.environ.get('AUTHOR_ASSOC', '')
  pr_author = os.environ.get('PR_AUTHOR', '')

  if is_draft or event_action == 'converted_to_draft':
    print(f'PR #{pr_number} is a draft. Skipping tvOS build.')
    _set_kokoro_label(repo, pr_number, should_have_label=False)
    _write_github_output(
        github_output, should_analyze='false', changed_files='[]')
    return 0

  if not _is_author_authorized(repo, pr_author, author_assoc):
    print(f"Author '{pr_author}' is not a committer. Skipping auto-labeling.")
    _write_github_output(
        github_output, should_analyze='false', changed_files='[]')
    return 0

  labels = _extract_labels(_get_pr_info(repo, pr_number))
  if KOKORO_FORCE_RUN_LABEL in labels:
    print(f'{KOKORO_FORCE_RUN_LABEL} is present. Ensuring {KOKORO_RUN_LABEL}.')
    _set_kokoro_label(repo, pr_number, should_have_label=True)
    _write_github_output(
        github_output, should_analyze='false', changed_files='[]')
    return 0

  changed_files = _fetch_pr_changed_files(repo, pr_number)
  print(f'Changed files in PR #{pr_number}: {json.dumps(changed_files)}')
  if not changed_files:
    _set_kokoro_label(repo, pr_number, should_have_label=False)
    _write_github_output(
        github_output, should_analyze='false', changed_files='[]')
    return 0

  always_match = matches_always_trigger(changed_files)
  if always_match:
    print(f'File "{always_match}" directly affects tvOS. Applying label.')
    _set_kokoro_label(repo, pr_number, should_have_label=True)
    _write_github_output(
        github_output, should_analyze='false', changed_files='[]')
    return 0

  _write_github_output(
      github_output,
      should_analyze='true',
      changed_files=json.dumps(changed_files, separators=(',', ':')),
  )
  return 0


def cmd_wait_for_label(
    github_output: Optional[pathlib.Path] = None,
    max_attempts: int = 120,
    sleep_seconds: int = 10,
) -> int:
  """Waits for Auto Label Kokoro to finish and checks if tvOS should run."""
  commit_sha = os.environ['COMMIT_SHA']
  resolved = _resolve_repo_and_pr()
  if not resolved:
    raise ValueError('REPO and PR_NUMBER (or PR_URL) must be set.')
  repo, pr_number = resolved
  is_draft = os.environ.get('IS_DRAFT', '').lower() == 'true'

  if is_draft:
    print(f'PR #{pr_number} is a draft. Skipping Kokoro build and tvOS tests.')
    _write_github_output(github_output, skipped='true')
    return 0

  print(f'Waiting for Auto Label Kokoro workflow for commit {commit_sha}...')
  runs_endpoint = (
      f'repos/{repo}/actions/workflows/auto_label_kokoro.yaml/runs'
      '?event=pull_request_target&per_page=50')
  for attempt in range(1, max_attempts + 1):
    try:
      data = _github_api_request('GET', runs_endpoint)
      runs = (data or {}).get('workflow_runs', [])
    except (urllib.error.URLError, ValueError):
      runs = []

    matching = [
        r for r in runs
        if commit_sha in (r.get('display_title') or '')
    ]
    if matching:
      run_info = matching[0]
      status = run_info.get('status')
      conclusion = run_info.get('conclusion')
      print(f'Auto Label Kokoro status: {status} (conclusion: {conclusion})')
      if status == 'completed':
        break
    elif attempt >= 3:
      in_progress = [r for r in runs if r.get('status') != 'completed']
      if not in_progress:
        print('No active Auto Label Kokoro run found; checking PR labels.')
        break

    print(
        f'Waiting {sleep_seconds}s for Auto Label Kokoro '
        f'(attempt {attempt}/{max_attempts})...')
    time.sleep(sleep_seconds)

  pr_data = _get_pr_info(repo, pr_number)
  if pr_data.get('draft'):
    print(f'PR #{pr_number} is now a draft. Skipping tvOS build and tests.')
    _write_github_output(github_output, skipped='true')
    return 0

  labels = _extract_labels(pr_data)
  if KOKORO_RUN_LABEL not in labels and KOKORO_FORCE_RUN_LABEL not in labels:
    print(
        f'Neither {KOKORO_RUN_LABEL} nor {KOKORO_FORCE_RUN_LABEL} is present '
        f'on PR #{pr_number}. Skipping Kokoro build and tvOS tests.')
    _write_github_output(github_output, skipped='true')
    return 0

  print(f'Kokoro label is present on PR #{pr_number}. Proceeding...')
  _write_github_output(github_output, skipped='false')
  return 0


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
      '--files-json',
      type=str,
      default=None,
      help='JSON array of modified file paths.',
  )
  parser.add_argument(
      '--pr-url',
      type=str,
      default=None,
      help='Optional PR URL to update kokoro:run label after analysis.',
  )
  parser.add_argument(
      '--update-label',
      action='store_true',
      help='Update kokoro:run label on PR using REPO/PR_NUMBER or --pr-url.',
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
      help='Subcommand (precheck, wait-for-label) or modified file paths.',
  )
  args = parser.parse_args(argv)

  if len(args.files) == 1 and not args.files_json:
    if args.files[0] == 'precheck':
      return cmd_precheck(args.github_output)
    if args.files[0] == 'wait-for-label':
      return cmd_wait_for_label(args.github_output)

  files: List[str] = list(args.files)
  if args.files_json:
    parsed = json.loads(args.files_json)
    if not isinstance(parsed, list):
      raise ValueError('--files-json must decode to a JSON list of strings.')
    files.extend(str(item) for item in parsed)

  try:
    affects = check_tvos_impact(args.src_dir.resolve(), files, args.out_dir)
  except Exception as exc:  # pylint: disable=broad-exception-caught
    print(
        f'Error while checking tvOS impact ({exc}); '
        'defaulting to affects_tvos=true.',
        file=sys.stderr,
    )
    affects = True

  value = 'true' if affects else 'false'
  _write_github_output(args.github_output, affects_tvos=value)

  if args.update_label or args.pr_url:
    resolved = _resolve_repo_and_pr(args.pr_url)
    if resolved:
      _set_kokoro_label(resolved[0], resolved[1], should_have_label=affects)

  return 0


if __name__ == '__main__':
  sys.exit(main())
