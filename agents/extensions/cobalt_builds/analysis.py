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
from base_analyzer import BaseAnalyzer

# --- Log Analysis ---



class TestLogAnalyzer(BaseAnalyzer):
  """Analyzes test logs."""

  def analyze(self) -> tuple[str, int, int, Dict[str, tuple[int, int]]]:
    """Analyzes the log."""
    gtest_parser = GtestParser(self.lines)
    gtest_parser.parse()
    test_boundaries = gtest_parser.test_boundaries
    gtest_markers = gtest_parser.gtest_markers

    issue_detector = IssueDetector(self, test_boundaries, gtest_markers)
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

  def analyze(self) -> tuple[str, int, int]:
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

  def _analyze_build_log(self) -> tuple[str, int, int]:
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

      # --- Unified Build Error Detection ---
      elif patterns.BUILD_ERROR_TRIGGER_PATTERN.search(line):
        errors += 1
        # Find the start of the command, first by looking for a build step.
        command_start = self._find_build_action_start(
            i, patterns.BUILD_STEP_PATTERN)

        # If no build step is found, fall back to the "Regenerating" line.
        if command_start == -1:
          for j in range(i, -1, -1):
            if patterns.REGENERATING_NINJA_PATTERN.search(self.lines[j]):
              command_start = j
              break
          else:
            # If all else fails, start at the error line itself.
            command_start = i

        # Find the end of the error report
        error_end = i + 1
        while error_end < len(self.lines):
          current_line = self.lines[error_end]
          if patterns.BUILD_STEP_PATTERN.search(current_line):
            break
          if patterns.END_ERROR_PATTERN.search(current_line):
            error_end += 1
            break
          if patterns.NINJA_ERROR_PATTERN.search(current_line):
            error_end += 1
            break
          error_end += 1

        results.append('--- ERROR ---')
        highlight_lines = set()
        for j in range(command_start, error_end):
          if patterns.BUILD_ERROR_TRIGGER_PATTERN.search(self.lines[j]):
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
      elif patterns.NINJA_ERROR_PATTERN.search(line):
        errors += 1
        results.append('--- ERROR ---')
        results.append(f'>> {str(i+1).ljust(max_line_num_width)}: {line.rstrip()}')
        reported_lines.add(i)

      elif patterns.REGENERATING_NINJA_PATTERN.search(line):
        # This case handles "Regenerating..." ONLY when it's NOT followed by a warning.
        # The case where it is followed by a warning is handled by the gn_warning_pattern block.
        if not (i + 1 < len(self.lines) and patterns.GN_WARNING_PATTERN.search(self.lines[i+1])):
          results.append(f'   {str(i+1).ljust(max_line_num_width)}: {line.rstrip()}')
          reported_lines.add(i)

      elif (
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