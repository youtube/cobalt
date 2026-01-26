#!/usr/bin/env python3
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
"""Script to verify the packaging logic used in GitHub Actions."""

import contextlib
import json
import os
import shutil
import subprocess
import tempfile

import unittest


def run_command(cmd, cwd=None, env=None):
  print(f"Running: {' '.join(cmd)}")
  subprocess.check_call(cmd, cwd=cwd, env=env)


class PackagingTest(unittest.TestCase):

  def test_packaging_workflow(self):
    output_dir = None
    if output_dir:
      output_dir = os.path.abspath(output_dir)
      os.makedirs(output_dir, exist_ok=True)
      temp_dir_ctx = contextlib.nullcontext(output_dir)
    else:
      temp_dir_ctx = tempfile.TemporaryDirectory()

    with temp_dir_ctx as temp_dir:
      repo_root = os.getcwd()
      workspace = os.path.join(temp_dir, 'workspace')
      if os.path.exists(workspace):
        shutil.rmtree(workspace)
      src_dir = os.path.join(workspace, 'cobalt', 'src')
      os.makedirs(src_dir)

      os.makedirs(os.path.join(src_dir, 'cobalt/build'), exist_ok=True)
      shutil.copy(
          os.path.join(repo_root, 'cobalt/build/archive_test_artifacts.py'),
          os.path.join(src_dir, 'cobalt/build/archive_test_artifacts.py'))

      # Copy real files needed for browsertests instead of mocking them
      tools_src = os.path.join(repo_root, 'cobalt/testing/browser_tests/tools')
      tools_dst = os.path.join(src_dir, 'cobalt/testing/browser_tests/tools')
      os.makedirs(tools_dst, exist_ok=True)

      shutil.copy(
          os.path.join(tools_src, 'collect_test_artifacts.py'),
          os.path.join(tools_dst, 'collect_test_artifacts.py'))
      shutil.copy(
          os.path.join(tools_src, 'run_tests.template.py'),
          os.path.join(tools_dst, 'run_tests.template.py'))
      shutil.copy(
          os.path.join(tools_src, 'Dockerfile'),
          os.path.join(tools_dst, 'Dockerfile'))

      # Copy run_browser_tests.py and other referenced scripts
      os.makedirs(
          os.path.join(src_dir, 'cobalt/testing/browser_tests'), exist_ok=True)
      shutil.copy(
          os.path.join(repo_root,
                       'cobalt/testing/browser_tests/run_browser_tests.py'),
          os.path.join(src_dir,
                       'cobalt/testing/browser_tests/run_browser_tests.py'))

      os.makedirs(os.path.join(src_dir, 'testing'), exist_ok=True)
      shutil.copy(
          os.path.join(repo_root, 'testing/xvfb.py'),
          os.path.join(src_dir, 'testing/xvfb.py'))

      # Setup platform/config
      platform = 'android-arm'
      config = 'devel'
      out_dir_rel = f'out/{platform}_{config}'
      out_dir = os.path.join(src_dir, out_dir_rel)
      os.makedirs(out_dir)

      artifacts_dir = os.path.join(workspace, 'artifacts')
      os.makedirs(artifacts_dir)

      # 1. Browsertest target
      exe_name = 'cobalt_browsertests'
      # The script looks for this path if use_android_deps_path is True
      deps_file_path = os.path.join(
          out_dir, 'gen.runtime/cobalt/testing/browser_tests',
          f'{exe_name}__test_runner_script.runtime_deps')
      os.makedirs(os.path.dirname(deps_file_path), exist_ok=True)
      with open(deps_file_path, 'w', encoding='utf-8') as f:
        # Add a mock dependency
        f.write('bin/run_cobalt_browsertests\n')

      # Create the mock runner binary
      os.makedirs(os.path.join(out_dir, 'bin'), exist_ok=True)
      with open(
          os.path.join(out_dir, 'bin/run_cobalt_browsertests'),
          'w',
          encoding='utf-8') as f:
        f.write('mock runner script')

      # 4. test_targets.json
      test_targets_json_rel = f'{out_dir_rel}/test_targets.json'
      test_targets_json_path = os.path.join(src_dir, test_targets_json_rel)
      targets_data = {
          'test_targets': ['cobalt/testing/browser_tests:cobalt_browsertests'],
          'executables': []
      }
      with open(test_targets_json_path, 'w', encoding='utf-8') as f:
        json.dump(targets_data, f)

      print('--- Mimicking GHA Step: Archive Test Dependencies ---')
      python_exe = 'vpython3' if shutil.which('vpython3') else 'python3'

      targets_arg = ','.join(targets_data['test_targets'])

      cmd = [
          python_exe, '-u', './cobalt/build/archive_test_artifacts.py',
          '--source-dir', src_dir, '--out-dir', out_dir, '--destination-dir',
          artifacts_dir, '--targets', targets_arg, '--compression', 'gz',
          '--archive-per-target', '--use-android-deps-path', '--flatten-deps'
      ]
      run_command(cmd, cwd=src_dir)

      print('--- Verifying Explicit Archive ---')
      explicit_archive_path = os.path.join(artifacts_dir,
                                           'explicit_browsertests.tar.gz')
      cmd_explicit = [
          python_exe, '-u', './cobalt/build/archive_test_artifacts.py',
          '--source-dir', src_dir, '--out-dir', out_dir, '--destination-dir',
          artifacts_dir, '--targets', targets_arg, '--compression', 'gz',
          '--archive-per-target', '--use-android-deps-path', '--flatten-deps',
          '--browsertest-archive-path', explicit_archive_path
      ]
      run_command(cmd_explicit, cwd=src_dir)
      if not os.path.exists(explicit_archive_path):
        self.fail(f'explicit archive {explicit_archive_path} not found!')

      print('--- Verifying Archive Contents ---')
      extraction_dir = os.path.join(temp_dir, 'extracted')
      os.makedirs(extraction_dir)
      subprocess.check_call(
          ['tar', '-xf', explicit_archive_path, '-C', extraction_dir])

      expected_files = ['Dockerfile', 'run_tests.py']
      for f in expected_files:
        f_path = os.path.join(extraction_dir, f)
        if os.path.exists(f_path):
          print(f'SUCCESS: Found {f} in archive.')
        else:
          self.fail(f'{f} NOT found in archive!')


if __name__ == '__main__':
  unittest.main()
