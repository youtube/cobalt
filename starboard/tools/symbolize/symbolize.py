#!/usr/bin/env python

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

This tool supports three different formats for crashes and stack traces, but can
easily be expanded for addition cases. Examples of current formats are as
follows:

  Address Sanitizer
    #1 0x7fdc59bbaa6b  (<unknown module>)

  Cobalt
    <unknown> [0x7efcdf1fd52b]

  Raw
    0x7efcdf1fd52b

The results of the symbolizer will only be included if it was able to find the
name of the symbol, and it does not appear to be malformed. The only exception
is when the line was matched with the |_RAW| regular expression in which case it
will always output the results of the symbolizer.
"""

import argparse
import os
import re
import subprocess
import sys

from starboard.build import clang
from starboard.tools import build

_SYMBOLIZER = os.path.join(build.GetToolchainsDir(),
                           (f'x86_64-linux-gnu-clang-chromium-'
                            f'{clang.GetClangSpecification().revision}'), 'bin',
                           'llvm-symbolizer')

_RE_ASAN = re.compile(
    r'\s*(#[0-9]{1,3})\s*(0x[a-z0-9]*)\s*\(<unknown\smodule>\)')
_RE_COBALT = re.compile(r'\s*<unknown> \[(0x[a-z0-9]*)\]\s*')
_RE_RAW = re.compile(r'^(0x[a-z0-9]*)$')


def _Symbolize(filename, library, base_address):
  """Attempts to resolve memory addresses within the file specified.

  This function iterates through the file specified line by line. When a line is
  found that matches one of our regular expressions it will stop and invoke
  llvm-symbolizer with the offset of the symbol and the library specified. The
  results are verified and the output formatted to match whichever crash-style
  is being used.

  Args:
    filename:     The path to the file containing the stack trace.
    library:      The path to the library that is believed to have the symbol.
    base_address: The base address of the library when it was loaded and
      crashed, typically found in the logs.
  """
  if not os.path.exists(filename):
    raise ValueError(f'File not found: {filename}.')
  if not os.path.exists(library):
    raise ValueError(f'Library not found: {library}.')
  with open(filename, encoding='utf-8') as f:
    for line in f:
      # Address Sanitizer
      match = _RE_ASAN.match(line)
      if match:
        offset = int(match.group(2), 0) - int(base_address, 0)
        results = _RunSymbolizer(library, str(offset))
        if results and '?' not in results[0] and '?' not in results[1]:
          sys.stdout.write(f'    {match.group(1)} {hex(offset)} in '
                           f'{results[0]} {results[1]}\n')
          continue
      # Cobalt
      match = _RE_COBALT.match(line)
      if match:
        offset = int(match.group(1), 0) - int(base_address, 0)
        results = _RunSymbolizer(library, str(offset))
        if results and '?' not in results[0]:
          sys.stdout.write(f'        {hex(offset)} [{results[0]}]\n')
          continue
      # Raw
      match = _RE_RAW.match(line)
      if match:
        offset = int(match.group(1), 0) - int(base_address, 0)
        results = _RunSymbolizer(library, str(offset))
        if results:
          sys.stdout.write(f'{hex(offset)} {results[0]} in {results[1]}\n')
          continue
      sys.stdout.write(line)


def _RunSymbolizer(library, offset):
  """Uses a external symbolizer tool to resolve symbol names.

  Args:
    library: The path to the library that is believed to have the symbol.
    offset:  The offset into the library of the symbol we are looking for.
  """
  if int(offset) >= 0:
    command = subprocess.Popen(  # pylint:disable=consider-using-with
        [_SYMBOLIZER, '-e', library, offset, '-f'],
        stdout=subprocess.PIPE)
    results = command.communicate()
    if command.returncode == 0:
      return results[0].split(os.linesep)
  return None


def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '-f',
      '--filename',
      required=True,
      help='The path to the file that contains the stack traces, crashes, or '
      'raw addresses.')
  arg_parser.add_argument(
      '-l',
      '--library',
      required=True,
      help='The path to the library that is believed to contain the addresses.')
  arg_parser.add_argument(
      'base_address',
      type=str,
      nargs=1,
      help='The base address of the library.')
  args, _ = arg_parser.parse_known_args()

  if not os.path.exists(_SYMBOLIZER):
    raise ValueError(
        f'Please update {__file__} with a valid llvm-symbolizer path.')

  return _Symbolize(args.filename, args.library, args.base_address[0])


if __name__ == '__main__':
  sys.exit(main())
