#!/usr/bin/env python3

import platform
import subprocess
import sys

if __name__ == '__main__':
  if platform.system() != 'Linux':
    sys.exit(0)

  google_java_format_args = sys.argv[1:]
  sys.exit(subprocess.call(['google-java-format'] + google_java_format_args))
