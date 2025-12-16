import sys
import os
import pathlib
import argparse
import shutil
import importlib.util

# This is the only code that should run at import time.
# All path resolution will be done in main().
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from command_runner import run_command


class CoverageRunner:
  """Orchestrates a CI code coverage check."""

  def __init__(self, target, src_root, script_dir):
    self.target = target
    self.src_root = src_root
    self.script_dir = script_dir
    self.lcov_file = self.src_root / "out" / "lcov_report" / f"{self.target}.lcov"

  def run(self):
    """Runs the full coverage analysis."""
    self._generate_lcov_report()
    self._check_coverage()
    print(f"\n✅ Success! Code coverage report generated.")

  def _generate_lcov_report(self):
    """Generates an LCOV report for a given target."""
    print(f"\n--- Generating LCOV report for '{self.target}' ---")
    coverage_py_path = self.src_root / "tools" / "code_coverage" / "coverage.py"
    build_dir = self.src_root / "out" / "coverage"
    lcov_output_dir = self.src_root / "out" / "lcov_report"
    llvm_dir = self.src_root / "third_party" / "llvm-build" / "Release+Asserts" / "bin"
    target_build_path = build_dir / self.target

    coverage_cmd = [
        "python3",
        str(coverage_py_path),
        self.target,
        "-b",
        str(build_dir),
        "-o",
        str(lcov_output_dir),
        "-c",
        str(target_build_path),
        "--coverage-tools-dir",
        str(llvm_dir),
        "--format=lcov",
    ]
    run_command(coverage_cmd)

  def _check_coverage(self):
    """Runs the check_coverage.py script."""
    print("\n--- Analyzing coverage and generating report ---")
    check_coverage_py_path = self.script_dir / "check_coverage.py"

    if not self.lcov_file.exists():
      print(
          f"Error: LCOV file not found at '{self.lcov_file}'", file=sys.stderr)
      sys.exit(1)

    check_cmd = [
        "python3",
        str(check_coverage_py_path),
        "--input",
        str(self.lcov_file),
        "--compare-branch",
        "origin/main",
        "--fail-under",
        "80.0",
    ]
    run_command(check_cmd)


def check_dependencies():
  """Checks for required system tools and Python packages."""
  if not shutil.which('lcov'):
    sys.exit("Error: 'lcov' system tool not found. Please install it.")
  if not shutil.which('genhtml'):
    sys.exit("Error: 'genhtml' system tool not found. Please install it.")
  if not importlib.util.find_spec('diff_cover'):
    sys.exit(
        "Error: 'diff-cover' not found. Please run: pip install diff-cover")


def main():
  """Main entry point for the script."""
  # All path resolution is now done here, not at the global scope.
  script_dir = pathlib.Path(__file__).parent.resolve()
  src_root = (script_dir / ".." / ".." / "..").resolve()

  check_dependencies()
  parser = argparse.ArgumentParser(
      description="Orchestrates a CI code coverage check.")
  parser.add_argument(
      'target',
      nargs='?',
      default='cobalt_unittests',
      help='The test target to run.')
  args = parser.parse_args()

  os.chdir(src_root)

  runner = CoverageRunner(args.target, src_root, script_dir)
  runner.run()


if __name__ == "__main__":
  main()
