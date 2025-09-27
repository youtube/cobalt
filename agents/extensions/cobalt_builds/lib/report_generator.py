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
"""A parser for generating reports."""

from typing import Dict, List

import models
import patterns
from base_analyzer import BaseAnalyzer


class ReportGenerator:
  """Generates reports."""

  def __init__(self, analyzer: BaseAnalyzer, report_items: List[models.Issue],
               test_boundaries: Dict[str, tuple[int, int]]):
    self.analyzer = analyzer
    self.lines = analyzer.lines
    self.report_items = report_items
    self.test_boundaries = test_boundaries
    self.report: str = ''
    self.errors: int = 0
    self.warnings: int = 0

  def generate(self) -> None:
    """Generates the report."""
    self._generate_report()

  def _generate_report(self) -> None:
    """Generates the report."""
    results = []
    max_line_num_width = len(str(len(self.lines)))

    def _format_occurrence_detail(total: int, exact: int, similar: int, is_main_header: bool = False) -> str:
      if total <= 1:
        return ''
      # For the main header, exact=1 is the baseline (it's the reference line),
      # so we can simplify the output to only show the similar count.
      if is_main_header and exact == 1 and similar > 0:
        return f' ({total} times; {similar} similar)'
      if similar == 0:
        return f' ({total} times exact)'
      # For context lines, exact can be 0.
      if exact == 0:
        return f' ({total} times similar)'
      return f' ({total} times; {exact} exact, {similar} similar)'

    # Define gtest patterns here for context checking
    gtest_run_pattern = patterns.GTEST_RUN_PATTERN_SIMPLE
    gtest_ok_pattern = patterns.GTEST_OK_PATTERN_SIMPLE
    gtest_failed_pattern = patterns.GTEST_FAILED_PATTERN_SIMPLE
    gtest_skipped_pattern = patterns.GTEST_SKIPPED_PATTERN_SIMPLE

    self.report_items.sort(
        key=lambda item: item.context[0] if hasattr(item, 'context') else item.events[0].line_num
    )

    reported_multi_test_signatures = set()

    for item in self.report_items:
      is_multi_test = len(
          set(o['test_name'] for o in item.occurrences)) > 1
      # Treat gtest failures as single-test issues for now to preserve context
      if item.issue_type == 'grouped_cluster' and any(
          e.issue_type == 'gtest_fail' for e in item.events):
        is_multi_test = False

      signature = item.signature
      if is_multi_test and signature and signature in reported_multi_test_signatures:
        continue  # Skip, already reported.

      primary_type = item.issue_type
      if primary_type == 'grouped_cluster':
        if any(e.issue_type in ['error', 'crash', 'ninja_error', 'gtest_fail', 'gtest_failure_line', 'check_failure'] for e in item.events):
          primary_type = 'error'
        else:
          primary_type = 'warning'

      total_occurrences = len(item.occurrences) or 1

      if primary_type in [
          'gtest_fail', 'crash', 'ninja_error', 'error', 'grouped_cluster',
          'incomplete_test', 'check_failure', 'gtest_failure_line'
      ]:
        self.errors += total_occurrences
      elif primary_type == 'warning':
        self.warnings += total_occurrences

      # --- Test Context Handling ---
      test_name = item.test_name
      is_finished_test_with_issues = False
      if test_name and test_name in self.test_boundaries:
        test_start, test_end = self.test_boundaries[test_name]
        if hasattr(item, 'context'):
          item.context = (
              max(item.context[0], test_start),
              min(item.context[1], test_end))

        if 0 < test_end <= len(self.lines):
          final_status_line = self.lines[test_end - 1]
          if gtest_ok_pattern.search(final_status_line) or \
             gtest_failed_pattern.search(final_status_line) or \
             gtest_skipped_pattern.search(final_status_line):
            is_finished_test_with_issues = True
      # --- End Test Context Handling ---

      will_print_run_line = is_finished_test_with_issues or (
          test_name and hasattr(item, 'context') and test_name in self.test_boundaries)

      item_type = item.issue_type

      # --- Generate Report Header ---
      header = ''
      if is_multi_test:
        title = 'ISSUE'  # Default title
        if any(e.issue_type == 'crash' for e in item.events):
          title = 'CRASH'
        elif not any(e.issue_type in [
            'error', 'crash', 'ninja_error', 'gtest_fail',
            'gtest_failure_line'
        ] for e in item.events):
          title = 'WARNING'

        occurrences = item.occurrences
        total = len(occurrences)
        num_tests = len(set(o['test_name'] for o in occurrences))

        first_line_content = occurrences[0]['line_content']
        exact_matches = sum(
            1 for o in occurrences if o['line_content'] == first_line_content)
        similar_matches = total - exact_matches

        occurrence_detail = _format_occurrence_detail(
            total, exact_matches, similar_matches, is_main_header=True)

        header = f'--- {title}{occurrence_detail} across {num_tests} tests ---'
      else:
        if item_type == 'incomplete_test':
          if item.events and item.events[0].data.get('crash_events'):
            occurrence_info = f" (occurred {len(item.events[0].data['crash_events'])} times)" if len(
                item.events[0].data['crash_events']) > 1 else ''
            header = f'--- CRASH{occurrence_info} ---'
          else:
            header = f"--- ERROR: Test '{item.test_name}' did not complete ---"
        elif item.issue_type == 'crash':
          event = item.events[0]
          test_name_info = f" during test '{event.data['test_name']}'" if event.data['test_name'] else ''
          header = f"--- CRASH DETECTED: {event.data['signal']}{test_name_info} ---"
        elif item_type in ['grouped_cluster', 'warning', 'error', 'check_failure', 'gtest_failure_line', 'ninja_error']:
          title = 'ERROR'
          if primary_type == 'warning':
            title = 'WARNING'
          
          occurrences = item.occurrences
          total = len(occurrences)
          first_line_content = occurrences[0]['line_content']
          exact_matches = sum(
              1 for o in occurrences if o['line_content'] == first_line_content)
          similar_matches = total - exact_matches

          occurrence_info = _format_occurrence_detail(
              total, exact_matches, similar_matches, is_main_header=True)

          test_name_info = f" during test '{test_name}'" if test_name and not will_print_run_line else ''
          if not test_name:
            header = f'--- {title} ---'
          else:
            header = f'--- {title}{occurrence_info}{test_name_info} ---'

      if header:
        results.append(header)

      # --- Prepend [RUN] line if necessary ---
      if will_print_run_line:
        test_start, _ = self.test_boundaries[test_name]
        item_start = item.context[0]
        if item_start > test_start:
          line_num_str = str(test_start + 1).ljust(max_line_num_width)
          run_line = self.lines[test_start].strip()
          results.append(f'   {line_num_str}: {run_line}')
          skipped_count = item_start - (test_start + 1)
          self.analyzer._add_skipped_lines_message(results, skipped_count, max_line_num_width)

      # --- Generate Report Body ---
      if is_multi_test:
        # Add a representative log snippet from the first occurrence
        first_event = item.events[0]
        line_num = first_event.line_num

        # Find the boundaries of the test where the first event occurred
        first_event_test_name = first_event.data.get('test_name')
        start, end = item.context  # Fallback
        if first_event_test_name and first_event_test_name in self.test_boundaries:
          start, end = self.test_boundaries[first_event_test_name]

        highlight_lines = {
            e.line_num for e in item.events if start <= e.line_num < end
        }

        self.analyzer._add_log_lines_with_skipping(
            results,
            start,
            end,
            highlight_lines,
            max_line_num_width=max_line_num_width)

        # Add the summary of all affected tests
        from collections import Counter
        occurrences = item.occurrences
        primary_line_content = occurrences[0]['line_content']
        test_counts = Counter(o['test_name'] for o in occurrences)

        summary_parts = []
        for test_name, total_count in sorted(test_counts.items()):
          occurrences_for_this_test = [
              o for o in occurrences
              if o.get('test_name') == test_name
          ]
          exact_matches = sum(1 for o in occurrences_for_this_test
                              if o['line_content'] == primary_line_content)
          similar_matches = total_count - exact_matches

          detail_str = _format_occurrence_detail(total_count, exact_matches,
                                                 similar_matches)
          summary_parts.append(f'{test_name}{detail_str}')

        summary = ', '.join(summary_parts)
        results.append('\nOccurred in these tests: ' + summary)

        if signature:
          reported_multi_test_signatures.add(signature)
      else:
        # Handle single-test items
        if item_type == 'incomplete_test':
          start, end = item.context
          if item.events and item.events[0].data.get('crash_events'):
            filtered_traces = self.analyzer._extract_and_filter_stack_traces(start, end)
            highlight_lines = {
                e.line_num for e in item.events[0].data['crash_events']
            }
            for trace in filtered_traces:
              for line_content in trace:
                for j in range(start, end):
                  if line_content in self.lines[j]:
                    highlight_lines.add(j)
                    break
            self.analyzer._add_log_lines_with_skipping(
                results,
                start,
                end,
                highlight_lines,
                filtered_traces=filtered_traces,
                max_line_num_width=max_line_num_width)
          else:
            self.analyzer._add_log_lines_with_skipping(
                results,
                start,
                end,
                set(),
                max_line_num_width=max_line_num_width)

        elif item.issue_type == 'crash':
          if test_name and any(
              it.issue_type == 'incomplete_test' and
              it.test_name == test_name
              for it in self.report_items):
            pass  # Skip redundant, less-detailed report.
          else:
            start, end = item.context
            self.analyzer._add_log_lines_with_skipping(
                results,
                start,
                end,
                {item.events[0].line_num},
                max_line_num_width=max_line_num_width)

        elif item_type in ['grouped_cluster', 'warning', 'error', 'check_failure', 'gtest_failure_line', 'ninja_error']:
          start, end = item.context
          highlight_lines = {e.line_num for e in item.events}
          self.analyzer._add_log_lines_with_skipping(
              results,
              start,
              end,
              highlight_lines,
              max_line_num_width=max_line_num_width)

      # --- Append extra context from superset reports ---
      if hasattr(item, 'extra_context_tests'):
        from collections import Counter
        current_test_name = item.test_name
        primary_line_content = item.occurrences[0]['line_content']

        # Filter out occurrences without a test name
        valid_extra_occurrences = [
            o for o in item.extra_context_tests if o.get('test_name')
        ]
        test_counts = Counter(o['test_name'] for o in valid_extra_occurrences)

        if test_counts:
          is_single_test = len(test_counts) == 1
          single_test_name = list(test_counts.keys())[0] if is_single_test else None

          results.append('')  # Add the requested blank line.

          if is_single_test and single_test_name == current_test_name:
            count = test_counts[single_test_name]
            exact_matches = sum(1 for o in valid_extra_occurrences
                                if o['line_content'] == primary_line_content)
            similar_matches = count - exact_matches

            count_str = _format_occurrence_detail(count, exact_matches,
                                                  similar_matches)

            results.append(
                f'Also occurred in combination with other failures in this test{count_str}.')
          else:
            summary_parts = []
            for test_name, total_count in sorted(test_counts.items()):
              occurrences_for_this_test = [
                  o for o in valid_extra_occurrences
                  if o.get('test_name') == test_name
              ]
              exact_matches = sum(1 for o in occurrences_for_this_test
                                  if o['line_content'] == primary_line_content)
              similar_matches = total_count - exact_matches

              detail_str = _format_occurrence_detail(total_count, exact_matches,
                                                     similar_matches)

              summary_parts.append(f'{test_name}{detail_str}')

            summary = ', '.join(summary_parts)
            results.append(
                'Also occurred in combination with other failures in these tests: ')
      # --- Append final status line if necessary ---
      if is_finished_test_with_issues and not is_multi_test:
        _, test_end = self.test_boundaries[test_name]
        final_status_line_index = test_end - 1
        last_printed_line = -1
        if hasattr(item, 'context'):
          last_printed_line = item.context[1] - 1

        if final_status_line_index > last_printed_line:
          skipped_count = final_status_line_index - (last_printed_line + 1)
          if skipped_count > 0:
            pre_content_width = 3 + max_line_num_width + 2
            no_line_num_padding = ' ' * (pre_content_width - 3)
            results.append(
                f'   {no_line_num_padding}... ({skipped_count} lines skipped) ...')

          line_num_str = str(final_status_line_index + 1).ljust(
              max_line_num_width)
          final_status_line_content = self.lines[final_status_line_index].strip()
          results.append(f'   {line_num_str}: {final_status_line_content}')
    self.report = '\n'.join(results)
