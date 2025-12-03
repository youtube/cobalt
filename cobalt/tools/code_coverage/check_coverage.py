"Automates the process of checking code coverage."

import argparse
import json
import pathlib
import re
import subprocess
import sys
import shutil

# Add the script's directory to the Python path to allow importing command_runner
sys.path.append(str(pathlib.Path(__file__).parent.resolve()))

try:
  from command_runner import run_command
except ImportError:
  print(('Error: command_runner.py not found. '
         'Please ensure it is in the same directory as this script.'),
        file=sys.stderr)
  sys.exit(1)


def get_absolute_coverage(lcov_file: pathlib.Path) -> float:
  """
    Calculate the absolute line coverage from an LCOV file.

    Args:
        lcov_file: The path to the LCOV file.

    Returns:
        The absolute line coverage as a percentage.
    """
  if not shutil.which('lcov'):
    print(("Error: 'lcov' system tool not found. "
           "Please install it (e.g., sudo apt install lcov)."),
          file=sys.stderr)
    return 0.0

  try:
    result = run_command(
        ['lcov', '--summary', str(lcov_file)], capture_output=True, text=True)
    summary_output = result.stdout
    # Use a regular expression to find the line coverage percentage
    match = re.search(r'lines\.*:\s*([\d\.]+)%', summary_output)
    if match:
      return float(match.group(1))
  except subprocess.CalledProcessError as e:
    print(f'Error running lcov: {e.stderr}', file=sys.stderr)
    # Fallback for older lcov versions that might not support --summary
    try:
      with open(lcov_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
      total_lines = 0
      hit_lines = 0
      for line in lines:
        if line.startswith('DA:'):
          parts = line.split(',')
          if len(parts) == 2:
            total_lines += 1
            if not parts[1].strip().endswith('0'):
              hit_lines += 1
      if total_lines > 0:
        return (hit_lines / total_lines) * 100
    except IOError as ie:
      print(f'Error reading LCOV file: {ie}', file=sys.stderr)
  return 0.0


def get_differential_coverage(compare_branch: str) -> float:
  """
    Calculate the differential code coverage using diff-cover.

    Args:
        compare_branch: The branch to compare against.

    Returns:
        The differential code coverage as a percentage.
    """
  if not shutil.which('diff-cover'):
    print(("Error: 'diff-cover' not found. "
           "Please run: pip install diff-cover"),
          file=sys.stderr)
    return 0.0

  try:
    result = run_command(
        ['diff-cover', '--json-report', '/dev/stdout', compare_branch],
        capture_output=True,
        text=True)
    diff_cover_output = result.stdout
    diff_report = json.loads(diff_cover_output)
    return diff_report.get('percent_covered', 0.0)
  except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
    print(f'Error running diff-cover: {e}', file=sys.stderr)
    return 0.0


def main():
  """Main function to run the coverage check."""
  parser = argparse.ArgumentParser(
      description='Check code coverage and enforce thresholds.')
  parser.add_argument(
      '--input', type=pathlib.Path, help='The path to the LCOV file.')
  parser.add_argument(
      '--compare-branch',
      type=str,
      default='origin/main',
      help='The branch to compare against for differential coverage.')
  parser.add_argument(
      '--fail-under',
      type=float,
      default=80.0,
      help='The minimum differential coverage threshold.')
  parser.add_argument(
      '--report-file',
      type=pathlib.Path,
      default='coverage_summary.md',
      help='The path to save the coverage report.')
  args = parser.parse_args()

  absolute_coverage = 0.0
  if args.input:
    if not args.input.exists():
      print(f"Error: Input file '{args.input}' not found.", file=sys.stderr)
      sys.exit(1)
    absolute_coverage = get_absolute_coverage(args.input)

  differential_coverage = get_differential_coverage(args.compare_branch)

  # Generate a markdown report
  report_content = '## 📊 Code Coverage Report\n\n'
  report_content += '| Metric | Percentage | Threshold | Status |\n'
  report_content += '| :--- | :--- | :--- | :--- |\n'
  diff_status = ('✅ Pass'
                 if differential_coverage >= args.fail_under else '❌ Fail')
  report_content += (f'| **Differential** (New Code) | '
                     f'**{differential_coverage:.2f}%** | '
                     f'{args.fail_under}% | {diff_status} |\n')
  report_content += (f'| **Absolute** (Total) | '
                     f'**{absolute_coverage:.2f}%** | N/A | ℹ️ Info |\n')

  with open(args.report_file, 'w', encoding='utf-8') as f:
    f.write(report_content)

  print(f'\nReport saved to: {args.report_file}\n')

  if differential_coverage < args.fail_under:
    print(
        f'[FAILURE] Differential coverage ({differential_coverage:.2f}%) '
        f'is below the threshold ({args.fail_under}%).',
        file=sys.stderr)
    sys.exit(1)
  else:
    print('[SUCCESS] All checks passed.')


if __name__ == '__main__':
  main()
