#!/usr/bin/env python3
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
"""Converts one or more test JSON result files into a single JUnit XML file."""

from datetime import datetime
import json
import re
import sys
import xml.etree.ElementTree as ET

_INVALID_XML_RE = re.compile(
    r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x84\x86-\x9f\ud800-\udfff\ufffe\uffff]')

_STATUS = {
    'PASS': 'SUCCESS',
    'PASSED': 'SUCCESS',
    'SUCCESS': 'SUCCESS',
    'OK': 'SUCCESS',
    'SKIP': 'SKIPPED',
    'SKIPPED': 'SKIPPED',
    'FAIL': 'FAILURE',
    'FAILED': 'FAILURE',
    'FAILURE': 'FAILURE',
}


def _map_status(raw):
  """Maps a raw test result status strictly; unknown/missing becomes ERROR."""
  return _STATUS.get(str(raw).upper(), 'ERROR') if raw is not None else 'ERROR'


def _sanitize(text):
  """Strips characters disallowed in XML 1.0."""
  return _INVALID_XML_RE.sub('', str(text)) if text else ''


def _elapsed_sec(entry):
  """Computes test duration in seconds from duration or start/end timestamps."""
  if entry.get('duration') is not None:
    return max(0.0, float(entry['duration']))
  start, end = entry.get('start_time'), entry.get('end_time')
  if isinstance(start, (int, float)) and isinstance(end, (int, float)):
    return max(0.0, (end - start) / 1000.0)
  if isinstance(start, str) and isinstance(end, str):
    try:
      return max(0.0, (
          datetime.fromisoformat(end.replace('Z', '+00:00')) -
          datetime.fromisoformat(start.replace('Z', '+00:00'))).total_seconds())
    except ValueError:
      return 0.0
  return 0.0


def _iter_cases(data):
  """Yields (classname, method, status, elapsed_sec, output) tuples."""
  for iteration in data.get('per_iteration_data', []):
    for key, results in iteration.items():
      cls, name = key.split('#', 1) if '#' in key else ('UnknownClass', key)
      name = name.split('[')[0]
      for res in results:
        yield (cls, name, _map_status(res.get('status')),
               (res.get('elapsed_time_ms') or 0) / 1000.0,
               res.get('output_snippet', ''))

  for t in data.get('tests', []):
    cls = (
        t.get('class_name') or t.get('test_suite') or t.get('test_category') or
        'VegaTest')
    name = (
        t.get('test_title') or t.get('name') or t.get('test_category') or
        'UnknownTest')
    status = _map_status(t.get('result') or t.get('status'))
    out = '\n'.join(
        str(x)
        for k in ('output', 'errors')
        for x in (t.get(k) if isinstance(t.get(k), list) else [t.get(k)])
        if x)
    yield cls, name, status, _elapsed_sec(t), out


def convert(json_paths, xml_path):
  """Merges 1..N JSON test reports into a single JUnit XML output file."""
  suites = {}
  totals = [0, 0, 0, 0.0]  # tests, failures, errors, time

  for path in json_paths:
    with open(path, 'r', encoding='utf-8') as f:
      data = json.load(f)
    for cls, name, status, elapsed, out in _iter_cases(data):
      suites.setdefault(cls, []).append((name, status, elapsed, out))
      totals[0] += 1
      totals[3] += elapsed
      if status == 'FAILURE':
        totals[1] += 1
      elif status == 'ERROR':
        totals[2] += 1

  if totals[0] == 0:
    sys.exit(f'No test cases found in {json_paths}')

  root = ET.Element(
      'testsuites', {
          'name': 'AllTests',
          'tests': str(totals[0]),
          'failures': str(totals[1]),
          'errors': str(totals[2]),
          'time': f'{totals[3]:.3f}',
      })
  for suite_name, cases in suites.items():
    clean_suite = _sanitize(suite_name)
    s_el = ET.SubElement(
        root, 'testsuite', {
            'name': clean_suite,
            'tests': str(len(cases)),
            'failures': str(sum(1 for c in cases if c[1] == 'FAILURE')),
            'errors': str(sum(1 for c in cases if c[1] == 'ERROR')),
            'time': f'{sum(c[2] for c in cases):.3f}',
        })
    for name, status, elapsed, out in cases:
      c_el = ET.SubElement(
          s_el, 'testcase', {
              'name': _sanitize(name),
              'classname': clean_suite,
              'time': f'{elapsed:.3f}',
          })
      if status == 'FAILURE':
        el = ET.SubElement(c_el, 'failure', {'message': 'Test failed'})
        el.text = _sanitize(out)
      elif status == 'ERROR':
        el = ET.SubElement(c_el, 'error', {'message': f'Test {status}'})
        el.text = _sanitize(out)
      elif status == 'SKIPPED':
        ET.SubElement(c_el, 'skipped')

  tree = ET.ElementTree(root)
  if hasattr(ET, 'indent'):
    ET.indent(tree, space='  ', level=0)
  tree.write(xml_path, encoding='utf-8', xml_declaration=True)


if __name__ == '__main__':
  if len(sys.argv) < 3:
    print(
        'Usage: convert_json_to_junit_xml.py <input_json>... <output_xml>',
        file=sys.stderr)
    sys.exit(1)
  convert(sys.argv[1:-1], sys.argv[-1])
