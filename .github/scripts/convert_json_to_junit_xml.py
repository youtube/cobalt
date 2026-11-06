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
"""Converts a chromium test JSON results file to JUnit XML format."""

import json
import xml.etree.ElementTree as ET
import sys
import re


def _sanitize_xml_string(s):
  if not s:
    return ''
  # Remove invalid XML 1.0 characters
  return re.sub((r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x84\x86-\x9f'
                 r'\ud800-\udfff\ufffe\uffff]'), '', str(s))


def _iter_test_cases(data):
  for iteration in data.get('per_iteration_data', []):
    for test_key, results in iteration.items():
      classname, method = (
          test_key.split('#', 1) if '#' in test_key else
          ('UnknownClass', test_key))
      method = method.split('[')[0]
      for res in results:
        yield (
            classname,
            method,
            res.get('status', 'SUCCESS'),
            (res.get('elapsed_time_ms') or 0) / 1000.0,
            res.get('output_snippet', ''),
        )

  for t in data.get('tests', []):
    classname = (
        t.get('class_name') or t.get('test_suite') or t.get('test_category') or
        'VegaTest')
    name = (
        t.get('test_title') or t.get('name') or t.get('test_category') or
        'UnknownTest')
    raw_status = str(t.get('result') or t.get('status') or 'ERROR').upper()
    if any(s in raw_status for s in ('PASS', 'SUCCESS', 'OK')):
      status = 'SUCCESS'
    elif 'SKIP' in raw_status:
      status = 'SKIPPED'
    elif 'FAIL' in raw_status:
      status = 'FAILURE'
    else:
      status = 'ERROR'
    elapsed_sec = float(
        t.get('duration') or
        max(0, (t.get('end_time') or 0) - (t.get('start_time') or 0)) / 1000.0)
    output = '\n'.join(
        str(x)
        for k in ('output', 'errors')
        for x in (t.get(k) if isinstance(t.get(k), list) else [t.get(k)])
        if x)
    yield classname, name, status, elapsed_sec, output


def convert(json_path, xml_path):
  with open(json_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

  testsuites = ET.Element('testsuites')

  # Group by class name
  suites = {}

  total_tests = 0
  total_failures = 0
  total_errors = 0
  total_time = 0.0

  for (classname, method, status, elapsed_sec,
       output_snippet) in _iter_test_cases(data):
    suites.setdefault(classname, []).append({
        'name': method,
        'status': status,
        'time': elapsed_sec,
        'output': output_snippet
    })

    total_tests += 1
    total_time += elapsed_sec
    if status in ('FAILURE', 'FAIL'):
      total_failures += 1
    elif status in ('CRASH', 'TIMEOUT', 'ERROR'):
      total_errors += 1

  testsuites.set('tests', str(total_tests))
  testsuites.set('failures', str(total_failures))
  testsuites.set('errors', str(total_errors))
  testsuites.set('time', f'{total_time:.3f}')
  testsuites.set('name', 'AllTests')

  for suite_name, cases in suites.items():
    suite_el = ET.SubElement(testsuites, 'testsuite')
    suite_el.set('name', _sanitize_xml_string(suite_name))

    suite_tests = len(cases)
    suite_failures = sum(1 for c in cases if c['status'] in ('FAILURE', 'FAIL'))
    suite_errors = sum(
        1 for c in cases if c['status'] in ('CRASH', 'TIMEOUT', 'ERROR'))
    suite_time = sum(c['time'] for c in cases)

    suite_el.set('tests', str(suite_tests))
    suite_el.set('failures', str(suite_failures))
    suite_el.set('errors', str(suite_errors))
    suite_el.set('time', f'{suite_time:.3f}')

    for case in cases:
      case_el = ET.SubElement(suite_el, 'testcase')
      case_el.set('name', _sanitize_xml_string(case['name']))
      case_el.set('classname', _sanitize_xml_string(suite_name))
      case_time = case['time']
      case_el.set('time', f'{case_time:.3f}')

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
