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
"""Persistent LLVM symbolizer runner with caching and multi-binary support."""

import collections
import os
import subprocess
import sys
from typing import List, Optional, Tuple, Union

_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
if _SRC_DIR not in sys.path:
  sys.path.insert(0, _SRC_DIR)

from starboard.tools import paths  # pylint: disable=wrong-import-position

DEFAULT_SYMBOLIZER = os.environ.get(
    'LLVM_SYMBOLIZER_PATH',
    os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'llvm-build',
                 'Release+Asserts', 'bin', 'llvm-symbolizer'))


class SymbolizerRunner:
  """Manages a persistent llvm-symbolizer process across multiple binaries.

  Uses an interactive pipe protocol to query offsets without respawning the
  symbolizer process per lookup. Stderr is redirected to DEVNULL to avoid OS
  pipe buffer deadlocks when external libraries report errors or missing debug
  data. Lookup results are cached in an LRU cache.
  """

  def __init__(self,
               library: Optional[str] = None,
               default_library: Optional[str] = None,
               symbolizer_path: Optional[str] = None,
               max_cache_size: int = 10000):
    self._library = library or default_library
    self._default_library = self._library
    self._symbolizer_path = symbolizer_path or DEFAULT_SYMBOLIZER
    self._max_cache_size = max_cache_size
    self._proc = None
    self._cache = collections.OrderedDict()

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.close()

  @property
  def default_library(self) -> Optional[str]:
    """Returns the default library path configured for this runner."""
    return self._default_library

  def close(self):
    """Terminates the symbolizer subprocess and closes open pipe streams."""
    if self._proc:
      if self._proc.stdin:
        try:
          self._proc.stdin.close()
        except (OSError, ValueError):
          pass
      if self._proc.stdout:
        try:
          self._proc.stdout.close()
        except (OSError, ValueError):
          pass
      try:
        self._proc.wait()
      except (OSError, ValueError):
        pass
      self._proc = None

  def _ensure_proc(self):
    if self._proc is None:
      cmd = [self._symbolizer_path, '--demangle', '--inlines']
      if self._default_library:
        cmd.append(f'--obj={self._default_library}')
      self._proc = subprocess.Popen(  # pylint: disable=consider-using-with
          cmd,
          stdin=subprocess.PIPE,
          stdout=subprocess.PIPE,
          stderr=subprocess.DEVNULL,
          text=True,
          encoding='utf-8',
          errors='replace')

  def symbolize(
      self,
      offset: Union[int, str],
      binary: Optional[str] = None) -> Optional[List[Tuple[str, str]]]:
    """Resolves an offset to symbol frames.

    Args:
      offset: Numeric offset or hex/decimal string.
      binary: Path to the binary containing the offset. If omitted, uses
        default_library.

    Returns:
      A list of (function_name, file_line) tuples representing inlined frames
      from innermost to outermost, or None if unresolvable.
    """
    try:
      offset_int = int(str(offset), 0)
    except (ValueError, TypeError):
      return None

    if offset_int < 0:
      return None

    target_binary = binary or self._default_library
    cache_key = (target_binary, offset_int)

    if cache_key in self._cache:
      self._cache.move_to_end(cache_key)
      return self._cache[cache_key]

    try:
      self._ensure_proc()
      if binary:
        query = f'"{binary}" {hex(offset_int)}\n'
      elif self._default_library:
        query = f'{hex(offset_int)}\n'
      else:
        return None

      self._proc.stdin.write(query)
      self._proc.stdin.flush()

      raw_lines = []
      while True:
        line = self._proc.stdout.readline()
        if not line:
          self.close()
          break
        if line == '\n':
          break
        raw_lines.append(line.rstrip('\r\n'))

      if not raw_lines:
        return None

      frames = []
      for i in range(0, len(raw_lines), 2):
        func = raw_lines[i]
        file_line = raw_lines[i + 1] if i + 1 < len(raw_lines) else ''
        frames.append((func, file_line))

      # If all frames are unresolved question marks, treat as unresolvable.
      if all(func == '??' for func, _ in frames):
        res = None
      else:
        res = frames

      if len(self._cache) >= self._max_cache_size:
        self._cache.popitem(last=False)
      self._cache[cache_key] = res
      return res

    except (OSError, ValueError):
      self.close()
      return None


# Backward-compatible alias for existing call sites and tests.
_SymbolizerRunner = SymbolizerRunner
