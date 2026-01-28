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
"""A parser for detecting issues in log lines."""

import re
from typing import Dict, List

from base_analyzer import BaseAnalyzer
from lib.gtest_parser import GtestParser, GtestMarker

import models
import patterns


class IssueDetector:
  """Detects issues in log lines."""

  def __init__(self, analyzer: BaseAnalyzer, test_boundaries: Dict[str, tuple[int, int]], gtest_markers: List[GtestMarker]):
    self.analyzer = analyzer
    self.lines = analyzer.lines
    self.test_boundaries = test_boundaries
    self.gtest_markers = gtest_markers
    self.issues: List[models.LogEvent] = []

  def detect(self) -> None:
    """Detects issues in the log lines."""
    self._detect_issues()
    self._detect_gtest_failures()
    self._detect_incomplete_tests()

  def _detect_issues(self) -> None:
    """Detects issues in the log lines."""
    ignored_lines = set()
    last_running_test = None

    for i, line in enumerate(self.lines):
      if i in ignored_lines:
          continue
      test_name = None
      for name, (start, end) in self.test_boundaries.items():
          if start <= i < end:
              test_name = name
              break
      if test_name:
          last_running_test = test_name
      for issue_type, pattern in patterns.ISSUE_PATTERNS.items():
          match = pattern.search(line)
          if match:
              original_line = line.strip()
              # Strip metadata like '[...]' and parenthesized error codes like '(-112)'
              key_base = re.sub(r'^\[.*?\]\s*', '', original_line)
              grouping_key = self.analyzer._get_line_signature(key_base)

              data = {
                  'test_name': test_name or last_running_test,
                  'line_content': original_line,
                  'grouping_key': grouping_key
              }
              if issue_type == 'crash':
                  # Groups for signal: 1=((2=prefix)(3=SIG...)), for FATAL: all None
                  if match.group(3):
                      data['signal'] = match.group(3)
                  else:
                      data['signal'] = 'FATAL'
                  # Check for stack trace on the next few lines
                  has_stack_trace = False
                  # Look ahead up to 10 lines
                  for j in range(i + 1, min(i + 11, len(self.lines))):
                      if self.analyzer._is_stack_trace_line(self.lines[j]):
                          has_stack_trace = True
                          break
                  if has_stack_trace:
                      # Make the grouping key unique to prevent consolidation
                      data['grouping_key'] = f"crash_with_stack_{i}"
                      # A crash with a stack trace means the test didn't finish.
                      # Extend the boundary to the end of the file.
                      if test_name in self.test_boundaries:
                          self.test_boundaries[test_name] = (self.test_boundaries[test_name][0], len(self.lines))
                      # If this was a FATAL crash, check if a signal follows,
                      # and if so, don't create a separate issue for it.
                      if data['signal'] == 'FATAL':
                          for j in range(i + 1, min(i + 21, len(self.lines))):
                              if 'Caught signal:' in self.lines[j] or 'received signal' in self.lines[j]:
                                  ignored_lines.add(j)
                                  break

              self.issues.append(models.LogEvent(
                  line_num=i,
                  issue_type=issue_type,
                  data=data
              ))
              break # Process first match only

  def _detect_gtest_failures(self) -> None:
    """Detects gtest failures."""
    for marker in self.gtest_markers:
        if marker['type'] == 'failed':
            test_name = marker['name']
            start_line = self.test_boundaries[test_name][0]
            self.issues.append(models.LogEvent(line_num=marker['line'], issue_type='gtest_fail', data={'test_name': test_name, 'start_line': start_line}))

  def _detect_incomplete_tests(self) -> None:
    """Detects incomplete tests."""
    run_markers = [m for m in self.gtest_markers if m['type'] == 'run']
    finished_tests = set()
    for marker in self.gtest_markers:
        if marker['type'] in ['ok', 'failed', 'skipped']:
            finished_tests.add(marker['name'])

    for run_marker in run_markers:
      if run_marker['name'] not in finished_tests:
         start, end = self.test_boundaries[run_marker['name']]
         self.issues.append(models.LogEvent(line_num=start, issue_type='incomplete_test', data={'test_name': run_marker['name'], 'context': (start, end)}))
