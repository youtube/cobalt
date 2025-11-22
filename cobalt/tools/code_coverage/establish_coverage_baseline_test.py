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

  @mock.patch('subprocess.run')
  def test_build_tests(self, mock_run):
    """Test that the build command is constructed and run correctly."""
    targets = ['test1', 'test2']
    self.runner.build_tests(targets)
    mock_run.assert_called_once_with(
        ['autoninja', '-C',
         str(self.runner.coverage_build_dir)] + targets,
        check=True,
        cwd=None,
        capture_output=True,
        text=True)

  @mock.patch('subprocess.run')
  def test_run_coverage_for_target_success(self, mock_run):
    """Test successful coverage run for a single target."""
    test_name = 'my_test'
    self.runner.coverage_build_dir.mkdir(parents=True, exist_ok=True)
    (self.runner.coverage_build_dir / test_name).touch()

    # Simulate lcov.info creation
    def side_effect(*args, **kwargs):
      del kwargs
      cmd = args[0]
      if 'tools/code_coverage/coverage.py' in cmd:
        test_lcov_out_dir = pathlib.Path(cmd[cmd.index('-o') + 1])
        (test_lcov_out_dir / 'lcov.info').touch()
      return mock.Mock(stdout='ok', stderr='', returncode=0)

    mock_run.side_effect = side_effect

    result = self.runner.run_coverage_for_target(test_name)
    self.assertTrue(result)
    self.assertTrue(
        (self.runner.raw_lcov_dir / test_name / 'lcov.info').exists())

  @mock.patch('subprocess.run')
  def test_run_coverage_for_target_fail(self, mock_run):
    """Test failed coverage run for a single target."""
    test_name = 'fail_test'
    self.runner.coverage_build_dir.mkdir(parents=True, exist_ok=True)
    (self.runner.coverage_build_dir / test_name).touch()

    mock_run.side_effect = subprocess.CalledProcessError(1, ['cmd'])
    result = self.runner.run_coverage_for_target(test_name)
    self.assertFalse(result)

  @mock.patch('shutil.which', return_value='/usr/bin/lcov')
  @mock.patch('subprocess.run')
  def test_merge_lcov_files(self, mock_run, mock_which):
    """Test that lcov files are merged correctly."""
    del mock_which
    self.runner.raw_lcov_dir.mkdir(parents=True, exist_ok=True)
    (self.runner.raw_lcov_dir / 'test1').mkdir()
    (self.runner.raw_lcov_dir / 'test1' / 'lcov.info').touch()
    (self.runner.raw_lcov_dir / 'test2').mkdir()
    (self.runner.raw_lcov_dir / 'test2' / 'lcov.info').touch()

    self.runner.merge_lcov_files()

    expected_calls = [
        mock.call([
            'lcov', '-a',
            str(self.runner.raw_lcov_dir / 'test1' / 'lcov.info'), '-o',
            str(self.runner.merged_lcov_file)
        ],
                  check=True,
                  cwd=None,
                  capture_output=True,
                  text=True),
        mock.call([
            'lcov', '-a',
            str(self.runner.raw_lcov_dir / 'test2' / 'lcov.info'), '-o',
            str(self.runner.merged_lcov_file)
        ],
                  check=True,
                  cwd=None,
                  capture_output=True,
                  text=True),
    ]
    mock_run.assert_has_calls(expected_calls, any_order=True)

  @mock.patch('shutil.which', return_value='/usr/bin/genhtml')
  @mock.patch('subprocess.run')
  def test_generate_html_report(self, mock_run, mock_which):
    """Test that the HTML report is generated correctly."""
    del mock_which
    self.runner.merged_lcov_file.touch()

    def create_report_dir(*args, **kwargs):
      del args, kwargs
      self.runner.html_report_dir.mkdir()
      return mock.Mock(stdout='ok', stderr='', returncode=0)

    mock_run.side_effect = create_report_dir
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

  def test_get_test_targets_empty(self):
    """Test that an empty list is returned for empty test targets."""
    self.runner.test_targets_json.parent.mkdir(parents=True, exist_ok=True)
    with open(self.runner.test_targets_json, 'w', encoding='utf-8') as f:
      json.dump({'test_targets': []}, f)
    targets = self.runner.get_test_targets()
    self.assertEqual(targets, [])

  @mock.patch('subprocess.run')
  def test_build_tests_no_targets(self, mock_run):
    """Test that no build command is run for an empty list of targets."""
    self.runner.build_tests([])
    mock_run.assert_not_called()

  def test_run_coverage_for_target_binary_not_found(self):
    """Test that coverage run fails if the test binary is not found."""
    result = self.runner.run_coverage_for_target('non_existent_binary')
    self.assertFalse(result)

  def test_merge_lcov_files_no_lcov_files(self):
    """Test that merge fails gracefully if no lcov files are found."""
    self.runner.raw_lcov_dir.mkdir(parents=True, exist_ok=True)
    result = self.runner.merge_lcov_files()
    self.assertFalse(result)

  @mock.patch('shutil.which', return_value=None)
  def test_merge_lcov_files_lcov_not_found(self, mock_which):
    """Test that an EnvironmentError is raised if lcov is not found."""
    del mock_which
    self.runner.raw_lcov_dir.mkdir(parents=True, exist_ok=True)
    (self.runner.raw_lcov_dir / 'test1').mkdir()
    (self.runner.raw_lcov_dir / 'test1' / 'lcov.info').touch()
    with self.assertRaises(EnvironmentError):
      self.runner.merge_lcov_files()

  @mock.patch('shutil.which', return_value=None)
  def test_generate_html_report_genhtml_not_found(self, mock_which):
    """Test that an EnvironmentError is raised if genhtml is not found."""
    del mock_which
    self.runner.merged_lcov_file.touch()
    with self.assertRaises(EnvironmentError):
      self.runner.generate_html_report()

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


if __name__ == '__main__':
  unittest.main()
