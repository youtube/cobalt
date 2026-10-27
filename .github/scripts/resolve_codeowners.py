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
"""Match PR diff against reviewer persona path globs."""

import fnmatch
from pathlib import Path
import sys
import yaml


def main() -> int:
  if len(sys.argv) < 2 or not Path(sys.argv[1]).exists():
    return 0

  diff = Path(sys.argv[1]).read_text(encoding='utf-8', errors='replace')
  files = [
      line.split()[2][2:]
      for line in diff.splitlines()
      if line.startswith('diff --git a/')
  ]

  for md in sorted(Path('.agent/reviewers').glob('*.md')):
    parts = md.read_text(encoding='utf-8').split('---', 2)
    if len(parts) < 3:
      continue
    meta = yaml.safe_load(parts[1]) or {}
    patterns = [p.removeprefix('./') for p in meta.get('paths', [])]
    if any(
        fnmatch.fnmatch(f.removeprefix('./'), p)
        for f in files
        for p in patterns):
      print(str(md))
  return 0


if __name__ == '__main__':
  sys.exit(main())
