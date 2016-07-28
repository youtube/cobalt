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

"""Port OpenSSL to Starboard (or do a lot of the leg work)."""

import logging
import os
import re
import sys

# Template to use to replace moved system includes inside a Starboard check.
_TEMPLATE = """#include <openssl/opensslconf.h>
#if !defined(OPENSSL_SYS_STARBOARD)
{includes}
#endif  // !defined(OPENSSL_SYS_STARBOARD)
"""

# Included files that match any of these patterns should be left alone.
_INCLUDE_EXCEPTIONS = [
    r'alloca',  # This is defined for SNC compilers
    r'cryptlib',
    r'descrip',
    r'dl[.]',
    r'e_os',
    r'err[.]',
    r'io[.]',
    r'lib[$]routines',
    r'libdtdef',
    r'malloc',  # Only used inside a _WIN32 block
    r'nwfileio',
    r'openssl',
    r'rld_interface',
    r'sys/stat[.]',  # Only included inside NO_POSIX_IO checks
    r'sys/types[.]',  # Only included inside NO_SYS_TYPES_H checks
    r'windows',
]

# References to these symbols should be wrapped with OPENSSL_port_ so that we
# can redirect them for Starboard.
_SYSTEM_FUNCTIONS = [
    r'abort',
    r'assert',
    r'atoi',
    r'free',
    r'getenv',
    r'isalnum',
    r'isdigit',
    r'isspace',
    r'isupper',
    r'isxdigit',
    r'malloc',
    r'memchr',
    r'memcmp',
    r'memcpy',
    r'memmove',
    r'memset',
    r'printf',
    r'qsort',
    r'realloc',
    r'sscanf',
    r'strcasecmp',
    r'strcat',
    r'strchr',
    r'strcmp',
    r'strcpy',
    r'strdup',
    r'strerror',
    r'strlen',
    r'strncasecmp',
    r'strncmp',
    r'strncpy',
    r'strrchr',
    r'strtoul',
    r'time',
    r'tolower',
    r'toupper',
]

# Matches all "system" include lines. Note that this does NOT match include
# lines where there is more than one space between the # and the 'include', in
# an attempt to avoid touching includes that are already conditional.
#
# In one case, rand_unix.c, extra spaces were manually introduced to exclude
# a large set of includes that were already not being processed.
_RE_INCLUDE = re.compile(r'^#\s?include <.*>$')

# Matches include lines that look like system includes, but should not be
# relocated.
_RE_INCLUDE_EXCEPTION = re.compile(r'^#\s*include <(?:' +
                                   r'|'.join(_INCLUDE_EXCEPTIONS) +
                                   r').*>$')

# Matches system function calls that should be wrapped.
_RE_SYSTEM_FUNCTION = re.compile(r'(?<!\w)(' +
                                 r'|'.join(_SYSTEM_FUNCTIONS) +
                                 r')(?=[(])')

# Matches calls to fprintf(stderr, ...)
_RE_FPRINTF = re.compile(r'(?<!\w)fprintf\s*\(\s*stderr\s*,\s*')

# Matches time_t so it can be wrapped.
_RE_TIME = re.compile(r'(?<!\w)(time_t)(?!\w)')

# Matches errno so it can be wrapped.
_RE_ERRNO = re.compile(r'(?<![\w<])(errno)(?![\w.])')

# Matches time.h structs tm and timeval, so they can be wrapped.
_RE_TIME_STRUCTS = re.compile(r'(?<!\w)struct\s+(timeval|tm)(?!\w)')


def _IsSystemInclude(line):
  return _RE_INCLUDE.match(line) and not _RE_INCLUDE_EXCEPTION.match(line)


def _Replace(line):
  replaced_line = _RE_SYSTEM_FUNCTION.sub(r'OPENSSL_port_\1', line)
  replaced_line = _RE_FPRINTF.sub(r'OPENSSL_port_printferr(', replaced_line)
  replaced_line = _RE_TIME.sub(r'OPENSSL_port_\1', replaced_line)
  replaced_line = _RE_ERRNO.sub(r'OPENSSL_port_\1', replaced_line)
  replaced_line = _RE_TIME_STRUCTS.sub(r'OPENSSL_port_\1', replaced_line)
  return replaced_line


def _NeedsReplacement(line):
  return _Replace(line) != line


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

  # Find all system include lines, keeping track of their line numbers.
  includes_to_move = []
  line_numbers_to_delete = []
  needs_update = False
  for line_number in range(0, len(lines)):
    line = lines[line_number]
    if _IsSystemInclude(line):
      needs_update = True
      line_numbers_to_delete.append(line_number)
      includes_to_move.append(line)
    if _NeedsReplacement(line):
      needs_update = True

  # Nothing to do for this file.
  if not needs_update:
    return 0

  logging.info('Porting: %s', input_file_path)

  if line_numbers_to_delete:
    # Delete the include lines.
    for line_number in reversed(line_numbers_to_delete):
      del lines[line_number]

    # Insert them back inside the template.
    text_to_add = _TEMPLATE.format(includes='\n'.join(sorted(includes_to_move)))
    lines_to_add = text_to_add.splitlines()
    insertion_point = line_numbers_to_delete[0]
    for l in range(0, len(lines_to_add)):
      logging.debug('line %6d: %s', l + insertion_point, lines_to_add[l])
    lines[insertion_point:insertion_point] = lines_to_add

  for line_number in range(0, len(lines)):
    line = lines[line_number]
    replaced_line = _Replace(line)
    if replaced_line != line:
      logging.debug('line %6d: %s', line_number, replaced_line)
      lines[line_number] = replaced_line

  os.unlink(input_file_path)
  with open(input_file_path, 'w') as f:
    f.write('\n'.join(lines) + '\n')

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
