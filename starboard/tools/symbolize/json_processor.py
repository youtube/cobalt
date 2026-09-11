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
"""Decoupled test runner JSON summary processor with fast pre-filtering."""

import base64
import json
from typing import Callable, Iterable

# Signatures indicating that a log snippet may contain symbolizable traces.
STACK_TRACE_SIGNATURES = (
    '#0',
    'pc ',
    '[0x',
    '(<unknown',
    'Load start=',
    'CRASH LOG:',
    'AddressSanitizer',
    'LeakSanitizer',
    'SbLogRawDumpStack',
)


def has_stack_trace_signature(text: str) -> bool:
  """Fast pre-check for stack trace signatures in O(1)."""
  return any(sig in text for sig in STACK_TRACE_SIGNATURES)


def process_test_run(test_run: dict,
                     symbolize_fn: Callable[[Iterable[str]], str],
                     processor_tag: str = 'asan_symbolize.py') -> bool:
  """Symbolizes stack traces within a single test run entry if needed.

  Returns True if the snippet was modified, False otherwise.
  """
  snippet_b64 = test_run.get('output_snippet_base64')
  if not snippet_b64:
    return False

  # Quick pre-filter against decoded or raw snippet if present.
  existing_snippet = test_run.get('output_snippet')
  if existing_snippet and not has_stack_trace_signature(existing_snippet):
    return False

  try:
    decoded = base64.b64decode(snippet_b64).decode('utf-8', 'replace')
  except Exception:  # pylint: disable=broad-except
    return False

  if not has_stack_trace_signature(decoded):
    return False

  # Replace non-ascii characters with '?' to match legacy test runner behavior.
  sanitized = ''.join(c if c <= '\x7e' else '?' for c in decoded)

  lines = sanitized.splitlines(keepends=True)
  symbolized = symbolize_fn(lines)

  if symbolized in (sanitized, decoded):
    return False

  test_run['original_output_snippet'] = test_run.get('output_snippet', decoded)
  test_run['original_output_snippet_base64'] = snippet_b64
  test_run['output_snippet'] = symbolized
  test_run['output_snippet_base64'] = base64.b64encode(
      symbolized.encode('utf-8', 'replace')).decode()
  test_run['snippet_processed_by'] = processor_tag
  return True


def process_test_summary_json(json_path: str,
                              symbolize_fn: Callable[[Iterable[str]], str],
                              processor_tag: str = 'asan_symbolize.py') -> int:
  """Parses test_summary.json, symbolizes crash snippets, and rewrites file."""
  with open(json_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

  modified_count = 0
  for iteration in data.get('per_iteration_data', []):
    for _, test_runs in iteration.items():
      for test_run in test_runs:
        if process_test_run(test_run, symbolize_fn, processor_tag):
          modified_count += 1

  with open(json_path, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=3, sort_keys=True)

  return modified_count
