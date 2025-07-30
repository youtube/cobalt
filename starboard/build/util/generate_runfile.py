#!/usr/bin/env python3
"""
Generate runfiles for evergreen.
"""
import argparse
import os
import stat

_TEMPLATE = """#!/usr/bin/env python3
import subprocess
import sys

command = [
    '{outdir}/elf_loader_sandbox', '--evergreen_content=.',
    '--evergreen_library={library}.so'
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
parser.add_argument('--outdir', type=str, required=True)
parser.add_argument('--library', type=str, required=True)
args = parser.parse_args()

with open(args.output, 'w', encoding='utf-8') as f:
  f.write(_TEMPLATE.format(outdir=args.outdir, library=args.library))
current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
os.chmod(args.output, new_permissions)
