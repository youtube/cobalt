#!/usr/bin/env python3
"""
Generate runfiles for evergreen.
"""
import argparse
import os
import stat

TEMPLATE = """#!/usr/bin/env python3
import os
import subprocess
import sys

def get_crashed_test(lines):
  RUN_MARKER = '[ RUN      ]'
  END_MARKERS = (
      '[       OK ]',
      '[  FAILED  ]',
      '[  SKIPPED ]',
  )
  for i, line in reversed(list(enumerate(lines))):
    if RUN_MARKER in line:
      log = ''.join(lines[i:])
      # If the test crashed there are no end markers.
      if any(marker in log for marker in END_MARKERS):
        break
      test_name = line[len(RUN_MARKER):].strip()
      return test_name
  return None

{pre_run}
cmd_args = sys.argv[1:]
output_file = None
for arg in cmd_args:
  if arg.startswith('--gtest_output=xml:'):
    output_file = arg.split(':', 1)[1]
    break

command_base = [
    os.path.join(os.path.dirname(__file__), 'elf_loader_sandbox'),
    '--evergreen_content=.', '--evergreen_library={library}.so'
]

max_retries = 500
return_code = 1

for i in range(max_retries):
  command = command_base + cmd_args
  lines = []
  try:
    print('Running', ' '.join(command))
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1) as proc:
      for line in proc.stdout:
        sys.stdout.write(line)
        lines.append(line)
      return_code = proc.wait()
  except Exception as e:
    print("An unexpected error occurred: " + str(e), file=sys.stderr)
    sys.exit(1)

  if output_file and os.path.exists(output_file):
    print('Success with args:', ' '.join(cmd_args))
    sys.exit(return_code)

  if output_file:
    crashed_test = get_crashed_test(lines)
    if crashed_test:
      found = False
      for idx, arg in enumerate(cmd_args):
        if arg.startswith('--gtest_filter='):
          old_filter = arg.split('=', 1)[1]
          if old_filter == '*':
            new_filter = f'-{{crashed_test}}'
          elif old_filter.startswith('-') or ':-' in old_filter:
            new_filter= f'{{old_filter}}:{{crashed_test}}'
          else
            new_filter= f'{{old_filter}}:-{{crashed_test}}'

          cmd_args[idx] = f'--gtest_filter={{new_filter}}'
          found = True
          break
      if not found:
        cmd_args.append(f'--gtest_filter=-{{crashed_test}}')
      continue
    break
  else:
    break

sys.exit(return_code)
{post_run}
"""


def generate():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output', type=str, required=True)
  parser.add_argument('--library', type=str, required=True)
  parser.add_argument('--pre-run', type=str, default='')
  parser.add_argument('--post-run', type=str, default='')
  args = parser.parse_args()

  with open(args.output, 'w', encoding='utf-8') as f:
    f.write(
        TEMPLATE.format(
            library=args.library, pre_run=args.pre_run, post_run=args.post_run))
  current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
  new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
  os.chmod(args.output, new_permissions)


if __name__ == '__main__':
  generate()
