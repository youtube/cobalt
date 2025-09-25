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
"""Log analysis functions for Cobalt builds and runs."""

import os
import re
from typing import Dict, List

from lib.gtest_parser import GtestParser
from lib.issue_detector import IssueDetector
from lib.issue_consolidator import IssueConsolidator
from lib.report_generator import ReportGenerator
import models
import patterns

# --- Log Analysis ---


class BaseAnalyzer:
  """Base class for log analysis."""

  def __init__(self, lines: List[str]):
    self.lines = lines

  def _strip_timestamp(self, line: str) -> str:
    """Strips the timestamp from a log line for comparison."""
    return patterns.TIMESTAMP_PATTERN.sub('[...:', line)

  def _is_stack_trace_line(self, line: str) -> bool:
    """Checks if a line is likely part of a stack trace."""
    return bool(patterns.STACK_TRACE_LINE_PATTERN.search(line))

  def _extract_and_filter_stack_traces(self, start_line: int,
                                       end_line: int) -> List[List[str]]:
    """Extracts and filters stack traces from a log segment."""
    traces = []
    current_trace = []
    in_trace = False

    for i in range(start_line, end_line):
      line = self.lines[i].rstrip()
      if self._is_stack_trace_line(line):
        if not in_trace:
          in_trace = True
          current_trace = []
        current_trace.append(line)
      elif in_trace:
        in_trace = False
        if current_trace:
          traces.append(current_trace)
        current_trace = []

    if current_trace:
      traces.append(current_trace)

    if not traces:
      return []

    # Filter out subset traces
    maximal_traces = []
    for i, trace_a in enumerate(traces):
      is_subset = False
      set_a = set(trace_a)
      for j, trace_b in enumerate(traces):
        if i == j:
          continue
        set_b = set(trace_b)
        # Check if trace_a is a proper subset of trace_b
        if set_a.issubset(set_b) and set_a != set_b:
          is_subset = True
          break
      if not is_subset:
        maximal_traces.append(trace_a)

    return maximal_traces

  def _get_line_signature(self, line: str) -> str:
    """
    Generates a 'signature' for a log line by replacing variable data
    (numbers, hex addresses, etc.) with placeholders.
    """
    # Replace hex addresses (e.g., 0x7f123456)
    line = re.sub(r'0x[0-9a-fA-F]+', '0x<HEX>', line)
    # Replace timestamps like HH:MM:SS.ms
    line = re.sub(r'\d{2}:\d{2}:\d{2}\.\d+', '<TIMESTAMP>', line)
    # Replace general numbers, but try to avoid version numbers like 1.2.3
    line = re.sub(r'(?<!\.)\b\d+\b(?!\.)', '<NUM>', line)
    # Collapse multiple placeholders
    line = re.sub(r'(<NUM>\s*)+', '<NUM> ', line).strip()
    return line

  def _add_skipped_lines_message(self, results: List[str], skipped_count: int,
                                   max_line_num_width: int):
    """Adds a '... (lines skipped) ...' message to the results list."""
    if skipped_count > 0:
      pre_content_width = 3 + max_line_num_width + 2  # prefix + num + ': '
      no_line_num_padding = ' ' * (pre_content_width - 3)
      results.append(
          f'   {no_line_num_padding}... ({skipped_count} lines skipped) ...')

  def _add_log_lines_with_skipping(
      self,
      results: List[str],
      start: int,
      end: int,
      highlight_lines: set,
      context_lines: int = 2,
      filtered_traces: List[List[str]] = None,
      max_line_num_width: int = 0):
    # 1. Pre-computation of sequence collapses
    normalized_lines = {i: self._strip_timestamp(self.lines[i]) for i in range(start, end)}
    sequences = []
    processed_in_repeat = set()

    for j in range(start, end):
      if j in processed_in_repeat:
        continue

      best_repeat_start = -1
      best_repeat_len = 0

      for k in range(j + 1, end):
        if k in processed_in_repeat:
          continue
        if normalized_lines.get(j) == normalized_lines.get(k):
          length = 0
          while (j + length < end and k + length < end and
                 (k + length) not in processed_in_repeat and
                 normalized_lines.get(j + length) == normalized_lines.get(k + length)):
            length += 1
          if length > best_repeat_len:
            best_repeat_len = length
            best_repeat_start = k

      if best_repeat_len > 2:
        sequences.append((j, best_repeat_start, best_repeat_len))
        for i in range(best_repeat_len):
          processed_in_repeat.add(best_repeat_start + i)

    collapse_info = {}
    for orig_start, rep_start, length in sequences:
      collapse_info[rep_start] = (orig_start, length)

    # 2. Pre-computation for un-highlighting and summarizing duplicate lines
    highlighted_line_occurrences = {}
    for line_num in sorted(list(highlight_lines)):
        if start <= line_num < end:
            normalized_line = self._strip_timestamp(self.lines[line_num].strip())
            if normalized_line not in highlighted_line_occurrences:
                highlighted_line_occurrences[normalized_line] = []
            highlighted_line_occurrences[normalized_line].append(line_num)

    lines_to_unhighlight = set()
    for occurrences in highlighted_line_occurrences.values():
        if len(occurrences) > 2:
            for line_num in occurrences[1:]:
                lines_to_unhighlight.add(line_num)


    # 3. Printing logic with all collapses and alignment
    if not max_line_num_width:
      max_line_num_width = len(str(end))
    pre_content_width = 3 + max_line_num_width + 2  # prefix + num + ': '
    no_line_num_padding = ' ' * (pre_content_width - 3)

    filtered_trace_lines = set()
    if filtered_traces:
      for trace in filtered_traces:
        for line in trace:
          filtered_trace_lines.add(line)

    last_printed_line = -1
    j = start
    while j < end:
      if j in collapse_info:
        if last_printed_line != -1 and j > last_printed_line + 1:
          skipped_count = j - (last_printed_line + 1)
          self._add_skipped_lines_message(results, skipped_count, max_line_num_width)

        results.append(
            f'   {no_line_num_padding}... (lines {j+1}-{j+length} are a repeat of lines '
            f'{orig_start+1}-{orig_start+length}) ...')

        last_printed_line = j + length - 1
        j += length
        continue

      line_content = self.lines[j].rstrip()
      is_stack_trace = self._is_stack_trace_line(line_content)
      should_print = False
      if is_stack_trace:
        # If we are filtering traces, only print the maximal ones.
        # If we are NOT filtering, print everything.
        if not filtered_traces or line_content in filtered_trace_lines:
          should_print = True
      else:
        # For everything else within the context block, always print.
        should_print = True

      if should_print:
        if last_printed_line != -1 and j > last_printed_line + 1:
          skipped_count = j - (last_printed_line + 1)
          self._add_skipped_lines_message(results, skipped_count, max_line_num_width)

        prefix = ">> " if j in highlight_lines and j not in lines_to_unhighlight else "   "
        line_num_str = str(j + 1).ljust(max_line_num_width)
        results.append(f"{prefix}{line_num_str}: {line_content}")
        last_printed_line = j

        # Add summary line after the first occurrence of a duplicated highlight
        if j in highlight_lines and j not in lines_to_unhighlight:
            normalized_line = self._strip_timestamp(self.lines[j].strip())
            occurrences = highlighted_line_occurrences.get(normalized_line, [])
            if len(occurrences) > 2:
                other_lines = ", ".join(map(str, [o + 1 for o in occurrences[1:]]))
                results.append(f'   {no_line_num_padding}... (also occurs on lines {other_lines}) ...')

      j += 1


class TestLogAnalyzer(BaseAnalyzer):
  """Analyzes test logs."""

  def analyze(self) -> (str, int, int, Dict):
    """Analyzes the log."""
    gtest_parser = GtestParser(self.lines)
    gtest_parser.parse()
    test_boundaries = gtest_parser.test_boundaries
    gtest_markers = gtest_parser.gtest_markers

    issue_detector = IssueDetector(self.lines, test_boundaries, gtest_markers)
    issue_detector.detect()
    issues = issue_detector.issues

    issue_consolidator = IssueConsolidator(self.lines, issues)
    issue_consolidator.consolidate()
    report_items = issue_consolidator.report_items

    report_generator = ReportGenerator(self, report_items, test_boundaries)
    report_generator.generate()
    return report_generator.report, report_generator.errors, report_generator.warnings, test_boundaries


class BuildLogAnalyzer(BaseAnalyzer):
  """Analyzes build logs."""

  def analyze(self) -> (str, int, int):
    """Analyzes the log."""
    return self._analyze_build_log()

  def _find_build_action_start(self, failed_line_index: int,
                                 build_step_pattern: re.Pattern) -> int:
    """
    Searches backwards from a 'FAILED:' line to find the start of the command.
    """
    for j in range(failed_line_index, -1, -1):
      if build_step_pattern.search(self.lines[j]):
        return j
    return -1

  def _analyze_build_log(self) -> (str, int, int):
    """
    Analyzes a build log for ninja errors and gn warnings, capturing the full
    context of each issue using a single-pass approach.
    """
    results = []
    errors = 0
    warnings = 0
    max_line_num_width = len(str(len(self.lines)))
    reported_lines = set()

    i = 0
    while i < len(self.lines):
      if i in reported_lines:
        i += 1
        continue

      line = self.lines[i]

      # --- GN Warning Detection ---
      if patterns.GN_WARNING_PATTERN.search(line):
        warnings += 1
        start_warning = i
        # Check if the previous line is "Regenerating ninja files"
        if i > 0 and patterns.REGENERATING_NINJA_PATTERN.search(self.lines[i-1]):
          start_warning = i - 1

        end_warning = i + 1
        while end_warning < len(self.lines) and not patterns.BUILD_STEP_PATTERN.search(
            self.lines[end_warning]):
          end_warning += 1

        results.append('--- WARNING ---')

        self._add_log_lines_with_skipping(
            results,
            start_warning,
            end_warning,
            highlight_lines={i},
            max_line_num_width=max_line_num_width)

        for j in range(start_warning, end_warning):
          reported_lines.add(j)
        i = end_warning
        continue

      # --- Build Error Detection ---
      elif patterns.FAILED_PATTERN.search(line):
        errors += 1
        command_start = self._find_build_action_start(i, patterns.BUILD_STEP_PATTERN)

        if command_start != -1:
          # Find the end of the error report
          error_end = i + 1
          while error_end < len(self.lines):
            if patterns.BUILD_STEP_PATTERN.search(self.lines[error_end]):
              break
            if patterns.END_ERROR_PATTERN.search(self.lines[error_end]):
              error_end += 1
              break
            if patterns.END_BUILD_PATTERN.search(self.lines[error_end]):
              error_end += 1
              break
            error_end += 1
          results.append('--- ERROR ---')
          highlight_lines = {i, command_start}
          for j in range(command_start, error_end):
            if 'error:' in self.lines[j]:
              highlight_lines.add(j)

          self._add_log_lines_with_skipping(
              results,
              command_start,
              error_end,
              highlight_lines=highlight_lines,
              max_line_num_width=max_line_num_width)

          for j in range(command_start, error_end):
            reported_lines.add(j)
          i = error_end
          continue

      # --- Other important lines ---
      elif patterns.REGENERATING_NINJA_PATTERN.search(line):
        # This case handles "Regenerating..." ONLY when it's NOT followed by a warning.
        # The case where it is followed by a warning is handled by the gn_warning_pattern block.
        if not (i + 1 < len(self.lines) and patterns.GN_WARNING_PATTERN.search(self.lines[i+1])):
          results.append(f'   {str(i+1).ljust(max_line_num_width)}: {line.rstrip()}')
          reported_lines.add(i)

      elif (
          patterns.NINJA_STOPPED_PATTERN.search(line) or
          patterns.RETURN_CODE_PATTERN.search(line)):
        results.append(f'   {str(i+1).ljust(max_line_num_width)}: {line.rstrip()}')
        reported_lines.add(i)

      i += 1
    return '\n'.join(results), errors, warnings


def analyze_log(task_id_or_log_file: str, log_type: str | None = None) -> str:
  """Analyzes a log for errors and warnings.

  Accepts either a task ID from a build or run, or a direct path to a log file.
  The `log_type` can be 'build' or 'test'. If not provided, it's inferred
  from the log file name (e.g., 'build_...' or 'run_...').
  """
  log_file = task_id_or_log_file
  if not os.path.isfile(log_file):
    if log_file.startswith('build_') or log_file.startswith('run_'):
      log_file = f'/tmp/{log_file}.log'
    if not os.path.isfile(log_file):
      return f"Error: Log file not found at {task_id_or_log_file}"

  base_task_id = os.path.basename(task_id_or_log_file).replace('.log', '')
  analysis_log_path = f'/tmp/{base_task_id}_analysis.log'

  try:
    with open(log_file, 'r') as f:
      lines = f.readlines()
  except FileNotFoundError:
    return f"Error: Log file not found at {log_file}"

  # --- Determine log type ---
  effective_log_type = log_type
  if not effective_log_type:
    base_name = os.path.basename(task_id_or_log_file)
    if base_name.startswith('build'):
      effective_log_type = 'build'
    elif base_name.startswith('run'):
      effective_log_type = 'test'
    else:
      return "Error: Could not determine log type from filename. Please specify 'build' or 'test' for the log_type."

  # --- Main Analysis Pipeline ---
  if effective_log_type == 'build':
    analyzer = BuildLogAnalyzer(lines)
    report_str, errors, warnings = analyzer.analyze()
  else:  # 'test'
    analyzer = TestLogAnalyzer(lines)
    report_str, errors, warnings, _ = analyzer.analyze()
  # --- End of Pipeline ---

  # --- Post-process the report for clean formatting ---
  report_lines = report_str.strip().split('\n')
  while report_lines and not report_lines[-1]:
    report_lines.pop()
  # Remove the trailing separator if it exists
  if report_lines and report_lines[-1].startswith('---'):
      report_lines.pop()
  report_str = '\n'.join(report_lines)
  # --- Final Fallback for Silent Failures ---
  return_code = -1
  if lines:
    last_line = lines[-1].strip()
    if last_line.startswith('RETURN_CODE:'):
      try:
        return_code = int(last_line.split(':')[1].strip())
      except (ValueError, IndexError):
        pass

  if return_code != 0 and not errors:
    errors += 1
    fallback_report = [
        f"--- ERROR: Task failed with non-zero exit code ({return_code}) ---"
    ]
    start_line = max(0, len(lines) - 10)
    for i in range(start_line, len(lines)):
      fallback_report.append(f"   {i+1}: {lines[i].strip()}")
    fallback_report.append("--- " * 20 + "\n")
    report_str += "\n".join(fallback_report)

  if not report_str.strip():
    report_str = "No errors or warnings found."

  summary = f"Log analysis summary: Errors: {errors}, Warnings: {warnings}"
  analysis_content = report_str + f"\n\n{summary}"

  with open(analysis_log_path, 'w') as f:
    f.write(analysis_content)

  return report_str + f"\nFull log saved to: {log_file}\n{summary}"