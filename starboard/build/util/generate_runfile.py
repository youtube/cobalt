#!/usr/bin/env python3
"""
Generate runfiles for evergreen.
"""
import argparse
import os
import stat

# TODO: b/434191035 - Allowing for either elf_loader_sandbox path should be
# removed once CI has been updated to support the preferred path.
_TEMPLATE = """#!/usr/bin/env python3
import os
import subprocess
import sys

preferred_path = os.path.join(os.path.dirname(__file__), 'elf_loader_sandbox')
path_to_use = preferred_path
if not os.path.exists(preferred_path):
  path_to_use = '{outdir}/elf_loader_sandbox'

command = [
    path_to_use,
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

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=str, required=True)

# TODO: b/434191035 - Remove once CI supports preferred path
parser.add_argument('--outdir', type=str, required=True)
parser.add_argument('--library', type=str, required=True)
args = parser.parse_args()

with open(args.output, 'w', encoding='utf-8') as f:
  f.write(_TEMPLATE.format(outdir=args.outdir, library=args.library))
current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
os.chmod(args.output, new_permissions)
