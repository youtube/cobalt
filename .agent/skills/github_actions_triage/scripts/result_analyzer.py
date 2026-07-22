#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Analyzes GHA job logs for known error matching signatures."""

import argparse
import datetime
import json
import os
import re
import sys

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
if _scripts_dir not in sys.path:
  sys.path.append(_scripts_dir)
# pylint: disable=wrong-import-position
import gardener_utils

RULES = {
    'infra_error': [
        re.compile(r'Runner lost communication'),
        re.compile(r'connection reset by peer'),
        re.compile(r'API rate limit exceeded'),
        re.compile(r'No space left on device'),
        re.compile(r'HTTP 429'),
        re.compile(r'CIPD server error'),
        re.compile(r'overlayfs:'),
        re.compile(r'Failed to fetch file gs://'),  # GCS download failure
        re.compile(r'Unable to load AWS_CREDENTIAL_FILE'
                  ),  # AWS credential warning (often fatal in GCS context)
        re.compile(r'does not have a Release file'
                  ),  # Debian Buster EOL repository error
        re.compile(
            r'automatically failed because it uses a deprecated version of'
        ),  # GHA deprecated Actions version
        re.compile(
            r'Failed to allocate any device'),  # Lab device allocation failure
        re.compile(r'\bResult: ERROR\b'),  # YTS infrastructure/setup error
        re.compile(r'Timeout waiting for Kokoro build'),
        re.compile(
            r'No log files were collected\. The tests probably failed to run'),
        re.compile(r'Usage: python(?:3)? junit_mini_parser\.py'),
        re.compile(r'CommandException: No URLs matched:'),
        re.compile(r'unexpected disconnect'),
        re.compile(r'early EOF'),
        re.compile(r'invalid index-pack output'),
    ],
    'gn_error': [re.compile(r'ERROR at //.*\.gn'),],
    'ninja_error': [
        re.compile(r'FAILED: (?:obj)?/'
                  ),  # Ninja build failure (relative or absolute path)
        re.compile(r'err: remote-exec.*failed'),
    ],
    'compilation_error': [
        # GCC/Clang/Java/Kotlin compiler error:
        # file.cc:12: error: or file.cc:12:34: error:
        re.compile(r'\b\S+\.(?:cc|c|cpp|h|hpp|cxx|java|kt):\d+:(?:\d+:)?'
                   r'\s*(?:fatal\s+)?error:\s'),
        # MSVC compiler error:
        # file.cc(12): fatal error C1234: or file.cc(12): error C1234:
        re.compile(r'\b\S+\.(?:cc|c|cpp|h|hpp|cxx)\(\d+\)\s*:\s*'
                   r'(?:fatal\s+)?error\s+C\d+:\s'),
        # Linker error
        re.compile(r'undefined reference to'),
        re.compile(r'\bLNK\d+:\s'),
    ],
    'test_failure': [
        re.compile(r'\[  FAILED  \]'),
        re.compile(r'FAIL:\s'),
        re.compile(r'JUnit Failure:'),
        re.compile(r'\bResult: FAIL\b'),  # YTS test failure
    ],
    'crash_signature': [
        re.compile(r'Segmentation fault'),
        re.compile(r'SIGSEGV'),
        re.compile(r'CRASHED'),
        re.compile(r'Received signal'),
    ],
}

PRIORITY = [
    'infra_error',
    'crash_signature',
    'compilation_error',
    'gn_error',
    'ninja_error',
    'test_failure',
]

PRIORITY_MAP = {cat: idx for idx, cat in enumerate(PRIORITY)}


def _parse_traceback(lines, start_idx):
  """Parses traceback lines starting from start_idx.

  Args:
    lines: List of all log lines.
    start_idx: 0-based index of the 'Traceback' line in `lines`.

  Returns:
    A tuple of (tb_content, tb_exception_line, tb_exception_line_num, next_idx).
  """
  tb_lines = [lines[start_idx]]
  tb_start_line = start_idx + 1
  idx = start_idx + 1
  while idx < len(lines):
    next_line = lines[idx]
    tb_lines.append(next_line)
    if (next_line and not next_line[0].isspace() and
        not next_line.startswith('Traceback')):
      idx += 1
      break
    idx += 1

  tb_content = ''.join(tb_lines)
  tb_exception_line = tb_lines[-1].strip()
  tb_exception_line_num = tb_start_line + len(tb_lines) - 1
  return tb_content, tb_exception_line, tb_exception_line_num, idx


def analyze_log(log_path, context_lines=5):
  """Analyzes a job log file for compilation, test, or infra error patterns.

  Args:
    log_path: Path to the log file.
    context_lines: Number of lines of context to include around a match.

  Returns:
    A list of matches, each containing the line number, matching line,
    and context snippet.
  """
  if not os.path.exists(log_path):
    return [{
        'line_num': None,
        'line': f'Log file not found: {log_path}',
        'snippet': '',
    }]

  raw_matches = []
  try:
    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
      lines = f.readlines()

    idx = 0
    while idx < len(lines):
      line = lines[idx]
      line_num = idx + 1

      # Stateful traceback parser
      if line.startswith('Traceback (most recent call last):'):
        tb_content, tb_exception_line, tb_exception_line_num, next_idx = (
            _parse_traceback(lines, idx))

        # Check if traceback is a test wrapper assertion failure (ignore it)
        if 'cobalt_test_wrapper.py' in tb_content and (
            'AssertionError:' in tb_exception_line and
            '!= 0' in tb_exception_line):
          idx = next_idx
          continue

        # If it's a real traceback crash, record it (using the exception line)
        raw_matches.append({
            'line_num': tb_exception_line_num,
            'line': tb_exception_line
        })
        idx = next_idx
        continue

      # For non-traceback lines, run regex rules
      matched = False
      for _, regexes in RULES.items():
        for regex in regexes:
          if regex.search(line):
            matched = True
            break
        if matched:
          break

      if matched:
        raw_matches.append({'line_num': line_num, 'line': line.strip()})

      idx += 1

  except Exception as e:  # pylint: disable=broad-exception-caught
    return [{
        'line_num': None,
        'line': f'Error reading log file: {e}',
        'snippet': '',
    }]

  if not raw_matches:
    return [{
        'line_num': None,
        'line': 'No matching error signature found.',
        'snippet': '',
    }]

  # Generate matches with snippets
  matches = []
  for m in raw_matches:
    line_num = m['line_num']
    start = max(1, line_num - context_lines)
    end = min(len(lines), line_num + context_lines)
    snippet_lines = [
        f'{idx}: {lines[idx-1].rstrip()}' for idx in range(start, end + 1)
    ]
    snippet = '\n'.join(snippet_lines)

    matches.append({
        'line_num': line_num,
        'line': m['line'],
        'snippet': snippet
    })

  return matches


def generate_report(results):
  """Generates a structured branch health Triage report from GHA run results.

  Args:
    results: Dict containing run statistics and list of runs.

  Returns:
    A markdown formatted report.
  """
  total_jobs_fetched = results.get('total_jobs_fetched', 0)
  now = datetime.datetime.now(datetime.timezone.utc)

  branches = {}
  for run in results.get('runs', []):
    branch_name = run.get('branch', 'unknown')
    if branch_name not in branches:
      branches[branch_name] = {
          'runs': [],
          'failed_jobs': [],
          'outdated_runs': [],
      }

    branches[branch_name]['runs'].append(run)

    # Check if run is outdated
    is_outdated, age_str = gardener_utils.check_run_age(run, now)

    if is_outdated:
      if len(run.get('failed_jobs', [])) > 0:
        run_info = {
            'run_id': run.get('run_id'),
            'workflow_name': run.get('workflow_name'),
            'event': run.get('event', 'push'),
            'createdAt': run.get('createdAt'),
            'age': age_str,
            'url': run.get('url'),
            'failed_jobs_count': len(run.get('failed_jobs', [])),
        }
        branches[branch_name]['outdated_runs'].append(run_info)
      continue

    for job in run.get('failed_jobs', []):
      log_path = job.get('local_log_path', '')
      matches = analyze_log(log_path)

      job_info = {
          'run_id': run.get('run_id'),
          'workflow_name': run.get('workflow_name'),
          'branch': branch_name,
          'event': run.get('event'),
          'job_name': job.get('name'),
          'job_url': job.get('url'),
          'local_log_path': log_path,
          'time': run.get('createdAt', 'TODO'),
          'matches': matches,
      }

      branches[branch_name]['failed_jobs'].append(job_info)

  # Start report generation
  report = []
  report.append('# GitHub Build Status Triage Report\n')

  # Calculate total failed jobs across all branches
  total_failed = sum(len(b['failed_jobs']) for b in branches.values())

  report.append('## Job Stats')
  report.append(f'*   **Total Jobs Fetched**: {total_jobs_fetched}')
  report.append(f'*   **Failed Jobs**: {total_failed}\n')

  report.append('## Branch Health Report\n')

  sorted_branch_names = sorted(branches.keys())
  for branch in sorted_branch_names:
    branch_data = branches[branch]
    failed_count = len(branch_data['failed_jobs'])
    outdated_count = len(branch_data['outdated_runs'])

    if failed_count == 0 and outdated_count == 0:
      report.append(f'### Branch: {branch} (Healthy)')
      report.append('*   All runs completed successfully.\n')
    elif failed_count == 0 and outdated_count > 0:
      report.append(f'### Branch: {branch} (Outdated Failures)')
      report.append(
          f'*   No recent failures, but {outdated_count} outdated failed run(s)'
          ' exist (retrigger suggested).\n')
    else:
      report.append(f'### Branch: {branch} (Unhealthy)')
      report.append(f'*   **Failed Jobs**: {failed_count}')
      if outdated_count > 0:
        report.append(
            f'*   **Outdated Failed Runs**: {outdated_count} (retrigger'
            ' suggested)')
      report.append('')

  report.append('## Detailed Branch Failures\n')

  has_failures = False
  for branch in sorted_branch_names:
    branch_data = branches[branch]
    jobs = branch_data['failed_jobs']
    outdated_runs = branch_data['outdated_runs']

    if not jobs and not outdated_runs:
      continue

    has_failures = True
    report.append(f'### Branch: {branch}\n')

    if jobs:
      for job in jobs:
        report.append(f"#### Job: {job['job_name']}")
        report.append(f"*   **Invocation Time**: {job['time']}")
        report.append(f"*   **Cached Log**: `{job['local_log_path']}`")
        report.append(f"*   **URL**: {job['job_url']}")
        report.append('*   **Error Location(s)**:')

        for m in job['matches']:
          if m['line_num'] is not None:
            report.append(f"    *   Line {m['line_num']}: `{m['line']}`")
            context_lines = 5
            start_line = max(1, m['line_num'] - context_lines)
            line_count = 1 + context_lines * 2
            report.append(
                f'        *   Log Slice Command: `tail -n +{start_line}'
                f" \"{job['local_log_path']}\" | head -n {line_count}`")
          else:
            report.append(f"    *   `{m['line']}`")
        report.append('')

    if outdated_runs:
      report.append('#### Outdated Failed Runs (Retrigger Suggested)\n')
      for run in outdated_runs:
        report.append(
            f"*   **{run['workflow_name']}** (Run ID: {run['run_id']}, Event:"
            f" {run['event']}) - Failed {run['age']}.")
        report.append(f"    *   URL: {run['url']}\n")

  if not has_failures:
    report.append('*   None\n')

  return '\n'.join(report)


def main():
  parser = argparse.ArgumentParser(
      description='Analyze GitHub run results and generate a triage report.')
  parser.add_argument(
      'results_json', help='Path to the github_results.json file.')
  args = parser.parse_args()

  if not os.path.exists(args.results_json):
    print(
        f'Error: Results file not found: {args.results_json}', file=sys.stderr)
    sys.exit(1)

  try:
    with open(args.results_json, 'r', encoding='utf-8') as f:
      results = json.load(f)
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Error reading JSON file: {e}', file=sys.stderr)
    sys.exit(1)

  report = generate_report(results)
  print(report)


if __name__ == '__main__':
  main()
