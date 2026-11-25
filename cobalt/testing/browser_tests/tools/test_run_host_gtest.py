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
"""Tests for run_host_gtest.py."""

from __future__ import annotations

import ast
import io
import json
import os
from pathlib import Path
import select
import signal
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TOOLS_DIR not in sys.path:
  sys.path.insert(0, _TOOLS_DIR)

# pylint: disable=wrong-import-position
import run_host_gtest
# pylint: enable=wrong-import-position


class TestRunHostGtest(unittest.TestCase):
  """Unit tests for Android host GTest runner and XML conversion."""

  def test_python38_ast_and_attribute_compatibility(self):
    script_path = Path(_TOOLS_DIR, 'run_host_gtest.py')
    source = script_path.read_text(encoding='utf-8')
    tree = ast.parse(
        source, filename='run_host_gtest.py', feature_version=(3, 8))
    has_future_annotations = any(
        isinstance(node, ast.ImportFrom) and node.module == '__future__' and
        any(alias.name == 'annotations'
            for alias in node.names)
        for node in tree.body)
    self.assertTrue(has_future_annotations)
    forbidden_attrs = {'removesuffix', 'removeprefix', 'is_relative_to'}
    used_attrs = {
        node.attr for node in ast.walk(tree) if isinstance(node, ast.Attribute)
    }
    self.assertEqual(used_attrs & forbidden_attrs, set())

  def test_default_src_dir_points_to_checkout_root(self):
    expected_root = os.path.abspath(
        os.path.join(_TOOLS_DIR, '..', '..', '..', '..'))
    self.assertEqual(run_host_gtest._DEFAULT_SRC_DIR, expected_root)  # pylint: disable=protected-access
    self.assertTrue(
        os.path.isfile(
            os.path.join(expected_root, 'cobalt', 'testing', 'browser_tests',
                         'tools', 'run_host_gtest.py')))

  def test_help_before_separator_returns_zero_without_running(self):
    for flag in ('-h', '--help'):
      with mock.patch('sys.stdout', new_callable=io.StringIO) as fake_out:
        rc = run_host_gtest.run_host_gtest([flag])
      self.assertEqual(rc, 0)
      self.assertIn('--src-dir', fake_out.getvalue())

  def test_infer_suite_precedence_and_generic_exclusions(self):
    self.assertEqual(
        run_host_gtest.infer_suite('explicit_suite', '/a/url_unittests.apk',
                                   'net_unittests_testoutput.xml', ()),
        'explicit_suite',
    )
    self.assertEqual(
        run_host_gtest.infer_suite(None, '/a/base_unittests-debug.apk', None,
                                   ()),
        'base_unittests',
    )
    self.assertEqual(
        run_host_gtest.infer_suite(None, '/a/nplb-debug_incremental.apk', None,
                                   ()),
        'nplb',
    )
    self.assertEqual(
        run_host_gtest.infer_suite(
            None, '/a/build.apk', 'boringssl_crypto_tests_shard_1_results.xml',
            ()),
        'boringssl_crypto_tests',
    )
    self.assertEqual(
        run_host_gtest.infer_suite(None, '/a/test.apk', 'out.xml',
                                   ['cobalt_browsertests_loader']),
        'cobalt_browsertests_loader',
    )
    for generic_xml in ('out.xml', 'result.xml', 'results.xml',
                        'test_detail.xml'):
      self.assertIsNone(
          run_host_gtest.infer_suite(None, '/a/build.apk', generic_xml, ()))

  def test_normalize_passthrough_args_joins_filters_and_strips_gtest_output(
      self):
    # pylint: disable=protected-access
    actual = run_host_gtest._normalize_passthrough_args(
        [
            'cobalt_browsertests',
            'url_unittests',
            '--gtest_output=xml:/sdcard/Download/out.xml',
            '--gtest-output',
            'xml:/tmp/other.xml',
            '-f',
            'Foo.*',
            '--gtest_filter',
            '-Disabled.*',
            '--gtest-filter',
            'Bar.*',
            '--test-filter',
            'Baz.*',
            '--isolated-script-test-filter',
            'Qux.*',
            '--single-process-tests',
        ],
        explicit_suite='url_unittests',
    )
    self.assertEqual(
        actual,
        [
            '-f=Foo.*',
            '--gtest_filter=-Disabled.*',
            '--gtest-filter=Bar.*',
            '--test-filter=Baz.*',
            '--isolated-script-test-filter=Qux.*',
            '--single-process-tests',
        ],
    )

  def test_stage_shared_depot_tools_cache_reuse_and_lru_eviction(self):
    with tempfile.TemporaryDirectory() as tmp:
      cache_root = os.path.join(tmp, 'dt_cache')

      def make_dt(version: str) -> str:
        dt_dir = os.path.join(tmp, f'src_{version}')
        os.makedirs(dt_dir, exist_ok=True)
        Path(dt_dir, 'cipd_manifest.versions').write_text(
            version, encoding='utf-8')
        vpy = Path(dt_dir, 'vpython3')
        vpy.write_text('#!/bin/sh\n', encoding='utf-8')
        vpy.chmod(0o755)
        return dt_dir

      no_manifest = os.path.join(tmp, 'plain_dt')
      os.makedirs(no_manifest)
      with mock.patch.dict(os.environ,
                           {'COBALT_DEPOT_TOOLS_CACHE_DIR': cache_root}):
        self.assertEqual(
            run_host_gtest.stage_shared_depot_tools(no_manifest), no_manifest)
        first = run_host_gtest.stage_shared_depot_tools(make_dt('v1'))
        self.assertEqual(first,
                         run_host_gtest.stage_shared_depot_tools(make_dt('v1')))
        os.utime(first, (100.0, 100.0))
        for ver in ('v2', 'v3', 'v4'):
          staged = run_host_gtest.stage_shared_depot_tools(make_dt(ver))
          self.assertNotEqual(staged, first)
        cached = [
            name for name in os.listdir(cache_root)
            if name.startswith('depot_tools_')
        ]
        self.assertEqual(len(cached), 3)
        self.assertNotIn(os.path.basename(first), cached)

  def test_convert_launcher_summary_to_xml_formats_cases_and_guards(self):
    with tempfile.TemporaryDirectory() as tmp:
      summary = os.path.join(tmp, 'summary.json')
      out_xml = os.path.join(tmp, 'res.xml')
      payload = {
          'all_tests': [
              'SuiteA.PassAfterRetry',
              'SuiteA.SkipCase',
              'SuiteA.CrashCase',
              'SuiteB.MissingCase',
          ],
          'per_iteration_data': [{
              'SuiteA.PassAfterRetry': [
                  {
                      'status': 'FAILURE',
                      'elapsed_time_ms': 100,
                      'output_snippet': 'first try',
                  },
                  {
                      'status': 'SUCCESS',
                      'elapsed_time_ms': 250,
                  },
              ],
              'SuiteA.SkipCase': [{
                  'status': 'SKIPPED',
                  'elapsed_time_ms': 10,
              }],
              'SuiteA.CrashCase': [{
                  'status': 'CRASH',
                  'elapsed_time_ms': None,
                  'output_snippet': 'boom\x00\x08\x1b!',
              }],
          }],
      }
      Path(summary).write_text(json.dumps(payload), encoding='utf-8')
      self.assertTrue(
          run_host_gtest.convert_launcher_summary_to_xml(
              summary, out_xml, exit_code=1))
      root = ET.parse(out_xml).getroot()
      self.assertEqual(root.attrib['tests'], '4')
      self.assertEqual(root.attrib['failures'], '2')
      self.assertEqual(root.attrib['errors'], '0')
      self.assertEqual(root.attrib['skipped'], '1')
      self.assertEqual(root.attrib['time'], '0.260')

      crash_fail = root.find(".//testcase[@name='CrashCase']/failure")
      self.assertIsNotNone(crash_fail)
      self.assertEqual(crash_fail.attrib['message'], 'CRASH')
      self.assertEqual(crash_fail.text, 'boom!')

      notrun_fail = root.find(".//testcase[@name='MissingCase']/failure")
      self.assertIsNotNone(notrun_fail)
      self.assertEqual(notrun_fail.attrib['message'], 'NOTRUN')

      skip_el = root.find(".//testcase[@name='SkipCase']/skipped")
      self.assertIsNotNone(skip_el)
      self.assertEqual(skip_el.attrib['message'], 'SKIPPED')

      # Guard checks: non-zero exit with 0 tests or 0 failures, or UNRELIABLE.
      for idx, (code, listed, ran, tags, should_write) in enumerate((
          (1, [], {}, (), False),
          (1, [], {
              'S.P': 'SUCCESS'
          }, (), False),
          (0, [], {}, ('UNRELIABLE_RESULTS',), False),
          (1, [], {}, ('UNRELIABLE_RESULTS',), False),
          (0, [], {}, (), True),
      )):
        s_path = os.path.join(tmp, f's_{idx}.json')
        x_path = os.path.join(tmp, f'x_{idx}.xml')
        Path(s_path).write_text(
            json.dumps({
                'all_tests': listed,
                'per_iteration_data': [{
                    k: [{
                        'status': v,
                        'elapsed_time_ms': 5
                    }] for k, v in ran.items()
                }],
                'global_tags': list(tags),
            }),
            encoding='utf-8',
        )
        self.assertEqual(
            run_host_gtest.convert_launcher_summary_to_xml(
                s_path, x_path, exit_code=code),
            should_write,
        )
        self.assertEqual(os.path.isfile(x_path), should_write)

  def _setup_fake_tree(self, root: str, *, with_browser_deps: bool = False):
    src_dir = os.path.join(root, 'src')
    out_dir = os.path.join(src_dir, 'out', 'android-arm_devel')
    dt_dir = os.path.join(root, 'depot_tools')
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(os.path.join(src_dir, 'build', 'android'), exist_ok=True)
    os.makedirs(dt_dir, exist_ok=True)

    vpy = Path(dt_dir, 'vpython3')
    vpy.write_text(
        '#!/bin/sh\nexport USED_VPYTHON="$0"\nexec "$@"\n', encoding='utf-8')
    vpy.chmod(0o755)
    Path(dt_dir, 'cipd_manifest.versions').write_text('v1\n', encoding='utf-8')

    probe_path = os.path.join(root, 'probe.json')
    summary_str = json.dumps({
        'per_iteration_data': [{
            'S.T': [{
                'status': 'SUCCESS',
                'elapsed_time_ms': 12,
            }]
        }]
    })
    runner_body = '\n'.join([
        '#!/usr/bin/env python3',
        'import json, os, sys',
        'for a in sys.argv[1:]:',
        '  if a.startswith("--json-results-file="):',
        '    p = a.split("=", 1)[1]',
        '    with open(p, "w", encoding="utf-8") as f:',
        f'      f.write({summary_str!r})',
        f'with open({probe_path!r}, "w", encoding="utf-8") as f:',
        '  json.dump({',
        '      "script": sys.argv[0],',
        '      "args": sys.argv[1:],',
        '      "vpython": os.environ.get("USED_VPYTHON"),',
        '      "chrome_src": os.environ.get("CHROME_SRC"),',
        '      "ld": os.environ.get("LD_LIBRARY_PATH"),',
        '  }, f)',
        'print("runner stdout line")',
    ]) + '\n'

    test_runner = Path(src_dir, 'build', 'android', 'test_runner.py')
    test_runner.write_text(runner_body, encoding='utf-8')
    test_runner.chmod(0o755)

    if with_browser_deps:
      wrapper = Path(out_dir, 'bin', 'run_cobalt_browsertests')
      wrapper.parent.mkdir(parents=True, exist_ok=True)
      wrapper.write_text(runner_body, encoding='utf-8')
      wrapper.chmod(0o755)
      Path(out_dir, 'cobalt_browsertests.runtime_deps').write_text(
          'bin/run_cobalt_browsertests\n', encoding='utf-8')

    return src_dir, out_dir, probe_path

  def test_end_to_end_slim_apk_runner_symlinks_and_preserves_post_sep(self):
    with tempfile.TemporaryDirectory() as tmp:
      src_dir, out_dir, probe_path = self._setup_fake_tree(
          tmp, with_browser_deps=True)
      cache_dir = os.path.join(tmp, 'cache')
      output_dir = os.path.join(tmp, 'gen')
      apk_path = os.path.join(tmp, 'apks_in', 'url_unittests-debug.apk')
      os.makedirs(os.path.dirname(apk_path), exist_ok=True)
      Path(apk_path).write_text('apk', encoding='utf-8')

      with mock.patch.dict(os.environ,
                           {'COBALT_DEPOT_TOOLS_CACHE_DIR': cache_dir}):
        rc = run_host_gtest.run_host_gtest([
            '--src-dir',
            src_dir,
            '--output-dir',
            output_dir,
            '--apk',
            apk_path,
            '--result-xml',
            'url_unittests_testoutput.xml',
            '--device-serial',
            'adb-1234',
            '--gtest_output=xml:/sdcard/Download/ignored.xml',
            '--gtest_filter',
            '-Slow*',
            '--disable-test-server',
            '--test-launcher-shard-index=0',
            '--test-launcher-total-shards=2',
            '--',
            '--extra-flag',
            '-h',
        ])

      self.assertEqual(rc, 0)
      staged_apk = os.path.join(out_dir, 'url_unittests_apk',
                                'url_unittests-debug.apk')
      self.assertTrue(os.path.islink(staged_apk))
      self.assertEqual(os.path.realpath(staged_apk), os.path.realpath(apk_path))

      probe = json.loads(Path(probe_path).read_text(encoding='utf-8'))
      self.assertEqual(
          os.path.realpath(probe['script']),
          os.path.realpath(
              os.path.join(src_dir, 'build/android/test_runner.py')))
      self.assertTrue(probe['vpython'].startswith(cache_dir))
      self.assertEqual(probe['chrome_src'], os.path.abspath(src_dir))
      self.assertIn(os.path.join(out_dir, 'starboard'), probe['ld'])

      summary_json = os.path.join(output_dir, 'test_launcher_summary.json')
      self.assertEqual(
          probe['args'],
          [
              'gtest',
              '--output-directory',
              out_dir,
              '--suite',
              'url_unittests',
              '--use-existing-test-data',
              '--shard-timeout=600',
              '--gtest_filter=-Slow*',
              '--disable-test-server',
              '--test-launcher-shard-index=0',
              '--test-launcher-total-shards=2',
              f'--json-results-file={summary_json}',
              '-d',
              'adb-1234',
              '--',
              '--extra-flag',
              '-h',
          ],
      )
      self.assertIn(
          'runner stdout line',
          Path(output_dir, 'test_results.txt').read_text(encoding='utf-8'))
      xml_root = ET.parse(
          os.path.join(output_dir, 'url_unittests_testoutput.xml')).getroot()
      self.assertEqual(xml_root.attrib['tests'], '1')

  def test_local_wrapper_fallback_and_legacy_apks_symlink(self):
    with tempfile.TemporaryDirectory() as tmp:
      src_dir, out_dir, probe_path = self._setup_fake_tree(
          tmp, with_browser_deps=True)
      cache_dir = os.path.join(tmp, 'cache')
      output_dir = os.path.join(tmp, 'gen')

      with mock.patch.dict(os.environ,
                           {'COBALT_DEPOT_TOOLS_CACHE_DIR': cache_dir}):
        rc = run_host_gtest.run_host_gtest([
            '--src-dir', src_dir, '--output-dir', output_dir,
            'cobalt_browsertests', '-v'
        ])
      self.assertEqual(rc, 0)
      probe = json.loads(Path(probe_path).read_text(encoding='utf-8'))
      self.assertTrue(probe['script'].endswith('bin/run_cobalt_browsertests'))

      # Without --apk for a non-browser suite, symlink apks/<suite>.apk.
      legacy_apk = Path(out_dir, 'apks', 'net_unittests.apk')
      legacy_apk.parent.mkdir(parents=True, exist_ok=True)
      legacy_apk.write_text('apk', encoding='utf-8')
      with mock.patch.dict(os.environ,
                           {'COBALT_DEPOT_TOOLS_CACHE_DIR': cache_dir}):
        rc2 = run_host_gtest.run_host_gtest([
            '--src-dir', src_dir, '--output-dir', output_dir, '--suite',
            'net_unittests', '-t', '120'
        ])
      self.assertEqual(rc2, 0)
      self.assertEqual(
          os.path.realpath(
              os.path.join(out_dir, 'net_unittests_apk',
                           'net_unittests-debug.apk')),
          os.path.realpath(legacy_apk),
      )
      probe2 = json.loads(Path(probe_path).read_text(encoding='utf-8'))
      self.assertNotIn('--shard-timeout=600', probe2['args'])

  def test_sigterm_kills_child_process_group_and_converts_partial_summary(self):
    if os.name != 'posix':
      return
    with tempfile.TemporaryDirectory() as tmp:
      src_dir, _, _ = self._setup_fake_tree(tmp)
      cache_dir = os.path.join(tmp, 'cache')
      output_dir = os.path.join(tmp, 'gen')
      apk_path = os.path.join(tmp, 'url_unittests-debug.apk')
      Path(apk_path).write_text('apk', encoding='utf-8')

      fifo_path = os.path.join(tmp, 'child.fifo')
      os.mkfifo(fifo_path)
      read_fd = os.open(fifo_path, os.O_RDONLY | os.O_NONBLOCK)
      self.addCleanup(os.close, read_fd)

      summary_payload = json.dumps({
          'all_tests': ['S.Done', 'S.Pending'],
          'per_iteration_data': [{
              'S.Done': [{
                  'status': 'SUCCESS',
                  'elapsed_time_ms': 10
              }]
          }],
      })
      pid = os.getpid()
      runner = Path(src_dir, 'build', 'android', 'test_runner.py')
      runner.write_text(
          '\n'.join([
              '#!/bin/sh',
              'set -eu',
              'for arg in "$@"; do',
              '  case "$arg" in',
              '    --json-results-file=*)',
              '      p="${arg#--json-results-file=}"',
              f"      printf '%s\\n' '{summary_payload}' > \"$p\"",
              '      ;;',
              '  esac',
              'done',
              f'/bin/sh -ceu \'exec 3>"{fifo_path}"; '
              f'kill -TERM {pid}; exec sleep 60\' &',
              'wait "$!"',
          ]) + '\n',
          encoding='utf-8',
      )
      runner.chmod(0o755)

      with mock.patch.dict(os.environ,
                           {'COBALT_DEPOT_TOOLS_CACHE_DIR': cache_dir}):
        rc = run_host_gtest.run_host_gtest([
            '--src-dir',
            src_dir,
            '--output-dir',
            output_dir,
            '--apk',
            apk_path,
            '--result-xml',
            'url_unittests_testoutput.xml',
        ])

      self.assertEqual(rc, 128 + signal.SIGTERM)
      readable, _, _ = select.select([read_fd], [], [], 5.0)
      self.assertEqual(readable, [read_fd])
      self.assertEqual(os.read(read_fd, 1), b'')
      xml_root = ET.parse(
          os.path.join(output_dir, 'url_unittests_testoutput.xml')).getroot()
      self.assertEqual(xml_root.attrib['tests'], '2')
      self.assertEqual(xml_root.attrib['failures'], '1')


if __name__ == '__main__':
  unittest.main()
