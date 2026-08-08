#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
# TAG=agy
"""Unified analyzer for CI results."""

import argparse
import datetime
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

# Recency Limits
NIGHTLY_RECENCY_LIMIT = datetime.timedelta(hours=24)
POSTSUBMIT_RECENCY_LIMIT = datetime.timedelta(days=7)
NIGHTLY_EVENTS = ('nightly', 'schedule', 'workflow_dispatch')

# Unified Rules
RULES = {
    'infra_error': [
        re.compile(r'Runner lost communication'),
        re.compile(r'connection reset by peer'),
        re.compile(r'API rate limit exceeded'),
        re.compile(r'No space left on device'),
        re.compile(r'HTTP 429'),
        re.compile(r'CIPD server error'),
        re.compile(r'overlayfs:'),
        re.compile(r'Failed to fetch file gs://'),
        re.compile(r'Unable to load AWS_CREDENTIAL_FILE'),
        re.compile(r'does not have a Release file'),
        re.compile(
            r'automatically failed because it uses a deprecated version of'),
        re.compile(r'Failed to allocate any device'),
        re.compile(r'Test failed to allocate devices'),
        re.compile(r'\bResult: ERROR\b'),
        re.compile(r'Timeout waiting for \S+ build'),
        re.compile(r'No log files were collected'),
        re.compile(r'Usage: python(?:3)? junit_mini_parser\.py'),
        re.compile(r'CommandException: No URLs matched:'),
        re.compile(r'unexpected disconnect'),
        re.compile(r'early EOF'),
        re.compile(r'invalid index-pack output'),
        re.compile(r'MANEKI_XML_FILES_LISTING_ERROR'),
        re.compile(r'Infrastructure\sfailure', re.VERBOSE),
        re.compile(r'Failed to start docker'),
        re.compile(
            r'storage.objects.get access to the Google Cloud Storage object'),
        re.compile(r'HTTP Error 403: Forbidden'),
        re.compile(
            r'Failed to create decorator ManekiYouTubeLauncherDecorator'),
    ],
    'runner_error': [
        re.compile(
            r'TR\sFATAL\sERROR:\sFor\sinput\sstring:\s"Developer\sBuild"',
            re.VERBOSE),
    ],
    'timeout': [
        re.compile(r'timed\sout', re.VERBOSE),
        re.compile(r'deadline\sexceeded', re.VERBOSE),
        re.compile(r'\bTimeout\b', re.VERBOSE),
    ],
    'config_error': [
        re.compile(r'ERROR at //.*\.gn'),
        re.compile(
            r'UserException:\sAn\serror\shappened\swhile\s'
            r'reading\sand\sparsing\sthe\sbuild\sconfig', re.VERBOSE),
        re.compile(
            r'ConfigException:\sA\sBuild\sfile,\sDockerfile\sor\s'
            r'Transit\scontainer\sbuild\shas\snot\sbeen\sspecified',
            re.VERBOSE),
    ],
    'ninja_error': [
        re.compile(r'FAILED: (?:obj)?/'),
        re.compile(r'err: remote-exec.*failed'),
    ],
    'compilation_error': [
        # GCC/Clang/Java/Kotlin compiler error
        re.compile(r'\b\S+\.(?:cc|c|cpp|h|hpp|cxx|java|kt):\d+:(?:\d+:)?\s*'
                   r'(?:fatal\s+)?error:\s'),
        # MSVC compiler error
        re.compile(r'\b\S+\.(?:cc|c|cpp|h|hpp|cxx)\(\d+\)\s*:\s*'
                   r'(?:fatal\s+)?error\s+C\d+:\s'),
        re.compile(r'undefined reference to'),
        re.compile(r'\bLNK\d+:\s'),
        re.compile(r'failed\sto\scompile', re.VERBOSE),
        re.compile(r'fatal\serror:', re.VERBOSE),
    ],
    'test_failure': [
        re.compile(r'\[\s*FAILED\s*\]', re.VERBOSE),
        re.compile(r'FAIL:\s'),
        re.compile(r'JUnit Failure:'),
        re.compile(r'\bResult: FAIL\b'),
        re.compile(r'(?i:Result:\s*FAIL)', re.VERBOSE),
        re.compile(r'Test\sfailed', re.VERBOSE),
    ],
    'crash_signature': [
        re.compile(r'Segmentation fault'),
        re.compile(r'SIGSEGV'),
        re.compile(r'CRASHED'),
        re.compile(r'Received signal'),
        re.compile(r'Fatal signal \d+'),
        re.compile(r'\*\*\* \*\*\* \*\*\* \*\*\* \*\*\* \*\*\* \*\*\* \*\*\*'),
        re.compile(r'FATAL EXCEPTION:'),
        re.compile(r'Caused by:'),
        re.compile(r'Process \S+ \(pid \d+\) has died'),
    ],
    'dependent_failure': [
        re.compile(
            r'Test\sjobs\sfailed\.\s'
            r'See\sindividual\sfailing\sjobs\sfor\sdetails\.', re.VERBOSE),
    ],
}

PRIORITY = [
    'infra_error',
    'runner_error',
    'timeout',
    'crash_signature',
    'config_error',
    'ninja_error',
    'compilation_error',
    'test_failure',
    'dependent_failure',
]


def check_run_age(created_str: str, event: str,
                  now: datetime.datetime) -> Tuple[bool, str]:
  """Checks if a run is outdated based on creation time and event type."""
  if not created_str:
    return False, 'unknown age'

  try:
    # Handle ISO format with Z
    created_time = datetime.datetime.fromisoformat(
        created_str.replace('Z', '+00:00'))
    if created_time.tzinfo is None:
      created_time = created_time.replace(tzinfo=datetime.timezone.utc)

    age = now - created_time
    if age.days > 0:
      age_str = f'{age.days} day(s) ago'
    else:
      hours = age.seconds // 3600
      age_str = f'{hours} hour(s) ago'

    limit = (
        NIGHTLY_RECENCY_LIMIT
        if event in NIGHTLY_EVENTS else POSTSUBMIT_RECENCY_LIMIT)
    return age > limit, age_str
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Error parsing date {created_str}: {e}', file=sys.stderr)
    return False, 'unknown age'


def _parse_traceback(lines: List[str],
                     start_idx: int) -> Tuple[str, str, int, int]:
  """Parses traceback lines starting from start_idx."""
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


def analyze_log(log_path: str,
                context_lines: int = 5,
                tag: Optional[str] = None,
                required: bool = True) -> List[Dict[str, Any]]:
  """Analyzes a log file for error patterns."""
  if not log_path or not os.path.exists(log_path):
    if required:
      return [{
          'line_num': None,
          'line': f'Log file not found: {log_path}',
          'category': 'infra_error',
          'snippet': '',
          'log_path': log_path,
          'tag': tag
      }]
    else:
      return []

  raw_matches = []
  try:
    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
      lines = f.readlines()

    idx = 0
    while idx < len(lines):
      line = lines[idx]
      line_num = idx + 1

      # Stateful traceback parser (from GHA analyzer)
      if line.startswith('Traceback (most recent call last):'):
        tb_content, tb_exception_line, tb_exception_line_num, next_idx = (
            _parse_traceback(lines, idx))

        # Check if traceback is a test wrapper assertion failure (ignore it)
        if 'cobalt_test_wrapper.py' in tb_content and (
            'AssertionError:' in tb_exception_line and
            '!= 0' in tb_exception_line):
          idx = next_idx
          continue

        raw_matches.append({
            'line_num': tb_exception_line_num,
            'line': tb_exception_line,
            'category': 'crash_signature'
        })
        idx = next_idx
        continue

      # Regular rules
      matched = False
      for category in PRIORITY:
        for regex in RULES[category]:
          if regex.search(line):
            raw_matches.append({
                'line_num': line_num,
                'line': line.strip(),
                'category': category
            })
            matched = True
            break
        if matched:
          break

      idx += 1

  except Exception as e:  # pylint: disable=broad-exception-caught
    return [{
        'line_num': None,
        'line': f'Error reading log file: {e}',
        'category': 'infra_error',
        'snippet': '',
        'log_path': log_path,
        'tag': tag
    }]

  if not raw_matches:
    return [{
        'line_num': None,
        'line': 'No matching error signature found.',
        'category': None,
        'snippet': '',
        'log_path': log_path,
        'tag': tag
    }]

  # Generate matches with snippets
  matches = []
  for m in raw_matches:
    line_num = m['line_num']
    start = max(1, line_num - context_lines)
    end = min(len(lines), line_num + context_lines)
    snippet_lines = [
        f'{l_idx}: {lines[l_idx-1].rstrip()}'
        for l_idx in range(start, end + 1)
    ]
    snippet = '\n'.join(snippet_lines)

    matches.append({
        'line_num': line_num,
        'line': m['line'],
        'category': m['category'],
        'snippet': snippet,
        'log_path': log_path,
        'tag': tag
    })

  return matches


def process_results_data(
    data: Dict[str, Any], now: datetime.datetime
) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
  """Processes a single unified results data dictionary."""
  source = data.get('source') or 'unknown'
  failed_jobs_details = []
  outdated_runs_details = []

  for run in data.get('runs', []):
    branch = run.get('branch', 'unknown')
    event = run.get('event', 'unknown')
    created_at = run.get('createdAt', '')

    is_outdated = False
    age_str = 'ignored age'
    if not run.get('ignore_age'):
      is_outdated, age_str = check_run_age(created_at, event, now)

    if is_outdated:
      conclusion = run.get('conclusion')
      failed_jobs = run.get('failed_jobs', [])

      if conclusion:
        # Treat any non-success outcome as failure (failure, timed_out,
        # cancelled, etc.)
        has_failed = conclusion.lower() not in ('success', 'skipped', 'neutral')
      else:
        # Fallback for backward compatibility
        has_failed = len(failed_jobs) > 0

      if has_failed:
        outdated_runs_details.append({
            'source': source,
            'branch': branch,
            'job_name': run.get('job_name'),
            'run_id': run.get('run_id'),
            'age': age_str,
            'url': run.get('url'),
            'event': event,
        })
      continue

    for job in run.get('failed_jobs', []):
      log_path = job.get('local_log_path')
      log_type = job.get('log_type', f'{source}_log')
      system_log_path = job.get('device_system_log_path')
      device_logs_status = job.get('device_logs_status')

      matches = []
      if log_path:
        matches.extend(analyze_log(log_path, tag=log_type, required=True))
      else:
        matches.append({
            'line_num': None,
            'line': 'Log file not downloaded.',
            'category': 'infra_error',
            'snippet': '',
            'log_path': '',
            'tag': log_type
        })

      if system_log_path:
        matches.extend(
            analyze_log(
                system_log_path, tag='device_system_log', required=False))

      failed_jobs_details.append({
          'source': source,
          'branch': branch,
          'run_id': run.get('run_id'),
          'run_name': run.get('job_name'),
          'run_url': run.get('url'),
          'job_name': job.get('name'),
          'job_url': job.get('url'),
          'local_log_path': log_path,
          'device_system_log_path': system_log_path,
          'device_logs_status': device_logs_status,
          'time': created_at,
          'matches': matches,
          'log_downloaded': bool(log_path and os.path.exists(log_path)),
          'device_log_downloaded': bool(system_log_path and os.path.exists(system_log_path)),
      })

  return failed_jobs_details, outdated_runs_details


def branch_sort_key(branch_name: str) -> tuple:
  """Sort key for branch names to order them by version descending.

  Sort order:
  1. main / master (Priority 1)
  2. Versioned branches starting with digits (Priority 0), sorted by version.
  3. Other branches (Priority -1), sorted alphabetically.
  """
  if branch_name in ('master', 'main'):
    return (1, (float('inf'), '', float('inf')), branch_name)

  match = re.match(r'^(\d+)\.(\w+)(?:\.(\d+))?', branch_name)
  if match:
    major = int(match.group(1))
    track = match.group(2)
    minor = int(match.group(3)) if match.group(3) else 0
    return (0, (major, track, minor), branch_name)

  match_cobalt = re.match(r'^COBALT_(\d+)', branch_name)
  if match_cobalt:
    major = int(match_cobalt.group(1))
    return (0, (major, 'cobalt', 0), branch_name)

  return (-1, (0, '', 0), branch_name)


def generate_report(failed_jobs: List[Dict[str, Any]],
                    outdated_runs: List[Dict[str, Any]],
                    total_fetched: Dict[str, int]) -> str:
  """Generates the unified markdown report."""
  report = []
  report.append('# Unified CI Build Status Triage Report\n')

  report.append('## Job Stats')

  # Group stats by source
  sources = set(total_fetched.keys())
  for job in failed_jobs:
    sources.add(job['source'])
  for run in outdated_runs:
    sources.add(run['source'])

  for src in sorted(sources):
    src_failed_jobs = [j for j in failed_jobs if j['source'] == src]
    src_outdated_runs = [r for r in outdated_runs if r['source'] == src]

    # Count downloaded logs
    logs_downloaded = 0
    for job in src_failed_jobs:
      if job.get('log_downloaded'):
        logs_downloaded += 1
      if job.get('device_log_downloaded'):
        logs_downloaded += 1

    report.append(f'### Source: {src.upper()}')
    report.append(f'*   **Total Jobs Fetched**: {total_fetched.get(src, 0)}')
    report.append(f'*   **Failed Jobs (Recent)**: {len(src_failed_jobs)}')
    report.append(f'*   **Outdated Failed Runs**: {len(src_outdated_runs)}')
    report.append(f'*   **Log Files Downloaded**: {logs_downloaded}\n')


  # Group by branch
  branches = {}
  for job in failed_jobs:
    branch = job['branch']
    if branch not in branches:
      branches[branch] = {'failed': [], 'outdated': []}
    branches[branch]['failed'].append(job)

  for run in outdated_runs:
    branch = run['branch']
    if branch not in branches:
      branches[branch] = {'failed': [], 'outdated': []}
    branches[branch]['outdated'].append(run)

  report.append('## Branch Health Report\n')
  for branch in sorted(branches.keys(), key=branch_sort_key, reverse=True):
    b_data = branches[branch]
    failed_count = len(b_data['failed'])
    outdated_count = len(b_data['outdated'])

    if failed_count == 0 and outdated_count == 0:
      report.append(f'### Branch: {branch} (Healthy)')
      report.append('*   All runs completed successfully.\n')
    elif failed_count == 0 and outdated_count > 0:
      report.append(f'### Branch: {branch} (Outdated Failures)')
      report.append(
          f'*   No recent failures, but {outdated_count} outdated failed '
          'run(s) exist (retrigger suggested).\n')
    else:
      report.append(f'### Branch: {branch} (Unhealthy)')
      report.append(f'*   **Failed Jobs**: {failed_count}')
      if outdated_count > 0:
        report.append(f'*   **Outdated Failed Runs**: {outdated_count} '
                      '(retrigger suggested)')
      report.append('')

  report.append('## Detailed Branch Failures\n')
  for branch in sorted(branches.keys(), key=branch_sort_key, reverse=True):
    b_data = branches[branch]
    if not b_data['failed'] and not b_data['outdated']:
      continue

    report.append(f'### Branch: {branch}\n')

    if b_data['failed']:
      # Group by Run
      runs = {}
      for job in b_data['failed']:
        run_key = (job['run_name'], job['run_id'], job['run_url'],
                   job['source'])
        if run_key not in runs:
          runs[run_key] = []
        runs[run_key].append(job)

      for run_key, jobs in runs.items():
        run_name, run_id, run_url, source = run_key
        report.append(f'#### Run: {run_name} (ID: {run_id}) [{source.upper()}]')
        report.append(f'*   **URL**: {run_url}')
        report.append('*   **Failed Jobs**:')

        for job in jobs:
          job_name = job['job_name']
          job_url = job['job_url']
          local_log_path = job['local_log_path']
          report.append(f'    *   **Job**: {job_name}')
          report.append(f'        *   **URL**: {job_url}')
          report.append(f'        *   **Cached Log**: `{local_log_path}`')
          device_system_log_path = job.get('device_system_log_path')
          if device_system_log_path:
            report.append(
                f'        *   **Device System Log**: `{device_system_log_path}`'
            )
          elif job.get('device_logs_status') == 'MISSING':
            report.append('        *   **Device System Log**: MISSING')
          report.append('        *   **Error Location(s)**:')

          for m in job['matches']:
            m_tag = m.get('tag')
            m_category = m.get('category')
            tag_str = f'[{m_tag}] ' if m_tag else ''
            cat_str = f'[{m_category}] ' if m_category else ''
            m_line_num = m['line_num']
            m_line = m['line']
            m_log_path = m['log_path']
            if m_line_num is not None:
              report.append(
                  f'            *   {tag_str}{cat_str}Line {m_line_num}: '
                  f'`{m_line}`')
              # Log slice command
              start_line = max(1, m_line_num - 5)
              report.append(
                  f'                *   Log Slice Command: '
                  f'`tail -n +{start_line} "{m_log_path}" | head -n 11`')
            else:
              report.append(f'            *   {tag_str}{cat_str}`{m_line}`')
          report.append('')

    if b_data['outdated']:
      report.append('#### Outdated Failed Runs (Retrigger Suggested)\n')
      for run in b_data['outdated']:
        run_job_name = run['job_name']
        run_id = run['run_id']
        run_event = run['event']
        run_source = run['source'].upper()
        run_age = run['age']
        run_url = run['url']
        report.append(
            f'*   **{run_job_name}** (ID: {run_id}, Event: {run_event}) '
            f'[{run_source}] - Failed {run_age}.')
        report.append(f'    *   URL: {run_url}\n')

  return '\n'.join(report)


def main():
  parser = argparse.ArgumentParser(description='Unified CI Results Analyzer')
  parser.add_argument(
      '--incoming-dir',
      required=True,
      help='Directory containing results JSON files.')
  parser.add_argument(
      '--output', required=True, help='Path to write the markdown report.')
  args = parser.parse_args()

  if not os.path.isdir(args.incoming_dir):
    print(
        f'Error: Incoming directory not found: {args.incoming_dir}',
        file=sys.stderr)
    sys.exit(1)

  now = datetime.datetime.now(datetime.timezone.utc)
  all_failed_jobs = []
  all_outdated_runs = []
  total_fetched = {}

  for filename in os.listdir(args.incoming_dir):
    if filename.endswith(
        '.json') and filename != 'github_jobs.json':  # Skip temp files if any
      file_path = os.path.join(args.incoming_dir, filename)
      try:
        with open(file_path, 'r', encoding='utf-8') as f:
          data = json.load(f)
        source = data.get('source') or 'unknown'
        total_fetched[source] = (
            total_fetched.get(source, 0)
            + data.get('total_jobs_fetched', 0)
        )

        failed, outdated = process_results_data(data, now)
        all_failed_jobs.extend(failed)
        all_outdated_runs.extend(outdated)
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(f'Error processing file {filename}: {e}', file=sys.stderr)

  report = generate_report(all_failed_jobs, all_outdated_runs, total_fetched)

  try:
    with open(args.output, 'w', encoding='utf-8') as f:
      f.write(report)
    print(f'Successfully wrote report to {args.output}')
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Error writing report: {e}', file=sys.stderr)
    sys.exit(1)


if __name__ == '__main__':
  main()
