"""Automates the process of generating a code coverage baseline for Cobalt."""

import argparse
import glob
import json
import os
import pathlib
import shutil
import subprocess
import sys
from typing import Sequence


class CoverageBaselineRunner:
  """Automates generating a code coverage baseline for Cobalt."""

  def __init__(self,
               platform: str,
               build_type: str = 'qa',
               cobalt_src_root: str = '.',
               verbose: bool = False,
               skip_gn_gen: bool = False,
               test_target: str = None,
               post_process_only: bool = False,
               test_filters_dir: str = None,
               include_skipped_tests: bool = False):
    """Initializes the CoverageBaselineRunner.

    Args:
      platform: The target platform, e.g., 'linux-x64'.
      build_type: The build type, e.g., 'qa', 'devel'.
      cobalt_src_root: The path to the root of the Cobalt source code.
      verbose: Whether to print verbose output.
      skip_gn_gen: Whether to skip the 'gn gen' step.
      test_target: The single test target to run.
      post_process_only: Whether to only run the post-processing steps.
      test_filters_dir: Directory containing test filter JSON files.
      include_skipped_tests: Whether to include tests skipped by filters.
    """
    self.platform = platform
    self.build_type = build_type
    self.cobalt_src_root = pathlib.Path(cobalt_src_root).resolve()
    self.verbose = verbose
    self.skip_gn_gen = skip_gn_gen
    self.test_target = test_target
    self.post_process_only = post_process_only
    self.test_filters_dir = test_filters_dir
    self.include_skipped_tests = include_skipped_tests

    self.gn_gen_dir = (
        self.cobalt_src_root / f'out/{self.platform}_{self.build_type}')
    self.coverage_build_dir = (
        self.cobalt_src_root / f'out/coverage_{self.platform}')
    self.llvm_bin_dir = (
        self.cobalt_src_root / 'third_party/llvm-build/Release+Asserts/bin')
    self.raw_lcov_dir = self.cobalt_src_root / f'out/lcov_raw_{self.platform}'
    self.merged_lcov_file = self.raw_lcov_dir / 'merged.lcov'
    self.html_report_dir = self.cobalt_src_root / f'out/lcov_html_report_{self.platform}'
    base_target_path = self.cobalt_src_root / 'cobalt/build/testing/targets'
    platform_target_dir = self.platform
    if self.platform == 'android-x86':
      platform_target_dir = 'android-arm'

    self.test_targets_json = (
        base_target_path / platform_target_dir / 'test_targets.json')

    if self.verbose:
      print('--- Runner Initialized with the following paths ---')
      print(f'Cobalt src root: {self.cobalt_src_root}')
      print(f'GN gen dir: {self.gn_gen_dir}')
      print(f'Coverage build dir: {self.coverage_build_dir}')
      print(f'LLVM bin dir: {self.llvm_bin_dir}')
      print(f'Raw LCOV dir: {self.raw_lcov_dir}')
      print(f'Merged LCOV file: {self.merged_lcov_file}')
      print(f'HTML report dir: {self.html_report_dir}')
      print(f'Test targets JSON: {self.test_targets_json}')
      print('----------------------------------------------------')

    os.chdir(self.cobalt_src_root)

  def _run_command(self, 
                   command: Sequence[str], 
                   cwd: str | None = None, 
                   check: bool = True) -> subprocess.CompletedProcess:
    """Runs a shell command and prints its output.

    Args:
      command: The command to run as a sequence of strings.
      cwd: The working directory to run the command in.
      check: If True, raises an exception if the command fails.

    Returns:
      The CompletedProcess object.
    """
    print(f'--- Running command: {" ".join(command)} ---', flush=True)
    try:
      result = subprocess.run(
          command, check=check, cwd=cwd, capture_output=True, text=True)
      if self.verbose:
        print(result.stdout, flush=True)
        if result.stderr:
          print(result.stderr, file=sys.stderr, flush=True)
      return result
    except subprocess.CalledProcessError as e:
      print(f'Command failed: {e}', file=sys.stderr, flush=True)
      print(e.stdout, file=sys.stderr, flush=True)
      print(e.stderr, file=sys.stderr, flush=True)
      if check:
        raise

  def setup_gn_args(self) -> None:
    """Sets up the GN build directory with coverage flags."""
    print(f'--- Setting up GN for coverage in {self.coverage_build_dir} ---')
    if self.coverage_build_dir.exists():
      shutil.rmtree(self.coverage_build_dir)
    self.coverage_build_dir.mkdir(parents=True)

    if not self.gn_gen_dir.exists():
      print(f'Base GN directory {self.gn_gen_dir} not found. Generating...')
      self._run_command([
          'python3', 'cobalt/build/gn.py', '-p', self.platform, '-c',
          self.build_type
      ])

    base_args_file = self.gn_gen_dir / 'args.gn'
    coverage_args_file = self.coverage_build_dir / 'args.gn'

    if not base_args_file.exists():
      raise FileNotFoundError(f'Base args file not found: {base_args_file}')

    shutil.copy(base_args_file, coverage_args_file)

    with open(coverage_args_file, 'a', encoding='utf-8') as f:
      f.write('\n# Coverage args\n')
      f.write('use_clang_coverage=true\n')
      f.write('is_component_build=false\n')
      f.write('is_debug=false\n')

    self._run_command(['gn', 'gen', str(self.coverage_build_dir)])
    print(f'GN args.gn prepared in {self.coverage_build_dir}')

  def get_test_targets(self) -> list[str]:
    """Reads and filters unit test targets from the platform JSON file.

    Returns:
      A list of unit test target names.
    """
    if self.test_target:
      return [self.test_target]

    print(f'--- Finding Unit Test Targets for {self.platform} ---')
    if not self.test_targets_json.exists():
      raise FileNotFoundError(
          f'Test targets file not found: {self.test_targets_json}')

    with open(self.test_targets_json, 'r', encoding='utf-8') as f:
      test_data = json.load(f)

    targets = sorted(
        list(
            set(
                target.split(':')[1]
                for target in test_data.get('test_targets', []))))

    if not targets:
      print(f'No test targets of type \'test\' found in '
            f'{self.test_targets_json}')  # pylint: disable=W1405
    else:
      print(f'Found test targets: {", ".join(targets)}')
    return targets

  def run_coverage_for_target(self, test_name: str) -> bool:
    """Runs the coverage tool for a single test target.

    Args:
      test_name: The name of the test target to run.

    Returns:
      True if coverage was run successfully, False otherwise.
    """
    print(f'--- Running coverage for: {test_name} ---')
    test_lcov_out_dir = self.raw_lcov_dir / test_name
    test_lcov_out_dir.mkdir(parents=True, exist_ok=True)

    test_binary_path = self.coverage_build_dir / test_name
    gtest_filter_arg = None

    # Apply test filters unless explicitly told to include skipped tests.
    if not self.include_skipped_tests and self.test_filters_dir:
      filter_path = (pathlib.Path(self.test_filters_dir) /
                     f'{test_name}_filter.json')
      if filter_path.exists():
        print(f'Applying test filter: {filter_path}')
        with open(filter_path, 'r', encoding='utf-8') as f:
          filter_data = json.load(f)
        
        failing_tests = filter_data.get('failing_tests', [])
        if failing_tests == ['*']:
          print(f'Skipping target {test_name} as per filter configuration.')
          return True  # Skipped is not a failure.
        
        if failing_tests:
          gtest_filter_arg = '--gtest_filter=-' + ':'.join(failing_tests)

    cmd = [
        'time',
        '-v',
        'vpython3',
        'tools/code_coverage/coverage.py',
        test_name,
        '-b',
        str(self.coverage_build_dir),
        '-o',
        str(test_lcov_out_dir),
        '-c',
        str(test_binary_path),
        '--coverage-tools-dir',
        str(self.llvm_bin_dir),
        '--format=lcov',
    ]
    if gtest_filter_arg:
      cmd.append(gtest_filter_arg)
      
    print(f'Coverage.py command: {" ".join(cmd)}', flush=True)
    try:
      self._run_command(cmd)
      lcov_file = test_lcov_out_dir / 'linux' / 'coverage.lcov'
      if not lcov_file.exists():
        print(
            f'WARNING: coverage.lcov not found for {test_name} in '
            f'{test_lcov_out_dir / "linux"}',
            file=sys.stderr)
        return False
      return True
    except subprocess.CalledProcessError:
      return False

  def run_all_coverage(self, targets: Sequence[str]) -> tuple[list[str], list[str]]:
    """Runs coverage for all specified test targets serially.

    Args:
      targets: A list of test target names to run.

    Returns:
      A tuple containing two lists: successful targets and failed targets.
    """
    print('--- Running tests and generating coverage data (serially) ---')
    if self.raw_lcov_dir.exists():
      shutil.rmtree(self.raw_lcov_dir)
    self.raw_lcov_dir.mkdir(parents=True)

    successful_targets = []
    failed_targets = []
    for target in targets:
      if self.run_coverage_for_target(target):
        print(f'--- {target}: PASSED ---')
        successful_targets.append(target)
      else:
        print(f'--- {target}: FAILED ---', file=sys.stderr)
        failed_targets.append(target)
    return successful_targets, failed_targets


  def merge_lcov_files(self) -> None:
    """Merges all coverage.lcov files into a single file."""
    print(f'--- Merging LCOV files into {self.merged_lcov_file} ---')
    lcov_files = glob.glob(
        str(self.raw_lcov_dir / '**' / 'coverage.lcov'), recursive=True)
    if not lcov_files:
      print('No .lcov files found to merge.', file=sys.stderr)
      return

    merge_cmd = ['lcov', '-o', str(self.merged_lcov_file)]
    for file in lcov_files:
      merge_cmd.extend(['-a', file])
    self._run_command(merge_cmd)
    print(f'Successfully merged {len(lcov_files)} lcov files.')

  def filter_lcov_file(self, filters: Sequence[str]) -> None:
    """Filters the merged lcov file to include only specified directories.

    Args:
      filters: A list of directory patterns to include in the report.
    """
    print(f'--- Filtering LCOV file with: {filters} ---')
    if not shutil.which('lcov'):
      raise EnvironmentError('lcov not found. Please install lcov.')

    extract_cmd = [
        'lcov', '--extract',
        str(self.merged_lcov_file)
    ] + list(filters) + ['--output-file', str(self.merged_lcov_file)]
    self._run_command(extract_cmd)
    print('Successfully filtered lcov file.')

  def generate_html_report(self) -> None:
    """Generates an HTML report from the merged lcov file."""
    print(f'--- Generating HTML report in {self.html_report_dir} ---')
    if not shutil.which('genhtml'):
      raise EnvironmentError('genhtml not found. Please install lcov.')
    if self.html_report_dir.exists():
      shutil.rmtree(self.html_report_dir)
    self.html_report_dir.mkdir(parents=True)

    self._run_command([
        'genhtml',
        str(self.merged_lcov_file), '--output-directory',
        str(self.html_report_dir)
    ])
    print(f'Successfully generated HTML report in {self.html_report_dir}')

  def run_baseline(self,
                   skip_gn_gen: bool = False,
                   generate_html_report: bool = False,
                   lcov_filter: Sequence[str] | None = None) -> None:
    """Executes the full coverage baseline process."""
    try:
      failed_targets = []
      if not self.post_process_only:
        if not skip_gn_gen:
          self.setup_gn_args()
        targets = self.get_test_targets()
        if not targets:
          return
        _, failed_targets = self.run_all_coverage(targets)

      self.merge_lcov_files()

      if lcov_filter:
        self.filter_lcov_file(lcov_filter)

      if generate_html_report:
        self.generate_html_report()

      if failed_targets:
        print('\n--- Unit Test Failures ---', file=sys.stderr)
        for target in failed_targets:
          print(f'- {target}', file=sys.stderr)
        print('--------------------------', file=sys.stderr)
        sys.exit(1)

      print('--- Coverage baseline script finished ---')

    except (subprocess.CalledProcessError, FileNotFoundError,
            EnvironmentError) as e:
      print(f'An error occurred: {e}', file=sys.stderr)
      sys.exit(1)


def main() -> None:
  """Parses arguments and runs the coverage baseline process."""
  parser = argparse.ArgumentParser(
      description='Establish Cobalt Code Coverage Baseline')
  parser.add_argument(
      '-p',
      '--platform',
      required=True,
      help='Target platform (e.g., android-x86)')
  parser.add_argument(
      '-c',
      '--build_type',
      default='qa',
      help='Build type (e.g., \'qa\', \'devel\')')
  parser.add_argument(
      '--cobalt_src_root',
      default='.',
      help='Path to the Cobalt source root directory')
  parser.add_argument(
      '-v',
      '--verbose',
      action='store_true',
      help='Enable verbose output for debugging.')
  parser.add_argument(
      '--skip-gn-gen',
      action='store_true',
      help='Skip the "gn gen" step.')
  parser.add_argument(
      '--test-target',
      type=str,
      help='The single test target to run.')
  parser.add_argument(
      '--post-process-only',
      action='store_true',
      help='Only run the post-processing steps (merge lcov files).')
  parser.add_argument(
      '--generate-html-report',
      action='store_true',
      help='Generate an HTML report from the merged lcov file.')
  parser.add_argument(
      '--lcov-filter',
      nargs='+',
      help='A list of directory patterns to include in the report.')
  parser.add_argument(
      '--test-filters-dir',
      type=str,
      help='Directory containing test filter JSON files.')
  parser.add_argument(
      '--include-skipped-tests',
      action='store_true',
      help='Include tests that are normally skipped by the filters.')
  args = parser.parse_args()

  runner = CoverageBaselineRunner(args.platform, args.build_type,
                                  args.cobalt_src_root, args.verbose,
                                  args.skip_gn_gen, args.test_target,
                                  args.post_process_only,
                                  args.test_filters_dir,
                                  args.include_skipped_tests)
  runner.run_baseline(args.skip_gn_gen, args.generate_html_report,
                      args.lcov_filter)


if __name__ == '__main__':
  main()