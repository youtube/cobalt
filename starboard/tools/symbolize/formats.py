# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Format handlers and registries for parsing and formatting stack traces."""

import abc
import dataclasses
import os
import re
from typing import List, Optional, Tuple, Union

# Matches AddressSanitizer unknown module frames:
# Example: '    #0 0x7f48e7a45000  (<unknown module>)'
# Example with syslog prefix:
#   '[syslog]    #0 0x7f48e7a45000  (<unknown module>)'
_RE_ASAN_MODE1 = re.compile(
    r'^(.*?)(#[0-9]{1,3})\s+(0x[a-fA-F0-9]+)\s+\(<unknown\s+module>\)(.*)')

# Matches AddressSanitizer named module frames with explicit offset:
# Example: '    #1 0x7f48e7a46000  (libcobalt.so+0x12345)'
_RE_ASAN_MODE2 = re.compile(
    r'^(.*?)(#[0-9]{1,3})\s+(0x[a-fA-F0-9]+)\s+\((.*?)\+0x([a-fA-F0-9]+)\)(.*)')

# Matches Android tombstone and logcat stack lines:
# Example: '    #00 pc 000000000001a450  /system/lib64/libc.so'
# Example: '    #01 pc 0x5a120  /data/app/.../base.apk!libcobalt.so (abort+168)'
_RE_ANDROID = re.compile(
    r'^(.*?)(#[0-9]{1,3})\s+pc\s+(?:0x)?([a-fA-F0-9]+)\s+(.*)$')

# Matches standard Cobalt stack dump frames:
# Example: '        <unknown> [0x7f48e7a45000]'
# Example: '        CobaltCrashFunc(int) [0x7f48e7a45000]'
_RE_COBALT = re.compile(
    r'^(.*?(?:\t|\s{2,}))?(<unknown>|.+?)\s+\[(0x[0-9a-fA-F]+)\]\s*$')

# Matches inverted Cobalt stack dump frames (address before symbol):
# Example: '        0x7f48e7a45000 [<unknown>]'
# Example: '        0x7f48e7a45000 [CobaltCrashFunc]'
_RE_COBALT_INVERTED = re.compile(r'^(.*?(?:\s|\t|^))(0x[0-9a-fA-F]+)\s+'
                                 r'\[(.*?)\]\s*$')

# Matches standalone raw hexadecimal addresses:
# Example: '0x7f48e7a45000'
_RE_RAW = re.compile(r'^(0x[a-fA-F0-9]+)$')

# Matches GDB backtrace frames:
# Example: '#1 0x7f48e7a45000 in ?? ()'
# Example: '    #0  0x00007ffff7a2a000 in main ()'
_RE_GDB = re.compile(r'^(.*?)(#[0-9]{1,3})\s+(0x[a-fA-F0-9]+)\s+in\s+(.*?)$')


def normalize_symbol_results(
    results: Union[List[Tuple[str, str]], List[str]]) -> List[Tuple[str, str]]:
  """Normalizes symbolizer results into (function, file_line) pairs."""
  if not results:
    return []
  if isinstance(results[0], tuple):
    return results  # type: ignore
  # Handle flat list of strings: ['func', 'file:line'] or ['func']
  if len(results) == 1:
    return [(results[0], '')]
  if len(results) == 2 and not isinstance(results[1], tuple):
    return [(results[0], results[1])]
  pairs = []
  for i in range(0, len(results), 2):
    func = results[i]
    file_line = results[i + 1] if i + 1 < len(results) else ''
    pairs.append((func, file_line))
  return pairs


def strip_path_prefix(file_line: str,
                      prefixes: Optional[List[str]] = None) -> str:
  """Strips common build path prefixes from source file references."""
  if not file_line or file_line.startswith('??'):
    return ''
  cleaned = file_line
  all_prefixes = list(prefixes or [])
  all_prefixes.append('Release/../../')
  all_prefixes.append('out/')
  for prefix in all_prefixes:
    if prefix in cleaned:
      cleaned = cleaned.partition(prefix)[2]
  return cleaned


@dataclasses.dataclass
class FrameMatch:
  """Structured representation of a matched stack frame.

  Attributes:
    original_line: The full, unmodified source line as read from the input.
    prefix: Any leading text before the frame content (e.g. syslog tag, logcat
      header, or indentation whitespace).
    frame_index_str: The frame numbering token including hash prefix
      (e.g. '#0', '#01'), or None if the format does not specify frame numbers.
    address: The numerical memory address or file offset parsed from the frame.
    explicit_offset: Pre-computed relative offset if explicitly provided by the
      format (e.g. from 'lib.so+0xoffset' or Android 'pc 0x...'), bypassing
      base address calculation.
    binary: The filename or path of the binary associated with this frame
      (e.g. 'libcobalt.so'), or None if unknown or using default library.
    extra: Ancillary metadata captured from the line (e.g. existing symbol
      name, trailing details, or remainder of the line).
  """
  original_line: str
  prefix: str
  frame_index_str: Optional[str]
  address: int
  explicit_offset: Optional[int] = None
  binary: Optional[str] = None
  extra: Optional[str] = None


class FormatHandler(abc.ABC):
  """Base class for matching and formatting stack trace frames."""

  @abc.abstractmethod
  def match(self, line: str) -> Optional[FrameMatch]:
    """Returns parsed frame metadata if the line matches this format."""

  @abc.abstractmethod
  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    """Formats symbolized frames preserving the input style."""


class AsanMode1FormatHandler(FormatHandler):
  """Handles ASan '(<unknown module>)' frames.

  Supported input format:
      <prefix>#<frame> <address> (<unknown module>)<extra>
  Example input:
      '    #0 0x7f48e7a45000  (<unknown module>)'
  Example output:
      '    #0 0x12345 in CobaltCrashFunc() cobalt/browser/main.cc:123\n'
  """

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_ASAN_MODE1.match(line)
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=m.group(2),
        address=int(m.group(3), 0),
        extra=m.group(4))

  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    frame_num = int(
        match.frame_index_str.lstrip('#')) if match.frame_index_str else 0
    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      idx = f'#{frame_num + i}'
      file_line = strip_path_prefix(raw_file_line, prefixes=strip_prefixes)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      lines.append(
          f'{match.prefix}{idx} {hex(resolved_offset)} in {func}{file_str}\n')
    return lines


class AsanMode2FormatHandler(FormatHandler):
  """Handles ASan '(binary+0xoffset)' frames with explicit offsets.

  Supported input format:
      <prefix>#<frame> <address> (<binary>+0x<offset>)<extra>
  Example input:
      '    #1 0x7f48e7a46000  (libcobalt.so+0x12345)'
  Example output:
      '    #1 0x7f48e7a46000 in CobaltParentFunc() cobalt/browser/main.cc:456\n'
  """

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_ASAN_MODE2.match(line)
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=m.group(2),
        address=int(m.group(3), 0),
        explicit_offset=int(m.group(5), 16),
        binary=m.group(4),
        extra=m.group(6))

  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    frame_num = int(
        match.frame_index_str.lstrip('#')) if match.frame_index_str else 0
    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      idx = f'#{frame_num + i}'
      file_line = strip_path_prefix(raw_file_line, prefixes=strip_prefixes)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      lines.append(
          f'{match.prefix}{idx} {hex(match.address)} in {func}{file_str}\n')
    return lines


class AndroidFormatHandler(FormatHandler):
  """Handles Android logcat and tombstone lines ('pc 0x...').

  Supported input format:
      <prefix>#<frame> pc <offset> <binary_and_details>
  Example inputs:
      '    #00 pc 000000000001a450  /system/lib64/libc.so'
      '    #01 pc 0x5a120  /data/app/.../base.apk!libcobalt.so (abort+168)'
  Example output:
      '    #00 pc 0x1a450 in abort bionic/libc/bionic/abort.cpp:49\n'
  """

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_ANDROID.match(line)
    if not m:
      return None
    extra = m.group(4)
    binary = None
    if extra:
      first_token = extra.split()[0]
      if '/' in first_token or first_token.endswith('.so'):
        binary = os.path.basename(first_token.split('!')[-1])
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=m.group(2),
        address=int(m.group(3), 16),
        explicit_offset=int(m.group(3), 16),
        binary=binary,
        extra=extra)

  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      file_line = strip_path_prefix(raw_file_line, prefixes=strip_prefixes)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      extra_str = f' ({match.extra})' if match.extra else ''
      inlined_str = ' (inlined)' if i > 0 else ''
      lines.append(
          f'{match.prefix}{match.frame_index_str} pc {hex(resolved_offset)} '
          f'in {func}{file_str}{extra_str}{inlined_str}\n')
    return lines


class CobaltFormatHandler(FormatHandler):
  """Handles Cobalt stack dumps in standard and inverted formats.

  Supported input formats:
      Standard: <prefix><symbol_or_unknown> [<address>]
      Inverted: <prefix><address> [<symbol_or_unknown>]
  Example inputs:
      '        <unknown> [0x7f48e7a45000]'
      '        CobaltCrashFunc(int) [0x7f48e7a45000]'
      '        0x7f48e7a45000 [<unknown>]'
      '        0x7f48e7a45000 [CobaltCrashFunc]'
  Example output:
      '        0x12345 [CobaltCrashFunc(int)]\n'
  """

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_COBALT.match(line)
    if m:
      return FrameMatch(
          original_line=line,
          prefix=m.group(1) or '',
          frame_index_str=None,
          address=int(m.group(3), 0),
          extra=m.group(2))
    m = _RE_COBALT_INVERTED.match(line)
    if m:
      return FrameMatch(
          original_line=line,
          prefix=m.group(1) or '',
          frame_index_str=None,
          address=int(m.group(2), 0),
          extra=m.group(3))
    return None

  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    prefix = match.prefix if match.prefix is not None else '        '
    lines = []
    for i, (func, _) in enumerate(pairs):
      inlined_str = ' (inlined)' if i > 0 else ''
      lines.append(f'{prefix}{hex(resolved_offset)} [{func}]{inlined_str}\n')
    return lines


class RawFormatHandler(FormatHandler):
  """Handles standalone raw hexadecimal addresses.

  Supported input format:
      0x<address>
  Example input:
      '0x7f48e7a45000'
  Example output:
      '0x12345 CobaltCrashFunc() in cobalt/browser/main.cc:123\n'
  """

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_RAW.match(line.strip())
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix='',
        frame_index_str=None,
        address=int(m.group(1), 0))

  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs:
      return [match.original_line]
    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      file_line = strip_path_prefix(raw_file_line, prefixes=strip_prefixes)
      file_str = (f' in {file_line}'
                  if file_line and '?' not in file_line else '')
      inlined_str = ' (inlined)' if i > 0 else ''
      lines.append(f'{hex(resolved_offset)} {func}{file_str}{inlined_str}\n')
    return lines


class GdbFormatHandler(FormatHandler):
  """Handles GDB backtrace frames.

  Supported input format:
      <prefix>#<frame> <address> in <symbol_or_unknown>
  Example inputs:
      '#1 0x7f48e7a45000 in ?? ()'
      '    #0  0x00007ffff7a2a000 in main ()'
  Example output:
      '#1 0x12345 in CobaltCrashFunc() cobalt/browser/main.cc:123\n'
  """

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_GDB.match(line)
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=m.group(2),
        address=int(m.group(3), 0),
        extra=m.group(4))

  def format(self,
             match: FrameMatch,
             results: Union[List[Tuple[str, str]], List[str]],
             resolved_offset: int,
             strip_prefixes: Optional[List[str]] = None) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]
    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      file_line = strip_path_prefix(raw_file_line, prefixes=strip_prefixes)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      inlined_str = ' (inlined)' if i > 0 else ''
      lines.append(
          f'{match.prefix}{match.frame_index_str} {hex(resolved_offset)} in '
          f'{func}{file_str}{inlined_str}\n')
    return lines


class FormatRegistry:
  """Registry of format handlers tried in order."""

  def __init__(self, handlers: Optional[List[FormatHandler]] = None):
    self._handlers = handlers or [
        AsanMode1FormatHandler(),
        AsanMode2FormatHandler(),
        AndroidFormatHandler(),
        CobaltFormatHandler(),
        RawFormatHandler(),
        GdbFormatHandler(),
    ]

  def match(self, line: str) -> Optional[Tuple[FormatHandler, FrameMatch]]:
    """Finds the first format handler that matches the line."""
    for handler in self._handlers:
      m = handler.match(line)
      if m is not None:
        return handler, m
    return None
