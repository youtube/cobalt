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
               post_process_only: bool = False):
    """Initializes the CoverageBaselineRunner.

    Args:
      platform: The target platform, e.g., 'linux-x64'.
      build_type: The build type, e.g., 'qa', 'devel'.
      cobalt_src_root: The path to the root of the Cobalt source code.
      verbose: Whether to print verbose output.
      skip_gn_gen: Whether to skip the 'gn gen' step.
      test_target: The single test target to run.
      post_process_only: Whether to only run the post-processing steps.
    """
    self.platform = platform
    self.build_type = build_type
    self.cobalt_src_root = pathlib.Path(cobalt_src_root).resolve()
    self.verbose = verbose
    self.skip_gn_gen = skip_gn_gen
    self.test_target = test_target
    self.post_process_only = post_process_only

    self.gn_gen_dir = (
        self.cobalt_src_root / f'out/{self.platform}_{self.build_type}')
    self.coverage_build_dir = (
        self.cobalt_src_root / f'out/coverage_{self.platform}')
    self.llvm_bin_dir = (
        self.cobalt_src_root / 'third_party/llvm-build/Release+Asserts/bin')
    self.raw_lcov_dir = self.cobalt_src_root / f'out/lcov_raw_{self.platform}'
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

    if not test_binary_path.exists():
      print(
          f'WARNING: Test binary not found at {test_binary_path}. Skipping.',
          file=sys.stderr)
      return False

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

  def run_all_coverage(self, targets: Sequence[str]) -> list[str]:
    """Runs coverage for all specified test targets serially.

    Args:
      targets: A list of test target names to run.

    Returns:
      A list of targets that were successfully processed.
    """
    print('--- Running tests and generating coverage data (serially) ---')
    if self.raw_lcov_dir.exists():
      shutil.rmtree(self.raw_lcov_dir)
    self.raw_lcov_dir.mkdir(parents=True)

    successful_targets = []
    for target in targets:
      if self.run_coverage_for_target(target):
        print(f'--- {target}: PASSED ---')
        successful_targets.append(target)
      else:
        print(f'--- {target}: FAILED ---', file=sys.stderr)
    return successful_targets


  def run_baseline(self) -> None:
    """Executes the full coverage baseline process."""
    try:
      if self.post_process_only:
        self.merge_lcov_files()
        print('--- Coverage baseline script finished ---')
        return

      if not self.skip_gn_gen:
        self.setup_gn_args()
      targets = self.get_test_targets()
      if not targets:
        return

      self.run_all_coverage(targets)

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
  args = parser.parse_args()

  runner = CoverageBaselineRunner(args.platform, args.build_type,
                                  args.cobalt_src_root, args.verbose,
                                  args.skip_gn_gen, args.test_target)
  runner.run_baseline(args.skip_gn_gen)


if __name__ == '__main__':
  main()
