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
               cobalt_src_root: str = '.'):
    """Initializes the CoverageBaselineRunner.

    Args:
      platform: The target platform, e.g., 'linux-x64'.
      build_type: The build type, e.g., 'qa', 'devel'.
      cobalt_src_root: The path to the root of the Cobalt source code.
    """
    self.platform = platform
    self.build_type = build_type
    self.cobalt_src_root = pathlib.Path(cobalt_src_root).resolve()

    self.gn_gen_dir = (
        self.cobalt_src_root / f'out/{self.platform}_{self.build_type}')
    self.coverage_build_dir = (
        self.cobalt_src_root / f'out/coverage_{self.platform}')
    self.llvm_bin_dir = (
        self.cobalt_src_root / 'third_party/llvm-build/Release+Asserts/bin')
    self.raw_lcov_dir = self.cobalt_src_root / f'out/lcov_raw_{self.platform}'
    self.merged_lcov_file = (
        self.cobalt_src_root / f'out/lcov_merged_{self.platform}.info')
    self.html_report_dir = (
        self.cobalt_src_root / f'out/lcov_html_report_{self.platform}')
    base_target_path = self.cobalt_src_root / 'cobalt/build/testing/targets'
    platform_target_dir = self.platform
    if self.platform == 'android-x86':
      platform_target_dir = 'android-arm'

    self.test_targets_json = (
        base_target_path / platform_target_dir / 'test_targets.json')

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
    print(f'Running: {' '.join(command)}', flush=True)  # pylint: disable=W1309
    try:
      result = subprocess.run(
          command, check=check, cwd=cwd, capture_output=True, text=True)
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

  def build_tests(self, targets: Sequence[str]) -> None:
    """Builds the specified test targets.

    Args:
      targets: A list of test target names to build.
    """
    if not targets:
      print('No targets to build.')
      return

    print('--- Building test targets for coverage ---')
    self._run_command(
        ['autoninja', '-C', str(self.coverage_build_dir)] + list(targets))
    print('Build complete.')

  def run_coverage_for_target(self, test_name: str) -> bool:
    """Runs the coverage tool for a single test target.

    Args:
      test_name: The name of the test target to run.

    Returns:
      True if coverage was run successfully, False otherwise.
    """
    print(f'--- Processing: {test_name} ---')
    test_lcov_out_dir = self.raw_lcov_dir / test_name
    test_lcov_out_dir.mkdir(parents=True, exist_ok=True)

    test_binary_path = self.coverage_build_dir / test_name

    if not test_binary_path.exists():
      print(
          f'WARNING: Test binary not found at {test_binary_path}. Skipping.',
          file=sys.stderr)
      return False

    cmd = [
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
        '--run',
    ]
    try:
      self._run_command(cmd)
      lcov_file = test_lcov_out_dir / 'lcov.info'
      if not lcov_file.exists():
        print(
            f'WARNING: lcov.info not found for {test_name} in '
            f'{test_lcov_out_dir}',
            file=sys.stderr)
        return False
      return True
    except subprocess.CalledProcessError:
      print(f'ERROR: Coverage run failed for {test_name}', file=sys.stderr)
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
        successful_targets.append(target)
    return successful_targets

  def merge_lcov_files(self) -> bool:
    """Merges individual lcov.info files into a single file.

    Returns:
      True if merge was successful, False otherwise.
    """
    print('--- Merging LCOV files ---')
    lcov_files = glob.glob(str(self.raw_lcov_dir / '*' / 'lcov.info'))

    if not lcov_files:
      print(f'No lcov.info files found in {self.raw_lcov_dir}')
      return False

    if shutil.which('lcov') is None:
      raise EnvironmentError(
          'lcov tool not found. Please install it, e.g., \'sudo apt-get '
          'install lcov\'')

    if self.merged_lcov_file.exists():
      self.merged_lcov_file.unlink()

    first_file = lcov_files.pop(0)
    self.merged_lcov_file.touch()  # Workaround for lcov bug.
    self._run_command(
        ['lcov', '-a', first_file, '-o',
         str(self.merged_lcov_file)])

    for lcov_file in lcov_files:
      self._run_command(
          ['lcov', '-a', lcov_file, '-o',
           str(self.merged_lcov_file)])

    print(f'Merged LCOV data written to {self.merged_lcov_file}')
    return True

  def generate_html_report(self) -> bool:
    """Generates an HTML report from the merged LCOV file.

    Returns:
      True if the report was generated, False otherwise.
    """
    print('--- Generating HTML report ---')
    if not self.merged_lcov_file.exists():
      print(f'Merged LCOV file not found: {self.merged_lcov_file}')
      return False

    if shutil.which('genhtml') is None:
      raise EnvironmentError(
          'genhtml tool not found. Please install it (part of lcov package)')

    if self.html_report_dir.exists():
      shutil.rmtree(self.html_report_dir)

    self._run_command([
        'genhtml',
        str(self.merged_lcov_file),
        '--output-directory',
        str(self.html_report_dir),
    ])
    print(f'HTML report generated in file://{self.html_report_dir}/index.html')
    return True

  def run_baseline(self) -> None:
    """Executes the full coverage baseline process."""
    try:
      self.setup_gn_args()
      targets = self.get_test_targets()
      if not targets:
        return

      self.build_tests(targets)
      self.run_all_coverage(targets)

      if self.merge_lcov_files():
        self.generate_html_report()

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
  args = parser.parse_args()

  runner = CoverageBaselineRunner(args.platform, args.build_type,
                                  args.cobalt_src_root)
  runner.run_baseline()


if __name__ == '__main__':
  main()
