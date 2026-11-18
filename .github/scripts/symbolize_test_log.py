#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Symbolizes Starboard stack dumps in an on-host Evergreen test log in place.

Evergreen test libraries are stripped at link time. When the in-process
symbolizer can't resolve a frame inside the Evergreen library, Starboard
prints its offset relative to the library's load address instead, e.g.
`<unknown> [0x174dd22]`. This script resolves those frames against the
unstripped copy of the library in lib.unstripped/ using
starboard/tools/symbolize.

The script never fails the calling step: if there is nothing to symbolize or
any input is missing, the log is left untouched.
"""

import argparse
import os
import re
import sys
import tempfile

# Matches the elf_loader log line naming the Evergreen library, e.g.
# `...:INFO:elf_loader_impl.cc(59)] Loading /path/to/libnplb.so`.
_RE_LOADED_LIBRARY = re.compile(r'elf_loader_impl\.cc\(\d+\)\] Loading (\S+)')
_UNSYMBOLIZED_FRAME = '<unknown> [0x'


def _find_loaded_libraries(log_path: str) -> list[str]:
  names = []
  with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
    for line in f:
      match = _RE_LOADED_LIBRARY.search(line)
      if match:
        name = os.path.basename(match.group(1))
        if name not in names:
          names.append(name)
  return names


def _has_unsymbolized_frames(log_path: str) -> bool:
  with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
    return any(_UNSYMBOLIZED_FRAME in line for line in f)


def symbolize_log(log_path: str, out_dir: str, source_dir: str) -> bool:
  """Returns True if the log was rewritten with symbolized frames."""
  if not os.path.isfile(log_path) or not _has_unsymbolized_frames(log_path):
    return False

  libraries = _find_loaded_libraries(log_path)
  if len(libraries) != 1:
    print(f'Not symbolizing {log_path}: expected exactly one Evergreen '
          f'library in the log, found {libraries}.')
    return False

  unstripped = os.path.join(out_dir, 'lib.unstripped', libraries[0])
  if not os.path.isfile(unstripped):
    print(f'Not symbolizing {log_path}: {unstripped} not found.')
    return False

  symbolizer = os.path.join(source_dir, 'third_party', 'llvm-build',
                            'Release+Asserts', 'bin', 'llvm-symbolizer')
  if not os.access(symbolizer, os.X_OK):
    print(f'Not symbolizing {log_path}: {symbolizer} not found.')
    return False

  # The symbolize package reads the symbolizer path at import time.
  os.environ['LLVM_SYMBOLIZER_PATH'] = symbolizer
  if source_dir not in sys.path:
    sys.path.insert(0, source_dir)
  # pylint: disable=import-outside-toplevel
  from starboard.tools.symbolize import symbolize

  log_dir = os.path.dirname(os.path.abspath(log_path))
  with tempfile.NamedTemporaryFile(
      'w', encoding='utf-8', dir=log_dir, delete=False) as out:
    tmp_path = out.name
    try:
      with open(log_path, 'r', encoding='utf-8', errors='replace') as log:
        symbolize.symbolize_stream(
            in_stream=log, out_stream=out, library=unstripped)
    except Exception:  # pylint: disable=broad-except
      os.unlink(tmp_path)
      raise
  os.replace(tmp_path, log_path)
  print(f'Symbolized {log_path} using {unstripped}.')
  return True


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--log', required=True, help='Test log to symbolize.')
  parser.add_argument(
      '--out-dir',
      required=True,
      help='Build output directory containing lib.unstripped/.')
  parser.add_argument(
      '--source-dir',
      default=os.getcwd(),
      help='Source root containing starboard/tools/symbolize and '
      'third_party/llvm-build.')
  args = parser.parse_args()
  try:
    symbolize_log(args.log, args.out_dir, os.path.abspath(args.source_dir))
  except Exception as e:  # pylint: disable=broad-except
    print(f'Failed to symbolize {args.log}: {e}')
  return 0


if __name__ == '__main__':
  sys.exit(main())
