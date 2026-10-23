#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for cobalt.build.test_targets."""

import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

# Ensure cobalt package is discoverable when running directly from repo root.
_SRC_DIR = Path(__file__).resolve().parent.parent.parent
if str(_SRC_DIR) not in sys.path:
  sys.path.insert(0, str(_SRC_DIR))

# pylint: disable=wrong-import-position
from cobalt.build import test_targets

# Sample static test manifest entries.
_SAMPLE_MANIFEST = [
    {
        'disabled': False,
        'description': 'Metadata header required by downstream runners',
    },
    {
        'target': '//base:base_unittests',
        'executable': 'base_unittests',
    },
    {
        'target': '//cobalt/black_box_tests:black_box_tests',
        'executable': 'black_box_tests',
    },
    {
        'target': '//starboard/nplb:nplb',
        'executable': 'nplb',
    },
    {
        'target': '//net:net_unittests',
        'executable': 'net_unittests',
    },
]


class TestTargetsTest(unittest.TestCase):

  def setUp(self):
    # pylint: disable=consider-using-with
    self.tmp_dir = tempfile.TemporaryDirectory()
    self.manifest_file = Path(self.tmp_dir.name) / 'test_targets.json'
    self.manifest_file.write_text(
        json.dumps(_SAMPLE_MANIFEST), encoding='utf-8')
    self.out_dir = Path(self.tmp_dir.name) / 'out'
    self.out_dir.mkdir(parents=True, exist_ok=True)

  def tearDown(self):
    self.tmp_dir.cleanup()

  def test_empty_diff_fails_open(self):
    """Empty diffs fail open without invoking gn."""

    def failing_gn(args):
      raise AssertionError(f'GN should not be invoked on empty diff: {args}')

    skip, manifest = test_targets.run_analysis(
        out_dir=self.out_dir,
        changed_files=[],
        targets=['cobalt:gn_all'],
        test_manifest_path=self.manifest_file,
        run_gn=failing_gn,
    )
    self.assertFalse(skip)
    tests = [t for t in manifest if 'target' in t]
    self.assertEqual(len(tests), 4)

  def test_infra_prefix_fails_open(self):
    """Infrastructure file changes fail open immediately."""

    def failing_gn(args):
      raise AssertionError(f'GN should not be invoked for infra: {args}')

    infra_files = [
        '.github/workflows/main.yaml',
        'cobalt/testing/filters/test.filters',
        'starboard/tools/run_test.py',
        'tools/gn',
        '.vpython3',
        'cobalt/build/gn.py',
        'DEPS',
        '.gn',
    ]
    for f in infra_files:
      with self.subTest(infra_file=f):
        skip, manifest = test_targets.run_analysis(
            out_dir=self.out_dir,
            changed_files=[f],
            targets=['cobalt:gn_all'],
            test_manifest_path=self.manifest_file,
            run_gn=failing_gn,
        )
        self.assertFalse(skip)
        tests = [t for t in manifest if 'target' in t]
        self.assertEqual(len(tests), 4)

  def test_no_dependency_skips_platform(self):
    """GN status 'No dependency' causes platform skip and empty test list."""

    def mock_run_gn(args):
      if args[0] == 'ls':
        return ['//cobalt:gn_all', '//base:base_unittests']
      return {'status': 'No dependency'}

    skip, manifest = test_targets.run_analysis(
        out_dir=self.out_dir,
        changed_files=['cobalt/browser/browser.cc'],
        targets=['cobalt:gn_all'],
        test_manifest_path=self.manifest_file,
        run_gn=mock_run_gn,
    )
    self.assertTrue(skip)
    self.assertEqual(len(manifest), 1)
    self.assertEqual(manifest[0]['disabled'], False)

  def test_found_dependency_prunes_subset(self):
    """GN status 'Found dependency' prunes manifest to affected subset."""

    def mock_run_gn(args):
      if args[0] == 'ls':
        return ['//cobalt:gn_all', '//base:base_unittests']
      return {
          'status': 'Found dependency',
          'compile_targets': ['//cobalt:gn_all'],
          'test_targets': ['//base:base_unittests'],
      }

    skip, manifest = test_targets.run_analysis(
        out_dir=self.out_dir,
        changed_files=['base/strings/string_util.cc'],
        targets=['cobalt:gn_all'],
        test_manifest_path=self.manifest_file,
        run_gn=mock_run_gn,
    )
    self.assertFalse(skip)
    tests = [t for t in manifest if 'target' in t]
    self.assertEqual(len(tests), 1)
    self.assertEqual(tests[0]['target'], '//base:base_unittests')

  def test_found_dependency_empty_targets_skips(self):
    """Found dependency with 0 compile and test targets evaluates as skip."""

    def mock_run_gn(args):
      if args[0] == 'ls':
        return ['//cobalt:gn_all']
      return {
          'status': 'Found dependency',
          'compile_targets': [],
          'test_targets': [],
      }

    skip, manifest = test_targets.run_analysis(
        out_dir=self.out_dir,
        changed_files=['docs/README.md'],
        targets=['cobalt:gn_all'],
        test_manifest_path=self.manifest_file,
        run_gn=mock_run_gn,
    )
    self.assertTrue(skip)
    self.assertEqual(len(manifest), 1)
    self.assertNotIn('target', manifest[0])

  def test_found_dependency_all_runs_everything(self):
    """GN status 'Found dependency (all)' runs all platform tests."""

    def mock_run_gn(args):
      if args[0] == 'ls':
        return ['//cobalt:gn_all']
      return {'status': 'Found dependency (all)'}

    skip, manifest = test_targets.run_analysis(
        out_dir=self.out_dir,
        changed_files=['cobalt/root/something.cc'],
        targets=['cobalt:gn_all'],
        test_manifest_path=self.manifest_file,
        run_gn=mock_run_gn,
    )
    self.assertFalse(skip)
    tests = [t for t in manifest if 'target' in t]
    self.assertEqual(len(tests), 4)

  def test_label_normalization_and_toolchains(self):
    """Verifies label canonicalization, implicit shorthands, and toolchains."""
    # pylint: disable=protected-access
    self.assertEqual(test_targets._canonical_label('//base'), '//base:base')
    self.assertEqual(
        test_targets._canonical_label('//cobalt/browser'),
        '//cobalt/browser:browser')
    self.assertEqual(
        test_targets._canonical_label('//base:base_unittests(//toolchain:x)'),
        '//base:base_unittests')

    all_labels = {
        '//cobalt:gn_all(//build/toolchain/linux:clang_x64)',
        '//starboard/nplb:nplb(//starboard/build/toolchain:modular)',
        '//starboard:starboard_test',
        '//base:base_unittests',
    }
    resolved_shorthand = test_targets._resolve_labels(all_labels,
                                                      ['//starboard/nplb'])
    self.assertEqual(
        resolved_shorthand,
        ['//starboard/nplb:nplb(//starboard/build/toolchain:modular)'])

    def mock_run_gn(args):
      if args[0] == 'ls':
        return list(all_labels)
      return {
          'status': 'Found dependency',
          'compile_targets': [
              '//cobalt:gn_all(//build/toolchain/linux:clang_x64)'
          ],
          'test_targets': ['//base:base_unittests'],
      }

    skip, manifest = test_targets.run_analysis(
        out_dir=self.out_dir,
        changed_files=['base/base.cc'],
        targets=['cobalt:gn_all'],
        test_manifest_path=self.manifest_file,
        run_gn=mock_run_gn,
    )
    self.assertFalse(skip)
    tests = [t for t in manifest if 'target' in t]
    self.assertEqual(len(tests), 1)
    self.assertEqual(tests[0]['target'], '//base:base_unittests')

  def test_gn_error_and_timeout_fail_open(self):
    """Subprocess errors and timeouts during gn analyze safely fail open."""

    def error_gn(_):
      raise subprocess.CalledProcessError(1, ['gn', 'analyze'])

    def timeout_gn(_):
      raise subprocess.TimeoutExpired(['gn', 'analyze'], 600)

    for failing_gn in (error_gn, timeout_gn):
      with self.subTest(gn_func=failing_gn.__name__):
        with mock.patch('sys.stderr', new_callable=io.StringIO):
          skip, manifest = test_targets.run_analysis(
              out_dir=self.out_dir,
              changed_files=['cobalt/browser/browser.cc'],
              targets=['cobalt:gn_all'],
              test_manifest_path=self.manifest_file,
              run_gn=failing_gn,
          )
        self.assertFalse(skip)
        tests = [t for t in manifest if 'target' in t]
        self.assertEqual(len(tests), 4)

  def test_precomputed_deps_file(self):
    """Precomputed deps JSON file filtering for platforms like tvOS."""
    deps_file = Path(self.tmp_dir.name) / 'deps.json'
    deps_file.write_text(
        json.dumps({
            'target1': ['//cobalt/browser/main.cc', 'starboard/common/log.cc'],
        }),
        encoding='utf-8',
    )
    # Unrelated change -> skip
    skip, _ = test_targets.run_analysis(
        out_dir=None,
        changed_files=['starboard/android/audio.cc'],
        targets=[],
        test_manifest_path=self.manifest_file,
        deps_file=deps_file,
    )
    self.assertTrue(skip)

    # Related change -> do not skip
    skip, manifest = test_targets.run_analysis(
        out_dir=None,
        changed_files=['cobalt/browser/main.cc'],
        targets=[],
        test_manifest_path=self.manifest_file,
        deps_file=deps_file,
    )
    self.assertFalse(skip)
    tests = [t for t in manifest if 'target' in t]
    self.assertEqual(len(tests), 4)

    # Malformed/empty deps file fails open
    bad_deps = Path(self.tmp_dir.name) / 'bad_deps.json'
    bad_deps.write_text(json.dumps({}), encoding='utf-8')
    with mock.patch('sys.stderr', new_callable=io.StringIO):
      skip, _ = test_targets.run_analysis(
          out_dir=None,
          changed_files=['cobalt/browser/main.cc'],
          targets=[],
          test_manifest_path=self.manifest_file,
          deps_file=bad_deps,
      )
    self.assertFalse(skip)

  def test_manifest_and_filter_file_generation(self):
    """Generates formatted JSON manifest, .filter file, and github output."""
    filter_content = test_targets.format_filter_file(
        ['//base:base_unittests', 'cobalt/dom:dom_unittests'])
    expected_filter = ('# Generated by cobalt/build/test_targets.py\n'
                       'base:base_unittests\n'
                       'cobalt/dom:dom_unittests\n')
    self.assertEqual(filter_content, expected_filter)

    cf_file = Path(self.tmp_dir.name) / 'changed.txt'
    cf_file.write_text('base/string.cc\n', encoding='utf-8')
    out_manifest = Path(self.tmp_dir.name) / 'out_manifest.json'
    out_filter = Path(self.tmp_dir.name) / 'out_filter.filter'
    out_gh = Path(self.tmp_dir.name) / 'gh_out.txt'

    with mock.patch('cobalt.build.test_targets.run_analysis') as mock_run:
      mock_run.return_value = (False, [
          _SAMPLE_MANIFEST[0],
          _SAMPLE_MANIFEST[1],
      ])
      rc = test_targets.main([
          '--out-dir',
          str(self.out_dir),
          '--changed-files',
          str(cf_file),
          '--test-targets-json',
          str(self.manifest_file),
          '--output',
          str(out_manifest),
          '--filter-output',
          str(out_filter),
          '--github-output',
          str(out_gh),
      ])
      self.assertEqual(rc, 0)

    self.assertTrue(out_manifest.exists())
    written_manifest = json.loads(out_manifest.read_text(encoding='utf-8'))
    self.assertEqual(len(written_manifest), 2)
    self.assertEqual(written_manifest[1]['target'], '//base:base_unittests')

    self.assertTrue(out_filter.exists())
    self.assertIn('base:base_unittests', out_filter.read_text(encoding='utf-8'))

    self.assertTrue(out_gh.exists())
    gh_content = out_gh.read_text(encoding='utf-8')
    self.assertIn('skip=false\n', gh_content)
    self.assertIn('test_targets_count=1\n', gh_content)
    self.assertIn('test_targets_json=["//base:base_unittests"]\n', gh_content)

  def test_load_manifest_malformed(self):
    bad_file = Path(self.tmp_dir.name) / 'bad.json'
    bad_file.write_text('{not valid json', encoding='utf-8')
    header, tests = test_targets.load_manifest(bad_file)
    self.assertEqual(header, {'disabled': False})
    self.assertEqual(tests, [])

    non_list_file = Path(self.tmp_dir.name) / 'non_list.json'
    non_list_file.write_text('{"foo": "bar"}', encoding='utf-8')
    header, tests = test_targets.load_manifest(non_list_file)
    self.assertEqual(header, {'disabled': False})
    self.assertEqual(tests, [])


if __name__ == '__main__':
  unittest.main()
