#!/usr/bin/env python3
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Lightweight utility to simplify resolving stack traces and crashes.

This tool supports multiple formats for crashes and stack traces (ASan,
Cobalt stack dumps, Android logcat/tombstones, GDB, and raw addresses) across
multiple dynamically loaded libraries and persistent streaming sessions.
"""

import argparse
import io
import os
import sys
from typing import Iterable, List, Optional, Union

_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
if _SRC_DIR not in sys.path:
  sys.path.insert(0, _SRC_DIR)

from starboard.tools.symbolize.detector import StreamingSessionTracker  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import FormatRegistry  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import RawFormatHandler  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import _RE_ANDROID  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import _RE_ASAN_MODE1 as _RE_ASAN  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import _RE_COBALT  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import _RE_GDB  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.formats import _RE_RAW  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.json_processor import process_test_summary_json  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.runner import DEFAULT_SYMBOLIZER as _SYMBOLIZER  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.runner import SymbolizerRunner  # pylint: disable=wrong-import-position
from starboard.tools.symbolize.runner import _SymbolizerRunner  # pylint: disable=wrong-import-position

__all__ = [
    '_SymbolizerRunner',
    'SymbolizerRunner',
    '_Symbolize',
    'symbolize_stream',
    'symbolize_string',
    'main',
    '_RE_ASAN',
    '_RE_ANDROID',
    '_RE_COBALT',
    '_RE_RAW',
    '_RE_GDB',
    '_SYMBOLIZER',
]


# pylint: disable=too-many-arguments,too-many-positional-arguments,invalid-name,unused-argument
def _Symbolize(filename: Optional[str] = None,
               library: Optional[str] = None,
               base_address: Optional[Union[str, int]] = '0',
               in_stream: Optional[Iterable[str]] = None,
               out_stream: Optional[io.TextIOBase] = None,
               runner: Optional[SymbolizerRunner] = None,
               strip_prefixes: Optional[List[str]] = None):
  """Attempts to resolve memory addresses within the file or stream specified.

  Args:
    filename: Path to the file containing stack traces.
    library: Path to the default library containing debug symbols.
    base_address: Base address string ('0x...' or decimal), int, or None.
    in_stream: Optional input iterable/stream of lines.
    out_stream: Optional output stream (defaults to sys.stdout).
    runner: Optional SymbolizerRunner instance.
    strip_prefixes: Optional list of source directory prefixes to strip.
  """
  if in_stream is None:
    if not filename or not os.path.exists(filename):
      raise ValueError(f'File not found: {filename}.')
  if runner is None:
    if not library or not os.path.exists(library):
      raise ValueError(f'Library not found: {library}.')

  out = out_stream if out_stream is not None else sys.stdout
  if base_address is None:
    base = None
  elif isinstance(base_address, int):
    base = base_address
  else:
    base = int(str(base_address), 0)

  active_runner = runner if runner is not None else SymbolizerRunner(library)
  tracker = StreamingSessionTracker(default_base_address=base)
  registry = FormatRegistry()

  def _process_lines(lines):
    for line in lines:
      tracker.check_line(line)
      match_pair = registry.match(line)
      if not match_pair:
        out.write(line)
        continue

      handler, frame_match = match_pair
      offset, _ = tracker.resolve_offset(
          address=frame_match.address,
          explicit_offset=frame_match.explicit_offset,
          runner=active_runner,
          binary=frame_match.binary)

      if offset is None:
        out.write(line)
        continue

      try:
        results = active_runner.symbolize(str(offset), frame_match.binary)
      except TypeError:
        results = active_runner.symbolize(str(offset))

      if isinstance(handler, RawFormatHandler) and (not results or
                                                    '?' in results[0][0]):
        out.write(f'{hex(offset)} ??\n')
        continue

      if not results or '?' in results[0][0]:
        out.write(line)
        continue

      formatted_lines = handler.format(frame_match, results, offset)
      for fl in formatted_lines:
        out.write(fl)

  try:
    if in_stream is not None:
      _process_lines(in_stream)
    else:
      with open(filename, 'r', encoding='utf-8') as f:
        _process_lines(f)
  finally:
    if runner is None:
      active_runner.close()


# pylint: disable=too-many-arguments,too-many-positional-arguments
def symbolize_stream(in_stream: Iterable[str],
                     out_stream: io.TextIOBase,
                     library: Optional[str] = None,
                     runner: Optional[SymbolizerRunner] = None,
                     base_address: Optional[Union[str, int]] = '0',
                     strip_prefixes: Optional[List[str]] = None):
  """Convenience function to symbolize from an in-stream to an out-stream."""
  return _Symbolize(
      in_stream=in_stream,
      out_stream=out_stream,
      library=library,
      runner=runner,
      base_address=base_address,
      strip_prefixes=strip_prefixes)


def symbolize_string(text: str,
                     library: Optional[str] = None,
                     runner: Optional[SymbolizerRunner] = None,
                     base_address: Optional[Union[str, int]] = '0',
                     strip_prefixes: Optional[List[str]] = None) -> str:
  """Convenience function to symbolize an in-memory string."""
  in_stream = io.StringIO(text)
  out_stream = io.StringIO()
  symbolize_stream(
      in_stream=in_stream,
      out_stream=out_stream,
      library=library,
      runner=runner,
      base_address=base_address,
      strip_prefixes=strip_prefixes)
  return out_stream.getvalue()


def main():
  arg_parser = argparse.ArgumentParser(description='Symbolize stack traces.')
  arg_parser.add_argument(
      '-f',
      '--filename',
      help='Path to file containing stack traces, crashes, or raw addresses.')
  arg_parser.add_argument(
      '-l',
      '--library',
      help='Path to library believed to contain the addresses.')
  arg_parser.add_argument(
      'base_address',
      type=str,
      nargs='?',
      default='0',
      help='The base address of the library.')
  arg_parser.add_argument(
      '--extra-binary',
      default=None,
      help='Path to dynamic binary for which to symbolize stack traces.')
  arg_parser.add_argument(
      '--test-summary-json-file',
      help='Path to a JSON file produced by the test launcher.')
  arg_parser.add_argument(
      'strip_path_prefix',
      nargs='*',
      help='When printing source file names, prefixes to strip.')
  args = arg_parser.parse_args()

  if not os.path.exists(_SYMBOLIZER):
    raise ValueError(
        f'Please update {__file__} with a valid llvm-symbolizer path.')

  default_lib = args.library or args.extra_binary
  base_address = args.base_address

  if args.test_summary_json_file:
    with SymbolizerRunner(default_library=default_lib) as runner:

      def symbolize_lines_fn(lines):
        return symbolize_string(
            ''.join(lines),
            runner=runner,
            base_address=base_address,
            strip_prefixes=args.strip_path_prefix)

      process_test_summary_json(args.test_summary_json_file, symbolize_lines_fn)
    return 0

  if args.strip_path_prefix:
    return _Symbolize(
        args.filename,
        default_lib,
        base_address,
        strip_prefixes=args.strip_path_prefix)
  return _Symbolize(args.filename, default_lib, base_address)


if __name__ == '__main__':
  sys.exit(main())
