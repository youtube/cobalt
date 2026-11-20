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
"""Resolve CODEOWNERS or reviewer frontmatter paths to critic reviewers.

Modes:
  Default (--codeowners): Match changed files against .github/CODEOWNERS and
    output sorted `<team>-reviewer.md` filenames.
  Path-triggered (--paths): Match changed files against `paths:` globs in
    reviewer markdown YAML frontmatter and output sorted reviewer file paths.
    When a reviewer declares `codeowner_teams:` and `.github/CODEOWNERS`
    assigns a matched file exclusively to other teams (for example,
    `/.github/CODEOWNERS` owned by `@youtube/cobalt-starboard-owners`), the
    CODEOWNERS override excludes that file from triggering that reviewer.
"""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import yaml

CODEOWNERS_PATH = Path('.github/CODEOWNERS')
REVIEWER_DIRS = (
    Path('.github/reviewers'),
    Path('.agent/reviewers'),
    Path('.agent/skills/critic_review/resources/reviewers'),
)
TEAM_ALIASES = {'nplb-filters': 'cobalt-starboard'}


def _compile_pattern(pattern: str) -> re.Pattern[str] | None:
  """Compile a CODEOWNERS or frontmatter path glob into a regex."""
  raw_pat = pattern.strip().removeprefix('./')
  if not raw_pat:
    return None

  anchored = raw_pat.startswith('/') or '/' in raw_pat.rstrip('/')
  pat = raw_pat.lstrip('/')
  if not pat:
    return re.compile('.*')

  is_dir = pat.endswith('/')
  has_glob = any(ch in pat for ch in '*?[')

  parts = ['^' if anchored else '(?:^|.+/)']
  i = 0
  while i < len(pat):
    if pat.startswith('**/', i):
      parts.append('(?:.+/)?')
      i += 3
    elif pat.startswith('**', i):
      parts.append('.*')
      i += 2
    elif pat[i] == '*':
      parts.append('[^/]*')
      i += 1
    elif pat[i] == '?':
      parts.append('[^/]')
      i += 1
    else:
      parts.append(re.escape(pat[i]))
      i += 1

  if is_dir:
    parts.append('.*')
  elif not has_glob:
    parts.append('(?:$|/.*)')
  else:
    parts.append('$')

  return re.compile(''.join(parts))


def match_pattern(path: str, pattern: str) -> bool:
  """Return True if repo-relative `path` matches CODEOWNERS/glob `pattern`."""
  norm_path = path.strip().removeprefix('./').lstrip('/')
  if not norm_path:
    return False
  regex = _compile_pattern(pattern)
  return bool(regex and regex.match(norm_path))


def load_codeowners(
    codeowners_path: Path = CODEOWNERS_PATH,) -> list[tuple[str, list[str]]]:
  """Parse `.github/CODEOWNERS` into `(pattern, owners)` tuples."""
  if not codeowners_path.exists():
    return []
  rules: list[tuple[str, list[str]]] = []
  with codeowners_path.open('r', encoding='utf-8') as handle:
    for line in handle:
      clean = line.split('#', 1)[0].strip()
      if clean:
        parts = clean.split()
        rules.append((parts[0], parts[1:]))
  return rules


def owners_for_file(
    file_path: str,
    rules: list[tuple[str, list[str]]],
) -> list[str] | None:
  """Return last-matching CODEOWNERS teams for `file_path`, or None."""
  for pattern, owners in reversed(rules):
    if match_pattern(file_path, pattern):
      return owners
  return None


def _owner_to_reviewer(owner: str) -> str:
  team = (
      owner.removeprefix('@youtube/').removesuffix(
          '-repository-owners').removesuffix('-owners'))
  name = TEAM_ALIASES.get(team, team)
  return f'{name}-reviewer.md'


def resolve_codeowners(
    files: list[str],
    rules: list[tuple[str, list[str]]],
) -> list[str]:
  """Resolve changed files to sorted `<name>-reviewer.md` filenames."""
  reviewers: set[str] = set()
  for file_path in files:
    owners = owners_for_file(file_path, rules)
    if owners:
      for owner in owners:
        reviewers.add(_owner_to_reviewer(owner))
  return sorted(reviewers)


def _parse_frontmatter(md_path: Path) -> dict | None:
  text = md_path.read_text(encoding='utf-8')
  if not text.startswith('---'):
    return None
  parts = text.split('---', 2)
  if len(parts) < 3:
    return None
  try:
    meta = yaml.safe_load(parts[1])
  except yaml.YAMLError:
    return None
  return meta if isinstance(meta, dict) else None


def resolve_paths(
    files: list[str],
    rules: list[tuple[str, list[str]]],
) -> list[str]:
  """Match changed files against reviewer YAML frontmatter `paths:` globs."""
  matched: list[str] = []
  for rdir in REVIEWER_DIRS:
    for md_path in sorted(rdir.glob('*.md')):
      meta = _parse_frontmatter(md_path)
      if not meta:
        continue
      paths = meta.get('paths', [])
      if not isinstance(paths, list):
        continue
      patterns = [p for p in paths if isinstance(p, str)]
      raw_teams = meta.get('codeowner_teams', [])
      declared_teams = ({t for t in raw_teams if isinstance(t, str)}
                        if isinstance(raw_teams, list) else set())
      for file_path in files:
        if not any(match_pattern(file_path, pat) for pat in patterns):
          continue
        if declared_teams:
          file_owners = owners_for_file(file_path, rules)
          if file_owners is not None and declared_teams.isdisjoint(file_owners):
            continue
        matched.append(str(md_path))
        break
  return sorted(set(matched))


def _read_files(cli_files: list[str]) -> list[str]:
  if cli_files:
    raw = cli_files
  elif not sys.stdin.isatty():
    raw = sys.stdin.read().splitlines()
  else:
    raw = subprocess.check_output(
        ['git', 'diff', '--name-only', 'HEAD'],
        text=True,
    ).splitlines()
  return [x.strip().removeprefix('./').lstrip('/') for x in raw if x.strip()]


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  mode_group = parser.add_mutually_exclusive_group()
  mode_group.add_argument(
      '--codeowners',
      action='store_true',
      help='Resolve changed files via .github/CODEOWNERS (default).',
  )
  mode_group.add_argument(
      '--paths',
      action='store_true',
      help='Match changed files against reviewer frontmatter paths.',
  )
  parser.add_argument('files', nargs='*', help='Changed file paths.')
  args = parser.parse_args(argv)

  files = _read_files(args.files)
  if not files:
    return 0

  rules = load_codeowners()
  results = (
      resolve_paths(files, rules) if args.paths else resolve_codeowners(
          files, rules))
  for item in results:
    print(item)
  return 0


if __name__ == '__main__':
  sys.exit(main())
