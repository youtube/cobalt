#!/usr/bin/env python3
"""
Orchestrates a CI code coverage check by generating coverage data and enforcing
quality gates.
"""

import subprocess
import sys
import os
import pathlib

# --- Configuration ---
# The target to build and run for coverage analysis.
TARGET_TO_COVERAGE = "crypto_unittests"

# The final location of the LCOV file, relative to the checkout root.
LCOV_FILE = f"out/lcov_report/{TARGET_TO_COVERAGE}.lcov"

# The name of the final Markdown report file.
REPORT_FILE = "coverage_summary.md"

# The script's location is cobalt/tools/code_coverage
SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()
# The checkout root is three levels up from this script.
SRC_ROOT = (SCRIPT_DIR / ".." / ".." / "..").resolve()


def run_command(cmd, check=True):
  """
  Executes a command, streams its output, and exits if it fails.
  """
  print(f"--- Running: {' '.join(cmd)} ---")
  try:
    # We stream the output instead of capturing it to provide real-time
    # feedback in a CI environment.
    process = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    for line in iter(process.stdout.readline, ''):
      sys.stdout.write(line)

    process.wait()
    if check and process.returncode != 0:
      print(
          f"\n--- Command failed with exit code {process.returncode} ---",
          file=sys.stderr)
      sys.exit(process.returncode)
  except FileNotFoundError as e:
    print(f"Error: Command not found - {e.filename}", file=sys.stderr)
    sys.exit(1)
  except Exception as e:
    print(f"An unexpected error occurred: {e}", file=sys.stderr)
    sys.exit(1)


def main():
  """Main entry point for the script."""
  # Ensure all commands are run from the root of the checkout.
  os.chdir(SRC_ROOT)

  # 1. Install Python dependency
  print("--- Installing diff-cover dependency ---")
  run_command([sys.executable, "-m", "pip", "install", "diff-cover"])

  # 2. Run the Chromium coverage tool to generate the LCOV file
  print(f"\n--- Generating LCOV report for '{TARGET_TO_COVERAGE}' ---")
  coverage_py_path = SRC_ROOT / "tools" / "code_coverage" / "coverage.py"
  build_dir = SRC_ROOT / "out" / "coverage"
  lcov_output_dir = SRC_ROOT / "out" / "lcov_report"
  llvm_dir = SRC_ROOT / "third_party" / "llvm-build" / "Release+Asserts" / "bin"

  coverage_cmd = [
      "python3",
      str(coverage_py_path),
      TARGET_TO_COVERAGE,
      "-b",
      str(build_dir),
      "-o",
      str(lcov_output_dir),
      "-c",
      str(build_dir / TARGET_TO_COVERAGE),
      "--coverage-tools-dir",
      str(llvm_dir),
      "--format=lcov",
  ]
  run_command(coverage_cmd)

  # 3. Run the check_coverage.py script
  print("\n--- Analyzing coverage and generating report ---")
  check_coverage_py_path = SCRIPT_DIR / "check_coverage.py"
  final_lcov_path = SRC_ROOT / LCOV_FILE

  if not final_lcov_path.exists():
    print(
        f"Error: LCOV file not found at '{final_lcov_path}' after running "
        "coverage.py.",
        file=sys.stderr)
    sys.exit(1)

  check_cmd = [
      "python3",
      str(check_coverage_py_path),
      "--input",
      str(final_lcov_path),
      "--compare-branch",
      "origin/main",
      "--fail-under",
      "80.0",
  ]
  run_command(check_cmd)

  # 4. Echo the location of the generated markdown report
  print(f"\n✅ Success! Code coverage report generated at: {REPORT_FILE}")


if __name__ == "__main__":
  main()
