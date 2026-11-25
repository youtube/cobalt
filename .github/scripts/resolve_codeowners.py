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
"""Resolve CODEOWNERS or reviewer frontmatter paths to critic reviewers."""

from pathlib import Path
import re
import subprocess
import sys
import yaml


def match(path: str, pat: str) -> bool:
  p = pat.strip().removeprefix('./')
  anc = p.startswith('/') or '/' in p.rstrip('/')
  p = p.lstrip('/')
  rx = re.escape(p).replace(r'\*\*/',
                            '(?:.+/)?').replace(r'\*\*', '.*').replace(
                                r'\*', '[^/]*').replace(r'\?', '[^/]')
  tail = '.*' if p.endswith('/') else (
      '$' if '*' in p or '?' in p else '(?:$|/.*)')
  return bool(p and re.match(('^' if anc else '(?:^|.+/)') + rx + tail, path))


def main() -> int:
  args = [a for a in sys.argv[1:] if a != '--codeowners']
  raw = [
      a for a in args if a != '--paths'
  ] or (sys.stdin.read() if not sys.stdin.isatty() else subprocess.check_output(
      ['git', 'diff', '--name-only', 'HEAD'], text=True)).splitlines()
  files = [x.strip().removeprefix('./').lstrip('/') for x in raw if x.strip()]
  co = Path('.github/CODEOWNERS')
  rules = [(w[0], w[1:])
           for ln in (co.read_text('utf-8').splitlines() if co.exists() else ())
           if (w := ln.split('#', 1)[0].split())]
  if '--paths' not in args:
    revs = {
        ('cobalt-starboard' if
         (t := o.removeprefix('@youtube/').removesuffix('-repository-owners')
          .removesuffix('-owners')) == 'nplb-filters' else t) + '-reviewer.md'
        for f in files
        for o in next((ow for pat, ow in reversed(rules) if match(f, pat)), ())
    }
    return print('\n'.join(sorted(revs))) or 0 if revs else 0
  for d in (Path('.github/reviewers'), Path('.agent/reviewers')):
    for md in sorted(d.glob('*.md')):
      parts = text.split(
          '---', 2) if (text := md.read_text('utf-8')).startswith('---') else ()
      try:
        meta = yaml.safe_load(parts[1]) if len(parts) >= 3 else None
      except yaml.YAMLError:
        continue
      if isinstance(meta, dict) and isinstance(meta.get('paths'), list):
        teams = set(meta['codeowner_teams']) if isinstance(
            meta['codeowner_teams'], list) else set()
        if any(
            any(match(f, p)
                for p in meta['paths']
                if isinstance(p, str)) and (not teams or not (ow := next(
                    (o
                     for pat, o in reversed(rules)
                     if match(f, pat)), None)) or not teams.isdisjoint(ow))
            for f in files):
          print(md)
  return 0


if __name__ == '__main__':
  sys.exit(main())
