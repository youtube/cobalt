#!/usr/bin/env vpython3
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
"""Runs a unittest module under the GTest CLI contract used by on_host_tests.

Honors --gtest_filter, --gtest_shard_index / --gtest_total_shards (or the
GTEST_* env vars) and --gtest_output=xml:<path>. Other flags are ignored.
"""

import fnmatch
import os
import sys
import time
import unittest
from xml.etree import ElementTree as ET


def flag(name, default):
  """Returns the last --<name>=<value> in argv, else $<NAME>, else default."""
  prefix = f'--{name}='
  values = [a[len(prefix):] for a in sys.argv if a.startswith(prefix)]
  return values[-1] if values else os.environ.get(name.upper(), default)


def _flatten(suite):
  for test in suite:
    is_suite = isinstance(test, unittest.TestSuite)
    yield from _flatten(test) if is_suite else [test]


def _selected(name, gtest_filter):
  positive, _, negative = gtest_filter.partition('-')
  globs = lambda s: any(fnmatch.fnmatch(name, g) for g in s.split(':') if g)
  return globs(positive or '*') and not globs(negative)


def main(module=None):
  """Runs `Class.method` tests from `module` (default: __main__)."""
  module = module or sys.modules['__main__']
  shard, shards = int(flag('gtest_shard_index', 0)), int(
      flag('gtest_total_shards', 1))
  tests = [(t, '.'.join(t.id().rsplit('.', 2)[-2:]))
           for t in _flatten(unittest.defaultTestLoader.loadTestsFromModule(
               module))]
  tests = [x for x in tests if _selected(x[1], flag('gtest_filter', '*'))]
  tests = tests[shard::shards]

  suite = ET.Element('testsuite', name=module.__name__)
  failed, start = 0, time.time()
  print(f'[==========] Running {len(tests)} tests.', flush=True)
  for test, name in tests:
    print(f'[ RUN      ] {name}', flush=True)
    result, t0 = unittest.TestResult(), time.time()
    test.run(result)
    classname, method = name.split('.')
    case = ET.SubElement(suite, 'testcase', classname=classname, name=method,
                         time=f'{time.time() - t0:.3f}')
    for _, trace in result.failures + result.errors:
      lines = [l for l in trace.splitlines() if l and not l[0].isspace()]
      ET.SubElement(case, 'failure', message=(lines[1:] or [trace])[0]
                   ).text = trace
    failed += not result.wasSuccessful()
    print(f'[{"  FAILED  " if not result.wasSuccessful() else "       OK "}] '
          f'{name} ({(time.time() - t0) * 1000:.0f} ms)', flush=True)
  print(f'[==========] {len(tests)} tests ran. {failed} FAILED.', flush=True)

  output = flag('gtest_output', '')
  if output.startswith('xml:'):
    stats = dict(tests=str(len(tests)), failures=str(failed), errors='0',
                 time=f'{time.time() - start:.3f}')
    suite.attrib.update(stats)
    root = ET.Element('testsuites', **stats)
    root.append(suite)
    os.makedirs(os.path.dirname(os.path.abspath(output[4:])), exist_ok=True)
    ET.ElementTree(root).write(output[4:], encoding='utf-8',
                               xml_declaration=True)
  sys.exit(1 if failed else 0)
