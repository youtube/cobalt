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
import zipfile
from pathlib import Path

# --- Script ---

PLATFORM_PARSER = argparse.ArgumentParser(
    description="Generate a code coverage report.")
PLATFORM_PARSER.add_argument(
    "-p",
    "--platform",
    default="android-x86",
    help="The platform to generate the report for.")
PLATFORM_PARSER.add_argument(
    "-f",
    "--filters",
    action="append",
    help="Extraction patterns to apply to the report.")
PLATFORM_PARSER.add_argument(
    "--no-zip",
    action="store_false",
    dest="zip",
    help="Do not zip the individual .lcov files.")
PLATFORM_PARSER.set_defaults(zip=True)
PLATFORM_PARSER.add_argument(
    "--html", action="store_true", help="Generate an HTML report.")


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
  """Rewrites an lcov file so all source file paths are relative to root.
  """
  print(f"Normalizing paths in {filepath}...")
  try:
    lines = filepath.read_text().splitlines()
    new_lines = []
    skipping = False
    for line in lines:
      if line.startswith("SF:"):
        # Skip generated files.
        if line.startswith("SF:gen/"):
          skipping = True
        else:
          skipping = False
          # Normalize paths starting with ../../ to be relative to root.
          if line.startswith("SF:../../"):
            line = "SF:" + line[9:]

      if not skipping:
        new_lines.append(line)

    filepath.write_text("\n".join(new_lines) + "\n")
  except IOError as e:
    print(f"Could not read or write to {filepath}: {e}")


def create_zip(zip_path, files_to_zip):
  """Creates a zip file containing the specified files."""
  print(f"\n--- Zipping {len(files_to_zip)} files into {zip_path} ---")
  try:
    with zipfile.ZipFile(zip_path, "w") as zipf:
      for file in files_to_zip:
        zipf.write(file, arcname=file.name)
    return True
  except (IOError, zipfile.BadZipFile) as e:
    print(f"ERROR: Failed to create zip file: {e}")
    return False


def generate_report(platform,
                    base_dir,
                    filters=None,
                    zip_files=True,
                    html_report=False):
  """Generates the coverage report."""
  # Directory where the individual .lcov files are located.
  lcov_src_dir = base_dir / "out" / f"{platform}_coverage"

  # Temporary directory to store cleaned-up tracefiles.
  cleaned_dir = lcov_src_dir / "cleaned"

  # Final merged and filtered .lcov file for cobalt.
  final_lcov_file = lcov_src_dir / "cobalt_coverage.lcov"

  # Final output directory for the HTML report.
  report_dir = base_dir / "out" / "cobalt_coverage_report"

  # Final ZIP file for individual .lcov files.
  zip_file = base_dir / "out" / f"{platform}_coverage.zip"

  print("---"
        " Starting Coverage Report Generation "
        "---")

  if not lcov_src_dir.exists():
    print(f"ERROR: LCOV source directory does not exist: {lcov_src_dir}")
    return False

  if cleaned_dir.exists():
    shutil.rmtree(cleaned_dir)
  cleaned_dir.mkdir(parents=True, exist_ok=True)

  print(f"\n--- Processing individual .lcov files from {lcov_src_dir} ---")
  initial_lcov_files = list(lcov_src_dir.glob("**/*.lcov"))
  if not initial_lcov_files:
    print(f"ERROR: No .lcov files found in {lcov_src_dir}. Exiting.")
    return False

  for lcov_file in initial_lcov_files:
    if cleaned_dir in lcov_file.parents or lcov_file == final_lcov_file:
      continue

    print(f"\nProcessing: {lcov_file}")
    relative_path = lcov_file.relative_to(lcov_src_dir)
    cleaned_file = cleaned_dir / str(relative_path).replace("/", "_")

    extract_patterns = filters or ["*/cobalt/*", "*/starboard/*"]
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
    return False

  merge_cmd = ["lcov"]
  for cleaned_file in cleaned_files_to_merge:
    merge_cmd.extend(["--add-tracefile", str(cleaned_file)])
  merge_cmd.extend(["--output-file", str(final_lcov_file)])

  if not run_command(merge_cmd):
    print("ERROR: Failed to merge cleaned .lcov files. Exiting.")
    return False

  if zip_files:
    # Zip individual cleaned files AND the final merged file.
    create_zip(zip_file, cleaned_files_to_merge + [final_lcov_file])

  if html_report:
    print(f"\n--- Generating HTML report in {report_dir} ---")
    genhtml_cmd = [
        "genhtml",
        str(final_lcov_file), "--output-directory",
        str(report_dir), "--ignore-errors", "inconsistent,range"
    ]
    if not run_command(genhtml_cmd, cwd=base_dir):
      print("ERROR: Failed to generate HTML report. Exiting.")
      return False

  print("\n--- Coverage Report Generation Complete ---")
  if zip_files:
    print(f"Success! ZIP archive created: {zip_file}")
  if html_report:
    print(f"Success! HTML report generated in: {report_dir}")
    print(f"You can view it by opening: file://{report_dir}/index.html")
  return True


def main():
  """Main function to generate the coverage report."""
  args, _ = PLATFORM_PARSER.parse_known_args()
  base_dir = Path.cwd().resolve()
  generate_report(
      args.platform,
      base_dir,
      filters=args.filters,
      zip_files=args.zip,
      html_report=args.html)


if __name__ == "__main__":
  main()
