#!/usr/bin/env vpython3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""A parser for gtest markers."""

import re
from typing import Dict, List, TypedDict

class GtestMarker(TypedDict):
  type: str
  name: str | None
  line: int

import patterns


class GtestParser:
  """Parses gtest markers and determines test boundaries."""

  def __init__(self, lines: List[str]):
    self.lines = lines
    self.gtest_markers: List[GtestMarker] = []
    self.test_boundaries: Dict[str, tuple[int, int]] = {}
    self.finished_tests: set[str] = set()

  def parse(self) -> None:
    """Parses the log lines for gtest markers and boundaries."""
    self._find_gtest_markers()
    self._determine_test_boundaries()

  def _find_gtest_markers(self) -> None:
    """Finds all gtest markers in the log lines."""
    summary_start_line = -1
    summary_pattern = re.compile(r'\[  (?:FAILED|SKIPPED)  \].*tests?, listed below:')
    for i, line in enumerate(self.lines):
        if summary_pattern.search(line):
            summary_start_line = i
            break

    summary_lines_to_ignore = set()
    if summary_start_line != -1:
        for i in range(summary_start_line + 1, len(self.lines)):
            line = self.lines[i].strip()
            if line.startswith('[  FAILED  ]') or line.startswith('[  SKIPPED ]'):
                summary_lines_to_ignore.add(i)
            else:
                # First line that is not a summary item ends the block
                break

    for i, line in enumerate(self.lines):
      if i in summary_lines_to_ignore:
          continue
      run_match = patterns.GTEST_RUN_PATTERN.search(line)
      ok_match = patterns.GTEST_OK_PATTERN.search(line)
      failed_match = patterns.GTEST_FAILED_PATTERN.search(line)
      skipped_match = patterns.GTEST_SKIPPED_PATTERN.search(line)
      log_match = patterns.GTEST_LOG_PATTERN.search(line)

      if run_match:
        self.gtest_markers.append({'type': 'run', 'name': run_match.group(1).strip(), 'line': i})
      elif ok_match:
        test_name = re.sub(r'\x1B\[[0-9;]*[a-zA-Z]', '', ok_match.group(1).strip()).split(' ')[0]
        self.gtest_markers.append({'type': 'ok', 'name': test_name, 'line': i})
      elif failed_match:
        test_name = re.sub(r'\x1B\[[0-9;]*[a-zA-Z]', '', failed_match.group(1).strip()).split(' ')[0]
        self.gtest_markers.append({'type': 'failed', 'name': test_name, 'line': i})
      elif skipped_match:
        test_name = re.sub(r'\x1B\[[0-9;]*[a-zA-Z]', '', skipped_match.group(1).strip()).split(' ')[0]
        self.gtest_markers.append({'type': 'skipped', 'name': test_name, 'line': i})
      elif log_match:
        self.gtest_markers.append({'type': 'log', 'line': i})

  def _determine_test_boundaries(self) -> None:
    """Determines the test boundaries from the gtest markers."""
    run_markers = [m for m in self.gtest_markers if m['type'] == 'run']
    self.finished_tests = set()

    for i, run_marker in enumerate(run_markers):
      test_name = run_marker['name']
      start_line = run_marker['line']
      end_line = len(self.lines)

      # Find the *first* corresponding 'ok', 'failed', or 'skipped' marker
      for marker in self.gtest_markers:
        if marker['line'] > start_line and marker.get('name') == test_name and marker['type'] in ['ok', 'failed', 'skipped']:
          end_line = marker['line'] + 1
          self.finished_tests.add(test_name)
          break

      # If no clean end, use the start of the next test as the boundary
      if test_name not in self.finished_tests and (i + 1) < len(run_markers):
        end_line = run_markers[i+1]['line']

      self.test_boundaries[test_name] = (start_line, end_line)