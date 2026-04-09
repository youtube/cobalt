#!/usr/bin/env python3
"""
Generate runfiles for evergreen.
"""
import argparse
import os
import stat

_TEMPLATE = """#!/usr/bin/env python3
import os
import subprocess
import sys

command = [
    {runner_args}os.path.join(os.path.dirname(__file__), 'elf_loader_sandbox'),
    '--evergreen_content=.', '--evergreen_library={library}.so'
] + sys.argv[1:]
try:
    result = subprocess.run(command, check=False)
    sys.exit(result.returncode)
except subprocess.CalledProcessError:
    # A subprocess failed, so don't log the python traceback.
    raise SystemExit(1)
except Exception as e:
    print("An unexpected error occurred: " + str(e), file=sys.stderr)
    sys.exit(1)
"""

_BROWSERTEST_RUNNER_ARGS = (
    'sys.executable, '
    'os.path.join(os.path.dirname(__file__), "cobalt_browsertests_runner"), ')

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=str, required=True)
parser.add_argument('--library', type=str, required=True)
args = parser.parse_args()

with open(args.output, 'w', encoding='utf-8') as f:
  if args.library == 'libcobalt_browsertests':
    runner_args = _BROWSERTEST_RUNNER_ARGS
  else:
    runner_args = ''

  f.write(_TEMPLATE.format(runner_args=runner_args, library=args.library))

current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
os.chmod(args.output, new_permissions)
