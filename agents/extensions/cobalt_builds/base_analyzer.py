#!usr/bin/env vpython3
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
"""Base class for log analysis."""

import re
from typing import List

import patterns


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
                                   max_line_num_width: int) -> None:
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
      filtered_traces: List[List[str]] | None = None,
      max_line_num_width: int = 0) -> None:
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
