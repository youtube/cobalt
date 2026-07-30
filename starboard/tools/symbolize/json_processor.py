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
import binascii
import json
import os
import tempfile
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
  """Fast heuristic pre-check to bypass snippets lacking trace signatures."""
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
  except (binascii.Error, ValueError):
    return False

  if not has_stack_trace_signature(decoded):
    return False

  # Replace non-ascii characters with '?' for ASCII-safe test summary output.
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

  if modified_count == 0:
    return 0

  dir_name = os.path.dirname(os.path.abspath(json_path))
  with tempfile.NamedTemporaryFile(
      'w', dir=dir_name, delete=False, encoding='utf-8') as tf:
    json.dump(data, tf, indent=3, sort_keys=True)
    temp_path = tf.name

  os.replace(temp_path, json_path)
  return modified_count
