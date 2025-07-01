#!/usr/bin/env python3
"""
Generate runfiles for evergreen.
"""
import sys

_TEMPLATE = """#!/usr/bin/env python3
import subprocess
import sys

command = [
    '{}/elf_loader_sandbox', '--evergreen_content=.',
    '--evergreen_library={}.so'
] + sys.argv[1:]
result = subprocess.run(command, check=False)
try:
    result = sys.exit(result.returncode)
except FileNotFoundError:
    print(f"Error: The command '{{command[0]}}' was not found.", file=sys.stderr)
    print("Please ensure that 'elf_loader_sandbox' is in your system's PATH.",
          file=sys.stderr)
    sys.exit(1)
except subprocess.CalledProcessError:
    # A subprocess failed, so don't log the python traceback.
    raise SystemExit(1)
except Exception as e:
    print(f"An unexpected error occurred: {{e}}", file=sys.stderr)
    sys.exit(1)
"""

with open(sys.argv[1], 'w', encoding='utf-8') as f:
  f.write(_TEMPLATE.format(sys.argv[2], sys.argv[3]))
