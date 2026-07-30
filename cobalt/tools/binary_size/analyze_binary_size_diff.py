#!/usr/bin/env vpython3
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
"""Supersize Diff Analysis Script.

Intended to be executed within the Supersize console environment.
Usage:
  python3 tools/binary_size/supersize console rdk_26_27.sizediff \
    --query="exec(open( \
    \"cobalt/tools/binary_size/analyze_binary_size_diff.py\").read())"
"""
# pylint: disable=undefined-variable

_MIN_BLOAT_THRESHOLD_BYTES = 5120


def analyze_size_diff():
  # 1. Generate the full DeltaSizeInfo object by diffing before/after snapshots
  delta = Diff()

  # 2. Print Total Size Increase Breakdown
  print("=" * 60)
  print("                   TOTAL DIFF METRICS                   ")
  print("=" * 60)
  net_pss = delta.symbols.pss
  added_pss = delta.symbols.WhereDiffStatusIs(models.DIFF_STATUS_ADDED).pss
  removed_pss = delta.symbols.WhereDiffStatusIs(models.DIFF_STATUS_REMOVED).pss
  changed_pss = delta.symbols.WhereDiffStatusIs(models.DIFF_STATUS_CHANGED).pss

  print(f"Net Size Change : {net_pss:+14,.0f} bytes "
        f"({net_pss / 1024.0 / 1024.0:+.2f} MB)")
  print(f"Added Symbols   : {added_pss:+14,.0f} bytes")
  print(f"Removed Symbols : {removed_pss:+14,.0f} bytes")
  print(f"Changed Symbols : {changed_pss:+14,.0f} bytes")

  # 3. Summarize Net Increase by Major Subsystems
  print("\n" + "=" * 60)
  print("              GROWTH BY CHROMIUM COMPONENT              ")
  print("=" * 60)
  # Pass use_pager=False to prevent hanging on interactive terminal prompts
  components = canned_queries.CategorizeByChromeComponent(delta)
  positive_components = [g for g in components if g.pss > 0]
  positive_components.sort(key=lambda g: g.pss, reverse=True)
  Print(models.SymbolGroup(positive_components), use_pager=False)

  # 4. Feature Assessment: Newly Added Components
  print("\n" + "=" * 60)
  print("             NEWLY ADDED COMPONENTS          ")
  print("=" * 60)
  # Isolate symbols that are entirely new in the modified build
  added_symbols = delta.symbols.WhereDiffStatusIs(models.DIFF_STATUS_ADDED)
  # Group newly added symbols by their documented functional component
  # ownership
  new_components = added_symbols.GroupedByComponent().Filter(
      lambda c: c.pss >= _MIN_BLOAT_THRESHOLD_BYTES).Sorted()[-10:]
  if new_components:
    for c in new_components:
      comp_name = c.name if c.name else "<Unassigned Component>"
      print(f"{c.pss:+14,.0f} bytes : {comp_name}")
  else:
    print("  No newly added component exceeds the "
          "meaningful bloat threshold (5 KB).")

  # 5. Feature Assessment: Isolate Third-Party Dependency Growth
  print("\n" + "=" * 60)
  print("         THIRD-PARTY DEPENDENCY GROWTH (PRUNING TARGETS)")
  print("=" * 60)
  tp_growth = delta.symbols.Filter(
      lambda s: "third_party/" in s.source_path).GroupedByPath(depth=2).Filter(
          lambda s: s.pss >= _MIN_BLOAT_THRESHOLD_BYTES).Sorted()[-10:]
  if tp_growth:
    for tp in tp_growth:
      print(f"{tp.pss:+14,.0f} bytes : //{tp.name}")
  else:
    print("  No third-party dependency exceeds the "
          "meaningful bloat threshold (5 KB).")


analyze_size_diff()
