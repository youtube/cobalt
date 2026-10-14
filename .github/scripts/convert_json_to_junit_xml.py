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
"""Converts JSON test results (GoogleTest, Vega, YTS) to JUnit XML format."""

import collections
import json
import re
import sys
import xml.etree.ElementTree as ET

_XML_DISALLOWED_RE = re.compile(
    r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x84\x86-\x9f\ud800-\udfff\ufffe\uffff]')


def _sanitize_xml_string(s):
  if not s:
    return ''
  return _XML_DISALLOWED_RE.sub('', str(s))


def _normalize_status(raw_status):
  status_str = str(raw_status).upper()
  if any(s in status_str for s in ('PASS', 'SUCCESS', 'OK')):
    return 'SUCCESS'
  if 'SKIP' in status_str:
    return 'SKIPPED'
  if 'FAIL' in status_str:
    return 'FAILURE'
  return 'ERROR'


def _parse_entry_duration(test):
  duration = test.get('duration')
  if duration is not None:
    try:
      return float(duration)
    except (ValueError, TypeError):
      return 0.0
  start_time = test.get('start_time')
  end_time = test.get('end_time')
  if start_time is not None and end_time is not None:
    try:
      return max(0.0, (float(end_time) - float(start_time)) / 1000.0)
    except (ValueError, TypeError):
      return 0.0
  return 0.0


def _parse_entry_output(test):
  lines = []
  for key in ('output', 'errors'):
    val = test.get(key)
    if isinstance(val, list):
      lines.extend(str(item) for item in val if item)
    elif val:
      lines.append(str(val))
  return '\n'.join(lines)


def _extract_cases(data):
  """Extracts test cases grouped by classname."""
  suites = collections.defaultdict(list)

  if isinstance(data, dict) and 'per_iteration_data' in data:
    for iteration in data.get('per_iteration_data') or []:
      for test_key, results in iteration.items():
        for res in results:
          if not isinstance(res, dict):
            continue
          classname, _, method = test_key.partition('#')
          if not method:
            classname, method = 'UnknownClass', classname
          elapsed_ms = res.get('elapsed_time_ms')
          try:
            elapsed_time = float(
                elapsed_ms) / 1000.0 if elapsed_ms is not None else 0.0
          except (ValueError, TypeError):
            elapsed_time = 0.0
          suites[classname].append({
              'name': method.split('[')[0],
              'status': _normalize_status(res.get('status', 'ERROR')),
              'time': elapsed_time,
              'output': res.get('output_snippet', '')
          })
    return suites

  tests = data.get('tests', []) if isinstance(
      data, dict) else (data if isinstance(data, list) else [])
  for t in tests:
    if not isinstance(t, dict):
      continue
    name = (
        t.get('test_title') or t.get('name') or t.get('test_category') or
        'UnknownTest')
    classname = (
        t.get('class_name') or t.get('suite_name') or t.get('test_suite') or
        t.get('test_category') or 'VegaTest')
    suites[classname].append({
        'name': str(name),
        'status': _normalize_status(t.get('result', t.get('status', 'ERROR'))),
        'time': _parse_entry_duration(t),
        'output': _parse_entry_output(t)
    })
  return suites


def _compute_stats(cases):
  failures = sum(1 for c in cases if c['status'] in ('FAILURE', 'FAIL'))
  errors = sum(1 for c in cases if c['status'] in ('CRASH', 'TIMEOUT', 'ERROR'))
  total_time = sum(c['time'] for c in cases)
  return {
      'tests': str(len(cases)),
      'failures': str(failures),
      'errors': str(errors),
      'time': f'{total_time:.3f}',
  }


def _add_testcase(parent, case, suite_name):
  case_el = ET.SubElement(
      parent,
      'testcase',
      {
          'name': _sanitize_xml_string(case['name']),
          'classname': _sanitize_xml_string(suite_name),
          'time': f"{case['time']:.3f}",
      },
  )
  status = case['status']
  output_text = _sanitize_xml_string(case['output'])
  if status in ('FAILURE', 'FAIL'):
    ET.SubElement(case_el, 'failure', {
        'message': 'Test failed'
    }).text = output_text
  elif status in ('CRASH', 'TIMEOUT', 'ERROR'):
    ET.SubElement(case_el, 'error', {
        'message': f'Test {status}'
    }).text = output_text
  elif status in ('SKIPPED', 'SKIP'):
    ET.SubElement(case_el, 'skipped')


def convert(json_path, xml_path):
  with open(json_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

  suites = _extract_cases(data)
  all_cases = [c for cases in suites.values() for c in cases]

  testsuites = ET.Element('testsuites', _compute_stats(all_cases))
  testsuites.set('name', 'AllTests')

  for suite_name, cases in suites.items():
    suite_el = ET.SubElement(testsuites, 'testsuite', {
        'name': _sanitize_xml_string(suite_name),
        **_compute_stats(cases)
    })
    for case in cases:
      _add_testcase(suite_el, case, suite_name)

  tree = ET.ElementTree(testsuites)
  if hasattr(ET, 'indent'):
    ET.indent(tree, space='  ')
  tree.write(xml_path, encoding='utf-8', xml_declaration=True)


if __name__ == '__main__':
  if len(sys.argv) != 3:
    print('Usage: convert_json_to_junit_xml.py <input.json> <output.xml>')
    sys.exit(1)
  convert(sys.argv[1], sys.argv[2])
