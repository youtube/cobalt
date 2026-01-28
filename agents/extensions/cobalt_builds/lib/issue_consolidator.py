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
"""A parser for consolidating issues."""

from collections import defaultdict
from typing import Dict, List

import models


class IssueConsolidator:
  """Consolidates issues."""

  def __init__(self, lines: List[str], events: List[models.LogEvent]):
    self.lines = lines
    self.events = events
    self.report_items: List[models.Issue] = []

  def consolidate(self) -> None:
    """Consolidates the issues."""
    self._consolidate_issues()

  def _consolidate_issues(self) -> None:
    """Consolidates the issues."""
    reported_lines = set()
    global_consolidated_reports = {}

    # 1. Pre-process to mark crashes that are already handled by an incomplete test
    incomplete_tests = [e for e in self.events if e.issue_type == 'incomplete_test']
    crashes = [e for e in self.events if e.issue_type == 'crash']

    for test in incomplete_tests:
        test_name = test.data['test_name']
        start, end = test.data['context']
        test.data['crash_events'] = []
        for crash in crashes:
            if crash.data['test_name'] == test_name and start <= crash.line_num < end:
                crash.data['handled_by_incomplete_test'] = True
                test.data['crash_events'].append(crash)
        if test.data['crash_events']:
            test.data['crash_event'] = test.data['crash_events'][0]


    # 2. Consolidate gtest failures into proper clusters first.
    gtest_fail_events = [e for e in self.events if e.issue_type == 'gtest_fail']
    gtest_failure_line_events = [e for e in self.events if e.issue_type == 'gtest_failure_line']
    other_events = [e for e in self.events if e.issue_type not in ['gtest_fail', 'gtest_failure_line']]

    for fail_event in gtest_fail_events:
      test_name = fail_event.data['test_name']
      start_line = fail_event.data['start_line']
      end_line = fail_event.line_num + 1

      # Find all issues within this test's boundaries
      inner_events = [fail_event]

      # Explicitly find and consume gtest_failure_line events
      for failure_event in gtest_failure_line_events:
          if start_line <= failure_event.line_num < end_line:
              inner_events.append(failure_event)
              reported_lines.add(failure_event.line_num) # Mark as handled

      for other_event in other_events:
          if start_line <= other_event.line_num < end_line:
              inner_events.append(other_event)
              reported_lines.add(other_event.line_num) # Mark as handled

      # Create a synthetic cluster for this failed test
      self.report_items.append(models.Issue(
          issue_type='grouped_cluster',
          events=inner_events,
          context=(start_line, end_line),
          test_name=test_name,
          occurrences=[{
              'test_name': test_name,
              'line_content': fail_event.data.get('line_content', '')
          }],
      ))
      reported_lines.add(fail_event.line_num)


    # 3. Group remaining events by test name.
    events_by_test = defaultdict(list)
    for event in self.events:
      if event.line_num not in reported_lines and event.issue_type not in ['gtest_run', 'gtest_fail']:
        # Skip crashes that will be handled by the incomplete_test logic
        if event.data.get('handled_by_incomplete_test'):
            continue
        test_name = event.data.get('test_name')
        events_by_test[test_name].append(event)

    # 4. First Pass: Cluster within each test group.
    # 5. Second Pass: Consolidate identical clusters globally.
    for test_name, test_events in events_by_test.items():
      # Handle incomplete tests separately, don't cluster them.
      incomplete_events = [e for e in test_events if e.issue_type == 'incomplete_test']
      for event in incomplete_events:
          self.report_items.append(models.Issue(
              issue_type=event.issue_type,
              events=[event],
              context=event.data['context'],
              test_name=event.data['test_name'],
          ))

      # Cluster the rest of the events.
      remaining_events = [e for e in test_events if e.issue_type != 'incomplete_test']
      if not remaining_events:
          continue

      clusters = []
      current_cluster = []
      last_event_line = -10

      for event in remaining_events:
        # If the pattern of errors starts repeating, start a new cluster. 
        if current_cluster and event.data['grouping_key'] == current_cluster[0].data['grouping_key']:
            clusters.append(current_cluster)
            current_cluster = []

        if current_cluster and (event.line_num - last_event_line) > 5:
          clusters.append(current_cluster)
          current_cluster = []
        current_cluster.append(event)
        last_event_line = event.line_num

      if current_cluster:
        clusters.append(current_cluster)

      for cluster in clusters:
        signature = frozenset(e.data['grouping_key'] for e in cluster if 'grouping_key' in e.data)
        if not signature:
            continue

        new_test_name = cluster[0].data.get('test_name')

        if signature not in global_consolidated_reports:
          issue_type = 'grouped_cluster' if len(cluster) > 1 else cluster[0].issue_type
          global_consolidated_reports[signature] = models.Issue(
              issue_type=issue_type,
              events=cluster,
              context=(max(0, cluster[0].line_num - 3), min(len(self.lines), cluster[-1].line_num + 4)),
              test_name=new_test_name,
              occurrences=[{
                  'test_name': new_test_name,
                  'line_content': cluster[0].data['line_content']
              }],
              signature=signature
          )
        else:
          report = global_consolidated_reports[signature]
          report.occurrences.append({
              'test_name': new_test_name,
              'line_content': cluster[0].data['line_content']
          })

    # Add the globally consolidated reports to the final list.
    self.report_items.extend(global_consolidated_reports.values())

    self.report_items.sort(key=lambda item: item.context[0] if hasattr(item, 'context') else item.events[0].line_num)

    # --- Post-processing to handle superset/subset clusters ---
    # Identify supersets and augment subsets
    for sig_a, report_a in global_consolidated_reports.items():
      for sig_b, report_b in global_consolidated_reports.items():
        if sig_a == sig_b:
          continue
        # If A is a proper subset of B, B is a superset.
        if sig_a.issubset(sig_b) and sig_a != sig_b:
          report_b.handled = True
          if not hasattr(report_a, 'extra_context_tests'):
            report_a.extra_context_tests = []
          # Add the context from the superset to the subset
          report_a.extra_context_tests.extend(report_b.occurrences)

    # Filter out the superset reports
    self.report_items = [
        r for r in self.report_items
        if not getattr(r, 'handled', False)
    ]
