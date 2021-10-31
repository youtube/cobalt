#!/usr/bin/env python
# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Port files to Starboard (or do a lot of the leg work)."""

import logging
import os
import re
import string
import sys

# Prefix for system functions.
_SYSTEM_FUNCTIONS_PREFIX = 'XML_'

# Prefix and suffix for system header.
_SYSTEM_HEADER_PREFIX = '#ifdef'
_SYSTEM_HEADER_PREFIX_TEMPLATE = string.Template('#ifdef HAVE_${header_name}_H')
_SYSTEM_HEADER_SUFFIX = '#endif'

# Includes that should be wrapped by .
_INCLUDES_TO_WRAP = [
]

# References to these symbols should be wrapped with the previx so that we
# can redirect them for Starboard.
_SYSTEM_FUNCTIONS = [
    r'abort',
    r'assert',
    r'free',
    r'malloc',
    r'realloc',
]

# Matches all "system" include lines. Note that this does NOT match include
# lines where there is more than one space between the # and the 'include', in
# an attempt to avoid touching includes that are already conditional.
_RE_INCLUDE = re.compile(r'^#\s?include <.*>$')

_RE_INCLUDE_TO_WRAP = re.compile(r'^#\s*include <(' +
                                 r'|'.join(_INCLUDES_TO_WRAP) +
                                 r').*>')

# Matches system function calls that should be wrapped.
_RE_SYSTEM_FUNCTION = re.compile(r'(?<!\w)(?!\>)(' +
                                 r'|'.join(_SYSTEM_FUNCTIONS) +
                                 r')(?=[(])')


def _IsSystemInclude(line):
  return _RE_INCLUDE.match(line) and not _RE_INCLUDE_EXCEPTION.match(line)


def _ReplaceSystemFunction(match):
  return _SYSTEM_FUNCTIONS_PREFIX + match.group(1).upper()


def _ReplaceIncludeToWrap(match):
  return _SYSTEM_FUNCTIONS_PREFIX + match.group(1).upper()


def _Replace(line):
  replaced_line = _RE_SYSTEM_FUNCTION.sub(_ReplaceSystemFunction, line)
  return replaced_line


def PortFile(input_file_path):
  """Reads a source file, ports it, and writes it again.

  Args:
    input_file_path: The path to the source file to port.
  Returns:
    0 on success.
  """
  lines = None
  with open(input_file_path, 'r') as f:
    lines = f.read().splitlines()
  output_lines = []

  logging.info('Porting: %s', input_file_path)

  for line_number in range(0, len(lines)):
    previous_line = lines[line_number - 1] if line_number > 0 else ''
    line = lines[line_number]

    result = _RE_INCLUDE_TO_WRAP.match(line)
    if result:
      if not previous_line.startswith(_SYSTEM_HEADER_PREFIX):
        output_lines.append(_SYSTEM_HEADER_PREFIX_TEMPLATE.substitute(header_name=result.group(1).upper()))
        output_lines.append(line)
        output_lines.append(_SYSTEM_HEADER_SUFFIX)
      else:
        output_lines.append(line)
      continue

    replaced_line = _Replace(line)
    if replaced_line != line:
      logging.debug('line %6d: %s', line_number, replaced_line)
    output_lines.append(replaced_line)

  os.unlink(input_file_path)
  with open(input_file_path, 'w') as f:
    f.write('\n'.join(output_lines) + '\n')

  return 0


def PortAll(file_list):
  result = 0
  for file_path in file_list:
    this_result = PortFile(file_path)
    if this_result != 0:
      result = this_result
  print '\n'
  return result


if __name__ == '__main__':
  logging.basicConfig(format='%(levelname)-7s:%(message)s', level=logging.INFO)
  sys.exit(PortAll(sys.argv[1:]))
