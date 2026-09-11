#!/usr/bin/env python3
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

import dataclasses
import re
from typing import List, Optional, Tuple, Union

_RE_ASAN_MODE1 = re.compile(
    r'^(.*?)(#[0-9]{1,3})\s+(0x[a-fA-F0-9]+)\s+\(<unknown\s+module>\)(.*)')
_RE_ASAN_MODE2 = re.compile(
    r'^(.*?)(#[0-9]{1,3})\s+(0x[a-fA-F0-9]+)\s+\((.*?)\+0x([a-fA-F0-9]+)\)(.*)')
_RE_ANDROID = re.compile(
    r'^(.*?)(#[0-9]{1,3})\s+pc\s+(?:0x)?([a-fA-F0-9]+)\s+(.*)$')
_RE_COBALT = re.compile(
    r'^(.*?(?:\s|\t|^))(<unknown>|[^\s\[\]]+(?:\(.*?\))?)\s+'
    r'\[(0x[0-9a-fA-F]+)\]\s*$')
_RE_RAW = re.compile(r'^(0x[a-fA-F0-9]+)$')
_RE_GDB = re.compile(r'^(.*?)(#[0-9]{1,3})\s+(0x[a-fA-F0-9]+)\s*')


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
      cleaned = cleaned[cleaned.find(prefix) + len(prefix):]
  return cleaned


@dataclasses.dataclass
class FrameMatch:
  """Structured representation of a matched stack frame."""
  original_line: str
  prefix: str
  frame_index_str: Optional[str]
  address: int
  explicit_offset: Optional[int] = None
  binary: Optional[str] = None
  extra: Optional[str] = None


class FormatHandler:
  """Base class for matching and formatting stack trace frames."""

  def match(self, line: str) -> Optional[FrameMatch]:
    """Returns parsed frame metadata if the line matches this format."""
    raise NotImplementedError()

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    """Formats symbolized frames preserving the input style."""
    raise NotImplementedError()


class AsanMode1FormatHandler(FormatHandler):
  """Handles ASan '(<unknown module>)' frames."""

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

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    frame_num = int(
        match.frame_index_str.lstrip('#')) if match.frame_index_str else 0
    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      idx = f'#{frame_num + i}'
      file_line = strip_path_prefix(raw_file_line)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      lines.append(
          f'{match.prefix}{idx} {hex(resolved_offset)} in {func}{file_str}\n')
    return lines


class AsanMode2FormatHandler(FormatHandler):
  """Handles ASan '(binary+0xoffset)' frames."""

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

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    frame_num = int(
        match.frame_index_str.lstrip('#')) if match.frame_index_str else 0
    lines = []
    for i, (func, raw_file_line) in enumerate(pairs):
      idx = f'#{frame_num + i}'
      file_line = strip_path_prefix(raw_file_line)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      lines.append(
          f'{match.prefix}{idx} {hex(match.address)} in {func}{file_str}\n')
    return lines


class AndroidFormatHandler(FormatHandler):
  """Handles Android logcat and tombstone lines ('pc 0x...')."""

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_ANDROID.match(line)
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=m.group(2),
        address=int(m.group(3), 16),
        explicit_offset=int(m.group(3), 16),
        extra=m.group(4))

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    lines = []
    for func, raw_file_line in pairs:
      file_line = strip_path_prefix(raw_file_line)
      file_str = f' {file_line}' if file_line and '?' not in file_line else ''
      extra_str = f' ({match.extra})' if match.extra else ''
      lines.append(
          f'{match.prefix}{match.frame_index_str} pc {hex(resolved_offset)} '
          f'in {func}{file_str}{extra_str}\n')
    return lines


class CobaltFormatHandler(FormatHandler):
  """Handles Cobalt stack dump format ('<unknown> [0x...]')."""

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_COBALT.match(line)
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=None,
        address=int(m.group(3), 0),
        extra=m.group(2))

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]

    prefix = match.prefix or '        '
    return [f'{prefix}{hex(resolved_offset)} [{pairs[0][0]}]\n']


class RawFormatHandler(FormatHandler):
  """Handles standalone raw hexadecimal addresses ('0x...')."""

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_RAW.match(line.strip())
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix='',
        frame_index_str=None,
        address=int(m.group(1), 0))

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs:
      return [match.original_line]
    func, raw_file_line = pairs[0]
    file_line = strip_path_prefix(raw_file_line)
    file_str = f' in {file_line}' if file_line and '?' not in file_line else ''
    return [f'{hex(resolved_offset)} {func}{file_str}\n']


class GdbFormatHandler(FormatHandler):
  """Handles GDB stack trace lines ('#1 0x... in ?? ()')."""

  def match(self, line: str) -> Optional[FrameMatch]:
    m = _RE_GDB.match(line)
    if not m:
      return None
    return FrameMatch(
        original_line=line,
        prefix=m.group(1),
        frame_index_str=m.group(2),
        address=int(m.group(3), 0))

  def format(self, match: FrameMatch, results: Union[List[Tuple[str, str]],
                                                     List[str]],
             resolved_offset: int) -> List[str]:
    pairs = normalize_symbol_results(results)
    if not pairs or '?' in pairs[0][0]:
      return [match.original_line]
    func, raw_file_line = pairs[0]
    file_line = strip_path_prefix(raw_file_line)
    file_str = f' {file_line}' if file_line and '?' not in file_line else ''
    return [
        f'{match.prefix}{match.frame_index_str} {hex(resolved_offset)} in '
        f'{func}{file_str}\n'
    ]


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
