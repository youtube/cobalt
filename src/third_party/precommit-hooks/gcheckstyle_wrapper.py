#!/usr/bin/env python3

import platform
import subprocess
import sys

try:
  from internal_tools_paths import checkstyle_path
except ImportError:
  checkstyle_path = None

if __name__ == '__main__':
  gcheckstyle_args = sys.argv[1:]
  try:
    sys.exit(subprocess.call([checkstyle_path] + gcheckstyle_args))
  except OSError:
    print('Checkstyle not found, skipping.')
    sys.exit(0)
