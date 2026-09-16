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
"""Dynamic session tracking and address resolution for streaming."""

import enum
import logging
import re
from typing import Optional, Tuple

_RE_LOAD_START = re.compile(r'Load start=(0x[0-9a-fA-F]+)')

# Threshold for 64-bit ASLR addresses (4 GB).
_ASLR_64BIT_THRESHOLD = 0x100000000

# Typical maximum relative virtual memory size for shared libraries (100 MB).
_MAX_RELATIVE_VSIZE = 100 * 1024 * 1024


class AddressMode(str, enum.Enum):
  """Latched resolution mode for ambiguous address spaces.

  - ABSOLUTE: The address parsed from the stack frame is an absolute virtual
    memory address in the process address space (e.g. 0x7f48e7a45000). To
    resolve symbols with llvm-symbolizer, the library's base load address (from
    'Load start=0x...') must be subtracted: offset = address - base_address.
  - RELATIVE: The address parsed from the stack frame is already relative to
    the binary's load segment or ELF image base (e.g. +0x12345 or 0x0001a450),
    or the library is loaded at base 0. No subtraction is performed:
    offset = address.
  """
  ABSOLUTE = 'absolute'
  RELATIVE = 'relative'


class StreamingSessionTracker:
  """Tracks dynamically loaded binary base addresses across streaming log lines.

  Maintains per-session base address state when encountering 'Load start=0x...'
  lines in logs, resolving address modes (architectural range check, multi-frame
  probing, and missing base fallback) in O(1) memory.
  """

  def __init__(self, default_base_address: Optional[int] = None):
    self.current_base_address = default_base_address
    self.session_id = 0
    self.latched_mode: Optional[AddressMode] = None

  def check_line(self, line: str) -> Optional[int]:
    """Inspects a line for 'Load start=' markers and updates active session."""
    m = _RE_LOAD_START.search(line)
    if m:
      self.current_base_address = int(m.group(1), 16)
      self.session_id += 1
      self.latched_mode = None
      return self.current_base_address
    return None

  def resolve_offset(self,
                     address: int,
                     explicit_offset: Optional[int] = None,
                     runner=None,
                     binary: Optional[str] = None) -> Tuple[Optional[int], str]:
    """Resolves an address into a library offset using three-tier resolution.

    Resolution Tiers:
      Tier 1 (Architectural Range Check):
        - Addresses > 4 GB (_ASLR_64BIT_THRESHOLD) are unambiguously 64-bit
          ASLR absolute addresses requiring base subtraction.
        - Addresses < 100 MB (_MAX_RELATIVE_VSIZE) are within typical shared
          library code size and are treated directly as relative offsets.
      Tier 2 (Multi-Frame Probing):
        - Addresses in the ambiguous 32-bit window (100 MB <= addr <= 4 GB)
          could either be absolute addresses or relative offsets in large
          binaries. Probing queries the symbolizer with both candidates to
          determine which produces valid symbols, latching the mode for
          subsequent frames.
      Tier 3 (Missing Base Fallback):
        - If a 64-bit ASLR address is encountered without a known base address,
          the original frame is preserved to prevent corrupting stack traces.

    Args:
      address: Raw memory address from stack frame.
      explicit_offset: Pre-extracted offset if format already provides one.
      runner: Optional symbolizer runner for probing in ambiguous cases.
      binary: Optional binary path.

    Returns:
      A tuple of (resolved_offset, resolution_tier_name).
    """
    if explicit_offset is not None:
      return explicit_offset, 'explicit'

    if address > _ASLR_64BIT_THRESHOLD:
      if self.current_base_address == 0:
        return address, 'explicit_zero_base'
      if (self.current_base_address is not None and
          self.current_base_address > 0):
        if address >= self.current_base_address:
          return address - self.current_base_address, 'base_subtracted'
        return address, 'base_relative'
      # Tier 3: Missing base fallback
      logging.warning(
          '64-bit ASLR address 0x%x encountered but no Load start= was logged; '
          'preserving original frame.', address)
      return None, 'missing_base'

    if self.current_base_address == 0:
      return address, 'explicit_zero_base'

    if self.current_base_address is None:
      if address < _MAX_RELATIVE_VSIZE:
        return address, 'tier1_relative'
      return address, 'no_base_relative'

    # When base is known and > 0:
    if address < self.current_base_address:
      return address, 'base_relative'

    # Ambiguous 32-bit range (address >= current_base_address):
    # Check if a previous frame in this crash session already determined
    # the mode.
    if self.latched_mode == AddressMode.RELATIVE:
      return address, 'latched_relative'
    if (self.latched_mode == AddressMode.ABSOLUTE and
        self.current_base_address is not None):
      return address - self.current_base_address, 'latched_absolute'

    # Tier 2: Multi-Frame Probing
    # Why probing is done: In 32-bit systems, an address such as 0x40123456
    # could either be an absolute virtual memory address (with base 0x40000000,
    # meaning offset is 0x123456) or an offset in a large binary. Speculatively
    # querying both candidates against the symbolizer reveals which offset
    # actually resolves to valid function names and source locations.
    if runner and self.current_base_address is not None:
      try:
        res_rel = runner.symbolize(address, binary)
        res_abs = runner.symbolize(address - self.current_base_address, binary)
      except TypeError:
        res_rel = runner.symbolize(address)
        res_abs = runner.symbolize(address - self.current_base_address)

      if res_abs and not res_rel:
        self.latched_mode = AddressMode.ABSOLUTE
        return address - self.current_base_address, 'tier2_probe_absolute'
      if res_rel and not res_abs:
        self.latched_mode = AddressMode.RELATIVE
        return address, 'tier2_probe_relative'

      # Purpose of entry_keywords: If both relative and absolute offsets happen
      # to match symbols (e.g. small offsets hitting low-address symbols),
      # entry_keywords acts as a heuristic tie-breaker. Crash stack traces
      # almost universally terminate at well-known entry points such as main(),
      # Starboard's SbEventHandle(), or message loop runners. A candidate
      # matching an entry keyword is overwhelmingly likely to be genuine.
      entry_keywords = ('main', 'SbEventHandle', 'MessageLoop', 'Run', 'start')

      def _has_entry_keyword(frames):
        return bool(frames) and any(
            k in frame[0] for frame in frames for k in entry_keywords)

      abs_has_entry = _has_entry_keyword(res_abs)
      rel_has_entry = _has_entry_keyword(res_rel)

      if abs_has_entry and not rel_has_entry:
        self.latched_mode = AddressMode.ABSOLUTE
        return address - self.current_base_address, 'tier2_entry_absolute'
      if rel_has_entry and not abs_has_entry:
        self.latched_mode = AddressMode.RELATIVE
        return address, 'tier2_entry_relative'

    if (self.current_base_address is not None and
        address >= self.current_base_address):
      return address - self.current_base_address, 'fallback_absolute'
    return address, 'fallback_relative'
