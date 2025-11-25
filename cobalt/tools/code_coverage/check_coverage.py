import argparse
import sys
import os
import subprocess
import json


def parse_lcov_absolute_coverage(lcov_file_path):
  """
    Parses an LCOV file to calculate absolute line coverage percentage.
    Sums up LF (Lines Found) and LH (Lines Hit) tags.
    """
  total_lines_found = 0
  total_lines_hit = 0

  if not os.path.exists(lcov_file_path):
    print(f"Error: LCOV file not found at {lcov_file_path}", file=sys.stderr)
    sys.exit(1)

  with open(lcov_file_path, 'r') as f:
    for line in f:
      if line.startswith('LF:'):
        total_lines_found += int(line.strip().split(':')[1])
      elif line.startswith('LH:'):
        total_lines_hit += int(line.strip().split(':')[1])

  if total_lines_found == 0:
    return 0.0

  return (total_lines_hit / total_lines_found) * 100.0


def get_differential_coverage(lcov_file_path, compare_branch):
  """
    Runs `diff-cover` to calculate coverage of changed lines.
    """
  json_report_path = "diff_cover_report.json"

  # We use a temp JSON report to extract the exact number
  cmd = [
      "diff-cover", lcov_file_path, f"--compare-branch={compare_branch}",
      f"--json-report={json_report_path}"
  ]

  try:
    # Run silently
    subprocess.run(
        cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    with open(json_report_path, 'r') as f:
      data = json.load(f)
      # Extract percentage. structure depends on diff-cover version,
      # but typically located at root or under 'report'
      # Fallback logic for common structures:
      if 'coverage' in data:
        return float(data['coverage'])
      # specific to some diff-cover versions
      root = data.get(next(iter(data))) if data else {}
      return float(root.get('coverage', 0.0))

  except subprocess.CalledProcessError as e:
    print(f"Error running diff-cover: {e}", file=sys.stderr)
    print(f"Stdout: {e.stdout.decode()}", file=sys.stderr)
    print(f"Stderr: {e.stderr.decode()}", file=sys.stderr)
    sys.exit(1)
  except FileNotFoundError:
    print(
        "Error: diff-cover command not found. Is it installed and in your PATH?",
        file=sys.stderr)
    sys.exit(1)
  except json.JSONDecodeError:
    print(
        f"Error: Could not decode JSON report from {json_report_path}",
        file=sys.stderr)
    sys.exit(1)
  except Exception as e:
    print(f"An unexpected error occurred: {e}", file=sys.stderr)
    sys.exit(1)


def main():
  parser = argparse.ArgumentParser(
      description="Calculate and enforce code coverage metrics.")
  parser.add_argument(
      "--lcov-file", required=True, help="Path to the LCOV file.")
  parser.add_argument(
      "--compare-branch",
      default="origin/main",
      help="Git branch to compare against for differential coverage.")
  parser.add_argument(
      "--threshold",
      type=float,
      default=80.0,
      help="Minimum differential coverage percentage required.")
  parser.add_argument(
      "--report-file",
      default="coverage_report.md",
      help="Output file for the Markdown report.")

  args = parser.parse_args()

  # Calculate Absolute Coverage
  absolute_coverage = parse_lcov_absolute_coverage(args.lcov_file)
  print(f"Absolute Coverage: {absolute_coverage:.2f}%")

  # Calculate Differential Coverage
  differential_coverage = get_differential_coverage(args.lcov_file,
                                                    args.compare_branch)
  print(f"Differential Coverage (New Code): {differential_coverage:.2f}%")

  # Generate Markdown Report
  with open(args.report_file, 'w') as f:
    f.write(f"# Code Coverage Report\n\n")
    f.write(f"## Absolute Coverage\n")
    f.write(f"Total coverage for the project: **{absolute_coverage:.2f}%**\n\n")
    f.write(f"## Differential Coverage\n")
    f.write(
        f"Coverage for new/changed code compared to `{args.compare_branch}`: **{differential_coverage:.2f}%**\n\n"
    )

    if differential_coverage >= args.threshold:
      f.write(
          f"✅ Differential coverage of {differential_coverage:.2f}% meets the required threshold of {args.threshold:.2f}%\n"
      )
    else:
      f.write(
          f"❌ Differential coverage of {differential_coverage:.2f}% is below the required threshold of {args.threshold:.2f}%\n"
      )
      sys.exit(1)  # Exit with error if threshold not met


if __name__ == "__main__":
  main()
