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
"""Dynamic session tracking and address resolution for streaming."""

import logging
import re
from typing import Optional, Tuple

_RE_LOAD_START = re.compile(r'Load start=(0x[0-9a-fA-F]+)')

# Threshold for 64-bit ASLR addresses (4 GB).
_ASLR_64BIT_THRESHOLD = 0x100000000

# Typical maximum relative virtual memory size for shared libraries (100 MB).
_MAX_RELATIVE_VSIZE = 100 * 1024 * 1024


class StreamingSessionTracker:
  """Tracks dynamically loaded binary base addresses across streaming log lines.

  Maintains per-session base address state when encountering 'Load start=0x...'
  lines in logs, resolving address modes (architectural range check, multi-frame
  probing, and missing base fallback) in O(1) memory.
  """

  def __init__(self, default_base_address: Optional[int] = None):
    self.current_base_address = default_base_address
    self.session_id = 0
    self.latched_mode = None  # Optional['absolute' | 'relative']

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

    if self.current_base_address is not None and self.current_base_address > 0:
      if address >= self.current_base_address:
        return address - self.current_base_address, 'base_subtracted'
      return address, 'base_relative'

    # When base is 0 or None:
    if address > _ASLR_64BIT_THRESHOLD:
      if self.current_base_address == 0:
        return address, 'explicit_zero_base'
      # Tier 3: Missing base fallback
      logging.warning(
          '64-bit ASLR address 0x%x encountered but no Load start= was logged; '
          'preserving original frame.', address)
      return None, 'missing_base'

    if address < _MAX_RELATIVE_VSIZE:
      return address, 'tier1_relative'

    # Ambiguous 32-bit range (100 MB <= address <= 4 GB)
    if self.latched_mode == 'relative':
      return address, 'latched_relative'
    if (self.latched_mode == 'absolute' and
        self.current_base_address is not None):
      return address - self.current_base_address, 'latched_absolute'

    # Tier 2: Multi-Frame Probing
    if runner and self.current_base_address is not None:
      try:
        res_rel = runner.symbolize(address, binary)
        res_abs = runner.symbolize(address - self.current_base_address, binary)
      except TypeError:
        res_rel = runner.symbolize(address)
        res_abs = runner.symbolize(address - self.current_base_address)

      if res_abs and not res_rel:
        self.latched_mode = 'absolute'
        return address - self.current_base_address, 'tier2_probe_absolute'
      if res_rel and not res_abs:
        self.latched_mode = 'relative'
        return address, 'tier2_probe_relative'

      entry_keywords = ('main', 'SbEventHandle', 'MessageLoop', 'Run', 'start')
      abs_has_entry = res_abs and any(
          any(k in frame[0] for k in entry_keywords) for frame in res_abs)
      rel_has_entry = res_rel and any(
          any(k in frame[0] for k in entry_keywords) for frame in res_rel)

      if abs_has_entry and not rel_has_entry:
        self.latched_mode = 'absolute'
        return address - self.current_base_address, 'tier2_entry_absolute'
      if rel_has_entry and not abs_has_entry:
        self.latched_mode = 'relative'
        return address, 'tier2_entry_relative'

    if (self.current_base_address is not None and
        address >= self.current_base_address):
      return address - self.current_base_address, 'fallback_absolute'
    return address, 'fallback_relative'
