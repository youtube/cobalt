"""Tests for establish_coverage_baseline."""

import json
import os
import pathlib
import shutil
import subprocess
import sys
import unittest
from unittest import mock

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from establish_coverage_baseline import CoverageBaselineRunner  # pylint: disable=C0413


class TestCoverageBaselineRunner(unittest.TestCase):
  """Test cases for the CoverageBaselineRunner class."""

  def setUp(self):
    """Set up the test environment."""
    self.platform = 'test-platform'
    self.build_type = 'qa'
    self.mock_cobalt_root = pathlib.Path('/mock/cobalt')

    # Create a temporary directory for test artifacts
    self.test_dir = pathlib.Path('test_tmp')
    self.test_dir.mkdir(exist_ok=True)
    self.original_cwd = os.getcwd()
    os.chdir(self.test_dir)

    with mock.patch('pathlib.Path.resolve', return_value=self.mock_cobalt_root):
      with mock.patch('os.chdir'):
        self.runner = CoverageBaselineRunner(
            self.platform,
            self.build_type,
            cobalt_src_root=str(self.mock_cobalt_root))

    # Mock paths used by the runner
    self.runner.gn_gen_dir = (
        self.test_dir / f'out/{self.platform}_{self.build_type}')
    self.runner.coverage_build_dir = (
        self.test_dir / f'out/coverage_{self.platform}')
    self.runner.raw_lcov_dir = self.test_dir / f'out/lcov_raw_{self.platform}'
    self.runner.merged_lcov_file = (
        self.test_dir / f'out/lcov_merged_{self.platform}.info')
    self.runner.html_report_dir = (
        self.test_dir / f'out/lcov_html_report_{self.platform}')
    self.runner.test_targets_json = (
        self.test_dir / f'cobalt/build/testing/targets/{self.platform}.json')

    # Ensure the output directory for the merged lcov file exists
    self.runner.merged_lcov_file.parent.mkdir(parents=True, exist_ok=True)

  def tearDown(self):
    """Clean up the test environment."""
    os.chdir(self.original_cwd)
    if self.test_dir.exists():
      shutil.rmtree(self.test_dir)

  @mock.patch('subprocess.run')
  def test_run_command(self, mock_run):
    """Test that _run_command executes a command correctly."""
    mock_run.return_value = mock.Mock(stdout='ok', stderr='', returncode=0)
    # pylint: disable=protected-access
    self.runner._run_command(['echo', 'hello'])
    mock_run.assert_called_once_with(['echo', 'hello'],
                                     check=True,
                                     cwd=None,
                                     capture_output=True,
                                     text=True)

  @mock.patch('subprocess.run')
  @mock.patch('shutil.copy')
  def test_setup_gn_args(self, mock_copy, mock_run):
    """Test that GN arguments are set up correctly for coverage."""
    self.runner.gn_gen_dir.mkdir(parents=True, exist_ok=True)
    base_args = self.runner.gn_gen_dir / 'args.gn'
    base_args.touch()

    self.runner.setup_gn_args()

    self.assertTrue(self.runner.coverage_build_dir.exists())
    mock_copy.assert_called_once_with(
        base_args, self.runner.coverage_build_dir / 'args.gn')

    coverage_args_content = (self.runner.coverage_build_dir /
                             'args.gn').read_text()
    self.assertIn('use_clang_coverage=true', coverage_args_content)
    self.assertIn('is_component_build=false', coverage_args_content)
    self.assertIn('is_debug=false', coverage_args_content)

    mock_run.assert_called_with(
        ['gn', 'gen', str(self.runner.coverage_build_dir)],
        check=True,
        cwd=None,
        capture_output=True,
        text=True)

  def test_get_test_targets(self):
    """Test that test targets are correctly extracted from the JSON file."""
    self.runner.test_targets_json.parent.mkdir(parents=True, exist_ok=True)
    test_data = {
        'test_targets': [
            'group1:test1', 'group2:test2', 'group1:test1',
            'group3:benchmark1'
        ]
    }
    with open(self.runner.test_targets_json, 'w', encoding='utf-8') as f:
      json.dump(test_data, f)

    targets = self.runner.get_test_targets()
    self.assertEqual(targets, ['benchmark1', 'test1', 'test2'])



  def test_run_coverage_for_target_binary_not_found(self):
    """Test that coverage run fails if the test binary is not found."""
    result = self.runner.run_coverage_for_target('non_existent_binary')
    self.assertFalse(result)


  def test_android_x86_platform_path(self):
    """Test that android-x86 platform uses the android-arm test targets."""
    with mock.patch('pathlib.Path.resolve',
                    return_value=self.mock_cobalt_root):
      with mock.patch('os.chdir'):
        runner = CoverageBaselineRunner(
            'android-x86',
            self.build_type,
            cobalt_src_root=str(self.mock_cobalt_root))
        self.assertEqual(
            runner.test_targets_json, self.mock_cobalt_root /
            'cobalt/build/testing/targets/android-arm/test_targets.json')

  @mock.patch('shutil.which', return_value='/usr/bin/lcov')
  @mock.patch('subprocess.run')
  def test_merge_lcov_files(self, mock_run, mock_which):
    """Test that lcov files are merged correctly."""
    del mock_which
    self.runner.raw_lcov_dir.mkdir(parents=True, exist_ok=True)
    (self.runner.raw_lcov_dir / 'test1').mkdir()
    (self.runner.raw_lcov_dir / 'test1' / 'coverage.lcov').touch()
    (self.runner.raw_lcov_dir / 'test2').mkdir()
    (self.runner.raw_lcov_dir / 'test2' / 'coverage.lcov').touch()

    self.runner.merge_lcov_files()

    mock_run.assert_called_once()
    # Can't assert the full call because the order of the files is not
    # guaranteed.
    self.assertEqual(mock_run.call_args[0][0][0], 'lcov')

  def test_skip_gn_gen(self):
    """Test that setup_gn_args is not called when skip_gn_gen is True."""
    self.runner.skip_gn_gen = True
    with (mock.patch.object(self.runner, 'setup_gn_args') as mock_setup_gn_args,
          mock.patch.object(self.runner, 'get_test_targets',
                             return_value=[]) as mock_get_test_targets,
          mock.patch.object(self.runner, 'run_all_coverage') as mock_run_all_coverage,
          mock.patch.object(self.runner, 'merge_lcov_files') as mock_merge_lcov_files):
      self.runner.run_baseline(skip_gn_gen=True)
      mock_setup_gn_args.assert_not_called()
      mock_get_test_targets.assert_called_once()
      mock_run_all_coverage.assert_not_called()
      mock_merge_lcov_files.assert_not_called()

  def test_post_process_only(self):
    """Test that only post-processing is run when post_process_only is True."""
    self.runner.post_process_only = True
    with (mock.patch.object(self.runner, 'setup_gn_args') as mock_setup_gn_args,
          mock.patch.object(self.runner, 'get_test_targets') as mock_get_test_targets,
          mock.patch.object(self.runner, 'run_all_coverage') as mock_run_all_coverage,
          mock.patch.object(self.runner, 'merge_lcov_files') as mock_merge_lcov_files):
      self.runner.run_baseline()
      mock_setup_gn_args.assert_not_called()
      mock_get_test_targets.assert_not_called()
      mock_run_all_coverage.assert_not_called()
      mock_merge_lcov_files.assert_called_once()

  @mock.patch('shutil.which', return_value='/usr/bin/genhtml')
  @mock.patch('subprocess.run')
  def test_generate_html_report(self, mock_run, mock_which):
    """Test that the HTML report is generated correctly."""
    del mock_which
    self.runner.merged_lcov_file.touch()

    self.runner.generate_html_report()
    mock_run.assert_called_once_with([
        'genhtml',
        str(self.runner.merged_lcov_file), '--output-directory',
        str(self.runner.html_report_dir)
    ],
                                     check=True,
                                     cwd=None,
                                     capture_output=True,
                                     text=True)
    self.assertTrue(self.runner.html_report_dir.exists())

  @mock.patch('shutil.which', return_value='/usr/bin/lcov')
  @mock.patch('subprocess.run')
  def test_filter_lcov_file(self, mock_run, mock_which):
    """Test that the lcov file is filtered correctly."""
    del mock_which
    self.runner.merged_lcov_file.touch()
    filters = ['/mock/cobalt/src/*', '/mock/cobalt/base/*']
    self.runner.filter_lcov_file(filters)
    mock_run.assert_called_once_with([
        'lcov', '--extract',
        str(self.runner.merged_lcov_file)
    ] + filters + ['--output-file', str(self.runner.merged_lcov_file)],
                                     check=True,
                                     cwd=None,
                                     capture_output=True,
                                     text=True)


if __name__ == '__main__':
  unittest.main()
