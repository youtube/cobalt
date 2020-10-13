#!/usr/bin/env python3

import os
import platform
import subprocess
import sys

if __name__ == '__main__':
  clang_format_args = sys.argv[1:]
  path_to_clang_format = [os.getcwd(), 'buildtools']

  system = platform.system()
  if system == 'Linux':
    path_to_clang_format += ['linux64', 'clang-format']
  elif system == 'Darwin':
    path_to_clang_format += ['mac', 'clang-format']
  elif system == 'Windows':
    path_to_clang_format += ['win', 'clang-format.exe']
  else:
    sys.exit(1)

  clang_format_executable = os.path.join(*path_to_clang_format)
  sys.exit(subprocess.call([clang_format_executable] + clang_format_args))
