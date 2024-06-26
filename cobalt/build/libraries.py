#!/usr/bin/env python3
"""Extract names and versions of imported libraries."""

import os
import subprocess
import sys

output_path = sys.argv[1]
source_root = sys.argv[2]

with open(output_path, 'w', encoding='utf-8') as out:
  print('name\tversion', file=out)  # header
  os.chdir(source_root)
  args = ['git', 'ls-files', '--', '*/README.*']
  p = subprocess.run(args, capture_output=True, text=True, check=True)
  for f in [f for f in p.stdout.splitlines() if not f.endswith('template')]:
    try:
      with open(f, encoding='utf-8') as file:
        name = None
        version = None
        for line in file.readlines():
          if line.startswith('Name: '):
            name = line[len('Name: '):].strip()
          if line.startswith('Version: '):
            version = line[len('Version: '):].strip()
        if name and version:
          print(name + '\t' + version, file=out)  # row
    except UnicodeDecodeError:
      pass
