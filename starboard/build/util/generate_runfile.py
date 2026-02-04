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
    os.path.join(os.path.dirname(__file__), 'elf_loader_sandbox'),
    '--evergreen_content=app/{library_target}/content', '--evergreen_library=lib{library_target}.so'
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

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=str, required=True)
parser.add_argument('--library_target', type=str, required=True)
args = parser.parse_args()

with open(args.output, 'w', encoding='utf-8') as f:
  f.write(_TEMPLATE.format(library_target=args.library_target))
current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
os.chmod(args.output, new_permissions)
