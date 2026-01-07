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
"""Generates a code coverage report for Cobalt."""
import argparse
import shutil
import subprocess
from pathlib import Path

BASE_DIR = Path.cwd().resolve()

# --- Script ---

PLATFORM_PARSER = argparse.ArgumentParser(
    description="Generate a code coverage report.")
PLATFORM_PARSER.add_argument(
    "-p",
    "--platform",
    default="android-x86",
    help="The platform to generate the report for.")
PLATFORM_PARSER.add_argument(
    "-i",
    "--input-file",
    default=None,
    help="Path to a single .lcov file to generate a report for.")
PLATFORM_PARSER.add_argument(
    "-f",
    "--filters",
    action="append",
    help="Extraction patterns to apply to the report.")


def run_command(command, cwd=None):
  """Runs a command and returns True on success, False on failure."""
  print(f'Running command: {" ".join(str(c) for c in command)}')
  try:
    process = subprocess.run(
        command, check=True, capture_output=True, text=True, cwd=cwd)
    if process.stdout:
      print(process.stdout)
    if process.stderr:
      print(process.stderr)
    return True
  except subprocess.CalledProcessError as e:
    print(f'ERROR running command: {" ".join(str(c) for c in command)}')
    if e.stdout:
      print(f"Stdout: {e.stdout}")
    if e.stderr:
      print(f"Stderr: {e.stderr}")
    return False


def normalize_lcov_paths(filepath):
  """
    Rewrites an lcov file so all source file paths (SF: lines) are relative
    to BASE_DIR, handling regular source and generated files from their
    distinct locations.
    """
  print(f"Normalizing paths in {filepath} to be relative to {BASE_DIR}...")
  try:
    lines = filepath.read_text().splitlines()
    new_lines = []
    skipping = False
    for line in lines:
      if line.startswith("SF:"):
        skipping = line.startswith("SF:gen/")

      if not skipping:
        if line.startswith("SF:../../"):
          path = Path(line[3:]).relative_to("../../")
          new_lines.append(f"SF:{path}")
        else:
          new_lines.append(line)

    filepath.write_text("\n".join(new_lines) + "\n")
  except IOError as e:
    print(f"Could not read or write to {filepath}: {e}")


def main():
  """Main function to generate the coverage report."""
  args, _ = PLATFORM_PARSER.parse_known_args()
  platform = args.platform

  # --- Configuration ---
  # The root directory of your checkout.
  # The script assumes it's run from the checkout root.
  base_dir = Path.cwd().resolve()

  # Directory where the individual .lcov files are located.
  lcov_src_dir = base_dir / "out" / f"{platform}_coverage"

  # Directory where the generated source files are located.
  gen_files_dir = base_dir / "out" / f"coverage_{platform}"

  # Temporary directory to store cleaned-up tracefiles.
  cleaned_dir = lcov_src_dir / "cleaned"

  # Final merged and filtered .lcov file for cobalt.
  final_lcov_file = lcov_src_dir / "cobalt_coverage.lcov"

  # Final output directory for the HTML report.
  report_dir = base_dir / "out" / "cobalt_coverage_report"

  if args.input_file:
    input_file = Path(args.input_file)
    if not input_file.exists():
      print(f"ERROR: Input file does not exist: {input_file}")
      return

    report_dir = base_dir / "out" / f"{input_file.stem}_report"
    if report_dir.exists():
      shutil.rmtree(report_dir)
    report_dir.mkdir(parents=True, exist_ok=True)

    normalized_file = report_dir / "normalized.lcov"
    shutil.copy(input_file, normalized_file)
    normalize_lcov_paths(normalized_file)

    genhtml_cmd = [
        "genhtml",
        str(normalized_file), "--output-directory",
        str(report_dir), "--ignore-errors", "inconsistent,range"
    ]
    # The extract command is only needed if we want to filter the input file.
    # For a single file report, we assume the input file is already filtered.
    # If further filtering is needed, we would need to run lcov --extract here.
    # For now, we directly use genhtml.
    if not run_command(genhtml_cmd, cwd=base_dir):
      print("ERROR: Failed to generate HTML report. Exiting.")
      return

    print("\n--- Coverage Report Generation Complete ---")
    print("Success! You can view the report by opening:")
    print(f"file://{report_dir}/index.html")
    return

  print("---"
        " Starting Coverage Report Generation "
        "---")

  if not lcov_src_dir.exists():
    print(f"ERROR: LCOV source directory does not exist: {lcov_src_dir}")
    return
  if not gen_files_dir.exists():
    print(f"ERROR: Generated files directory does not exist: {gen_files_dir}")
    return

  if cleaned_dir.exists():
    shutil.rmtree(cleaned_dir)
  cleaned_dir.mkdir(parents=True, exist_ok=True)

  print(f"\n--- Processing individual .lcov files from {lcov_src_dir} ---")
  initial_lcov_files = list(lcov_src_dir.glob("**/*.lcov"))
  if not initial_lcov_files:
    print(f"ERROR: No .lcov files found in {lcov_src_dir}. Exiting.")
    return

  for lcov_file in initial_lcov_files:
    if cleaned_dir in lcov_file.parents or lcov_file == final_lcov_file:
      continue

    print(f"\nProcessing: {lcov_file}")
    cleaned_file = cleaned_dir / lcov_file.name

    extract_patterns = args.filters or ["*/cobalt/*", "*/starboard/*"]
    extract_cmd = ["lcov", "--extract", str(lcov_file)]
    extract_cmd.extend(extract_patterns)
    extract_cmd.extend([
        "--exclude", "*/out/*", "--output-file",
        str(cleaned_file), "--ignore-errors",
        "inconsistent,corrupt,unsupported,empty,unused"
    ])
    run_command(extract_cmd)

    if cleaned_file.exists() and cleaned_file.stat().st_size > 0:
      normalize_lcov_paths(cleaned_file)

  print(f"\n--- Merging cleaned .lcov files into {final_lcov_file} ---")

  cleaned_files_to_merge = [
      f for f in cleaned_dir.glob("*.lcov")
      if f.exists() and f.stat().st_size > 0
  ]

  if not cleaned_files_to_merge:
    print(
        "ERROR: No coverage data found for 'cobalt' in any of the tracefiles. "
        "Exiting.")
    return

  merge_cmd = ["lcov"]
  for cleaned_file in cleaned_files_to_merge:
    merge_cmd.extend(["--add-tracefile", str(cleaned_file)])
  merge_cmd.extend(["--output-file", str(final_lcov_file)])

  if not run_command(merge_cmd):
    print("ERROR: Failed to merge cleaned .lcov files. Exiting.")
    return

  print(f"\n--- Generating HTML report in {report_dir} ---")
  genhtml_cmd = [
      "genhtml",
      str(final_lcov_file), "--output-directory",
      str(report_dir), "--ignore-errors", "inconsistent,range"
  ]
  if not run_command(genhtml_cmd, cwd=base_dir):
    print("ERROR: Failed to generate HTML report. Exiting.")
    return

  print("\n--- Coverage Report Generation Complete ---")
  print("Success! You can view the report by opening:")
  print(f"file://{report_dir}/index.html")


if __name__ == "__main__":
  main()
