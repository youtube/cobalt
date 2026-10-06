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
import sys
import xml.etree.ElementTree as ET

# Characters disallowed by XML 1.0 (excluding valid whitespace \t, \n, \r):
# Disallowed: 0x00-0x08, 0x0B, 0x0C, 0x0E-0x1F, 0x7F-0x84, 0x86-0x9F
_DISALLOWED_CODEPOINTS = (
    set(range(0x00, 0x09))
    | {0x0B, 0x0C}
    | set(range(0x0E, 0x20))
    | set(range(0x7F, 0x85))
    | set(range(0x86, 0xA0)))
_XML_CLEAN_TABLE = dict.fromkeys(_DISALLOWED_CODEPOINTS, None)


def _sanitize_xml_string(s):
  if not s:
    return ''
  return str(s).translate(_XML_CLEAN_TABLE)


def _normalize_status(raw_status):
  status_str = str(raw_status).upper()
  if 'FAIL' in status_str:
    return 'FAILURE'
  if any(code in status_str for code in ('CRASH', 'TIMEOUT', 'ERROR')):
    return 'ERROR'
  if 'SKIP' in status_str:
    return 'SKIPPED'
  return 'SUCCESS'


def _parse_entry_duration(test):
  if 'duration' in test:
    try:
      return float(test['duration'])
    except (ValueError, TypeError):
      return 0.0
  if 'start_time' in test and 'end_time' in test:
    try:
      return max(0.0,
                 (float(test['end_time']) - float(test['start_time'])) / 1000.0)
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
  """Extracts test cases grouped by classname from heterogeneous schemas."""
  suites = collections.defaultdict(list)

  if isinstance(data, dict) and 'per_iteration_data' in data:
    for iteration in data.get('per_iteration_data', []):
      for test_key, results in iteration.items():
        for res in results:
          if not isinstance(res, dict):
            continue
          classname, _, method = test_key.partition('#')
          if not method:
            classname, method = 'UnknownClass', classname
          try:
            elapsed_time = float(res.get('elapsed_time_ms') or 0) / 1000.0
          except (ValueError, TypeError):
            elapsed_time = 0.0
          suites[classname].append({
              'name': method.split('[')[0],
              'status': _normalize_status(res.get('status', 'SUCCESS')),
              'time': elapsed_time,
              'output': res.get('output_snippet', '')
          })
    return suites

  tests = data.get('tests', []) if isinstance(
      data, dict) else (data if isinstance(data, list) else [])
  for t in tests:
    if not isinstance(t, dict):
      continue
    name = t.get('test_title') or t.get('name') or t.get(
        'test_category') or 'UnknownTest'
    classname = t.get('class_name') or t.get('suite_name') or t.get(
        'test_category') or 'VegaTest'
    suites[classname].append({
        'name': name,
        'status': _normalize_status(t.get('result', t.get('status', 'PASSED'))),
        'time': _parse_entry_duration(t),
        'output': _parse_entry_output(t)
    })
  return suites


def convert(json_path, xml_path):
  with open(json_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

  suites = _extract_cases(data)
  testsuites = ET.Element('testsuites')

  all_cases = [c for cases in suites.values() for c in cases]
  testsuites.set('tests', str(len(all_cases)))
  testsuites.set(
      'failures',
      str(sum(1 for c in all_cases if c['status'] in ('FAILURE', 'FAIL'))))
  testsuites.set(
      'errors',
      str(
          sum(1 for c in all_cases
              if c['status'] in ('CRASH', 'TIMEOUT', 'ERROR'))))
  testsuites.set('time', f"{sum(c['time'] for c in all_cases):.3f}")
  testsuites.set('name', 'AllTests')

  for suite_name, cases in suites.items():
    suite_el = ET.SubElement(testsuites, 'testsuite')
    suite_el.set('name', suite_name)
    suite_el.set('tests', str(len(cases)))
    suite_el.set(
        'failures',
        str(sum(1 for c in cases if c['status'] in ('FAILURE', 'FAIL'))))
    suite_el.set(
        'errors',
        str(
            sum(1 for c in cases
                if c['status'] in ('CRASH', 'TIMEOUT', 'ERROR'))))
    suite_el.set('time', f"{sum(c['time'] for c in cases):.3f}")

    for case in cases:
      case_el = ET.SubElement(suite_el, 'testcase')
      case_el.set('name', case['name'])
      case_el.set('classname', suite_name)
      case_el.set('time', f"{case['time']:.3f}")

      if case['status'] in ('FAILURE', 'FAIL'):
        fail_el = ET.SubElement(case_el, 'failure')
        fail_el.set('message', 'Test failed')
        fail_el.text = _sanitize_xml_string(case['output'])
      elif case['status'] in ('CRASH', 'TIMEOUT', 'ERROR'):
        err_el = ET.SubElement(case_el, 'error')
        err_el.set('message', 'Test ' + case['status'])
        err_el.text = _sanitize_xml_string(case['output'])
      elif case['status'] in ('SKIPPED', 'SKIP'):
        ET.SubElement(case_el, 'skipped')

  tree = ET.ElementTree(testsuites)
  if hasattr(ET, 'indent'):
    ET.indent(tree, space='  ', level=0)
  tree.write(xml_path, encoding='utf-8', xml_declaration=True)


if __name__ == '__main__':
  if len(sys.argv) < 3:
    print('Usage: convert_json_to_junit_xml.py <input_json> <output_xml>')
    sys.exit(1)
  convert(sys.argv[1], sys.argv[2])
