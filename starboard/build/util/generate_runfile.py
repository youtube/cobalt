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

{env_setup}command = [
    os.path.join(os.path.dirname(__file__), 'elf_loader_sandbox'),
    '--evergreen_content=.', '--evergreen_library={library}.so'
] + sys.argv[1:]

if {is_browsertest}:
  command = [sys.executable, os.path.join(os.path.dirname(__file__),
      'cobalt_browsertests_runner'), *command]

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

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=str, required=True)
parser.add_argument('--library', type=str, required=True)
parser.add_argument(
    '--env',
    action='append',
    default=[],
    metavar='NAME=VALUE',
    help='Environment variable defaults to set in the generated runfile.')
args = parser.parse_args()

env_setup = ''
for env in args.env:
  name, _, value = env.partition('=')
  env_setup += f"os.environ.setdefault('{name}', '{value}')\n"
if env_setup:
  env_setup += '\n'

with open(args.output, 'w', encoding='utf-8') as f:
  f.write(
      _TEMPLATE.format(
          is_browsertest=(args.library == 'libcobalt_browsertests'),
          library=args.library,
          env_setup=env_setup))

current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
os.chmod(args.output, new_permissions)
