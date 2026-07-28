#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
# TAG=agy
"""Discovers recent GHA runs, downloads logs, and outputs in unified schema."""

import argparse
import concurrent.futures
import datetime
import getpass
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
import urllib.parse
import xml.etree.ElementTree as ET

NIGHTLY_RECENCY_LIMIT = datetime.timedelta(hours=24)
POSTSUBMIT_RECENCY_LIMIT = datetime.timedelta(days=7)
NIGHTLY_EVENTS = ('schedule', 'workflow_dispatch')


def check_run_age(run, now):
  """Checks if a GHA run is outdated based on its creation time and event type.

  Args:
    run: A dict representing the run, containing 'createdAt' and optionally
      'event'.
    now: A timezone-aware datetime representing the current time.

  Returns:
    A tuple of (is_outdated, age_str).
      - is_outdated: True if the run age exceeds the limit for its event type.
      - age_str: A string describing the age (e.g., "5 day(s) ago" or "unknown
      age").
  """
  if run.get('ignore_age'):
    return False, 'ignored age'

  created_str = run.get('createdAt')
  event = run.get('event', 'push')

  if not created_str:
    return False, 'unknown age'

  try:
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


def run_gh_command(args):
  """Runs a gh CLI command and returns its stdout.

  Args:
    args: A list of arguments for the gh command.

  Returns:
    The stdout of the command as a string.

  Raises:
    RuntimeError: If the command returns a non-zero exit code.
  """
  cmd = ['gh'] + args
  result = subprocess.run(cmd, capture_output=True, text=True, check=False)
  if result.returncode != 0:
    print(
        f"Error running gh command {' '.join(cmd)}: {result.stderr}",
        file=sys.stderr,
    )
    raise RuntimeError(f'gh command failed: {result.stderr}')
  return result.stdout


DEFAULT_REPOSITORY = 'youtube/cobalt'

# ==========================================
# Discovery Logic (from github_discover.py)
# ==========================================


def parse_build_status(file_path):
  """Parses a Cobalt build status markdown file for workflow triggers."""
  workflows = []
  repo_pattern = re.escape(DEFAULT_REPOSITORY)
  pattern = re.compile(r'^\[[^\]]+\]:\s*https://github\.com/' + repo_pattern +
                       r'/actions/workflows/(?P<workflow>[^/]+)/badge\.svg'
                       r'\?event=(?P<event>[^&]+)'
                       r'&branch=(?P<branch>[^\s\)]+)')
  if not os.path.exists(file_path):
    print(f'Warning: {file_path} not found.', file=sys.stderr)
    return workflows

  with open(file_path, 'r', encoding='utf-8') as f:
    for line in f:
      match = pattern.match(line)
      if match:
        workflow = match.group('workflow')
        event = match.group('event')
        branch = urllib.parse.unquote(match.group('branch'))

        entry = {'workflow': workflow, 'event': event, 'branch': branch}
        if entry not in workflows:
          workflows.append(entry)
  return workflows


def discover_single_run(workflow_config, limit):
  """Queries GHA API for a single run of a workflow."""
  workflow = workflow_config['workflow']
  branch = workflow_config['branch']
  event = workflow_config['event']
  try:
    args = [
        'run',
        'list',
        '--workflow',
        workflow,
        '--branch',
        branch,
        '--event',
        event,
        '--limit',
        str(limit),
        '--status',
        'completed',
        '--json',
        'databaseId,status,conclusion,url,createdAt',
        '-R',
        DEFAULT_REPOSITORY,
    ]
    stdout = run_gh_command(args)
    runs = json.loads(stdout)
    if not runs:
      return None, 0

    run_data = runs[0]
    run_data['event'] = event  # Inject event for check_run_age
    run_id = run_data['databaseId']
    conclusion = run_data.get('conclusion')

    now = datetime.datetime.now(datetime.timezone.utc)
    is_outdated, _ = check_run_age(run_data, now)

    # Optimization: Skip job details if the run is outdated OR succeeded
    if is_outdated or conclusion == 'success':
      return {
          'run_id': str(run_id),
          'job_name': workflow,
          'branch': branch,
          'event': event,
          'createdAt': run_data.get('createdAt'),
          'url': run_data.get('url'),
          'conclusion': conclusion,
          'failed_jobs': [],
      }, 0

    jobs_stdout = run_gh_command([
        'run', 'view',
        str(run_id), '--json', 'jobs', '-R', DEFAULT_REPOSITORY
    ])
    jobs_data = json.loads(jobs_stdout)
    jobs = jobs_data.get('jobs', [])

    failed_jobs = []
    for job in jobs:
      if job.get('conclusion') == 'failure':
        failed_jobs.append({
            'job_id': job.get('databaseId'),
            'name': job.get('name'),
            'url': job.get('url'),
        })

    return {
        'run_id': str(run_id),  # Convert to string for schema consistency
        'job_name':
            workflow,  # Map workflow_name to job_name for unified schema
        'branch': branch,
        'event': event,
        'createdAt': run_data.get('createdAt'),
        'url': run_data.get('url'),
        'conclusion': conclusion,
        'failed_jobs': failed_jobs,
    }, len(jobs)

  except Exception as e:  # pylint: disable=broad-exception-caught
    print(
        f'Failed to fetch runs for {workflow} on {branch} ({event}): {e}',
        file=sys.stderr,
    )
    return None, 0


def discover_runs(workflows, limit=1):
  """Discovers runs for a list of workflows in parallel."""
  discovered_runs = []
  total_jobs_fetched = 0

  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = {
        executor.submit(discover_single_run, workflow_config, limit):
            workflow_config for workflow_config in workflows
    }
    for future in concurrent.futures.as_completed(futures):
      run_info, jobs_count = future.result()
      total_jobs_fetched += jobs_count
      if run_info:
        discovered_runs.append(run_info)

  discovered_runs.sort(key=lambda x: (x['job_name'], x['branch'], x['event']))

  return {'total_jobs_fetched': total_jobs_fetched, 'runs': discovered_runs}


def discover_run_by_id(run_id):
  """Discovers a run by its GHA run ID."""
  try:
    args = [
        'run',
        'view',
        str(run_id),
        '--json',
        'databaseId,workflowName,headBranch,event,createdAt,url,jobs,'
        'conclusion',
        '-R',
        DEFAULT_REPOSITORY,
    ]
    stdout = run_gh_command(args)
    run_data = json.loads(stdout)

    jobs = run_data.get('jobs', [])
    failed_jobs = []
    for job in jobs:
      if job.get('conclusion') == 'failure':
        failed_jobs.append({
            'job_id': job.get('databaseId'),
            'name': job.get('name'),
            'url': job.get('url'),
        })

    run_info = {
        'run_id': str(run_data.get('databaseId')),  # String for consistency
        'job_name': run_data.get('workflowName'),  # Map to job_name
        'branch': run_data.get('headBranch'),
        'event': run_data.get('event'),
        'createdAt': run_data.get('createdAt'),
        'url': run_data.get('url'),
        'conclusion': run_data.get('conclusion'),
        'failed_jobs': failed_jobs,
        'ignore_age': True,
    }

    return {'total_jobs_fetched': len(jobs), 'runs': [run_info]}
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Failed to fetch run by ID {run_id}: {e}', file=sys.stderr)
    return {'total_jobs_fetched': 0, 'runs': []}


# ==========================================
# Download Logic (from github_download.py)
# ==========================================


def parse_junit_xml(xml_path):
  """Parses a JUnit XML file to extract test failures."""
  failures = []
  try:
    tree = ET.parse(xml_path)
    root = tree.getroot()
    testsuites = root.findall('.//testsuite')
    if not testsuites and root.tag == 'testsuite':
      testsuites = [root]

    for testsuite in testsuites:
      for testcase in testsuite.findall('.//testcase'):
        failure = testcase.find('failure')
        if failure is not None:
          failures.append({
              'testsuite': testsuite.get('name'),
              'testcase': testcase.get('name'),
              'message': failure.get('message'),
              'details': failure.text,
          })
        error = testcase.find('error')
        if error is not None:
          failures.append({
              'testsuite': testsuite.get('name'),
              'testcase': testcase.get('name'),
              'message': error.get('message'),
              'details': error.text,
          })
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Error parsing JUnit XML {xml_path}: {e}', file=sys.stderr)
  return failures


def write_synthetic_log(failures, dest_path):
  """Writes a synthetic log for test failures."""
  os.makedirs(os.path.dirname(dest_path), exist_ok=True)
  temp_dest = dest_path + '.tmp'
  try:
    with open(temp_dest, 'w', encoding='utf-8') as f:
      for fail in failures:
        f.write(f"JUnit Failure: {fail['testsuite']}.{fail['testcase']}\n")
        if fail['message']:
          f.write(f"Message: {fail['message']}\n")
        if fail['details']:
          f.write(f"Details:\n{fail['details']}\n")
        f.write('-' * 40 + '\n')
    os.replace(temp_dest, dest_path)
  except Exception as e:
    print(f'Failed to write synthetic log to {dest_path}: {e}', file=sys.stderr)
    if os.path.exists(temp_dest):
      try:
        os.remove(temp_dest)
      except Exception:  # pylint: disable=broad-exception-caught
        pass
    raise


def download_job_log(job_id, dest_path):
  """Downloads the log of a job."""
  os.makedirs(os.path.dirname(dest_path), exist_ok=True)
  temp_dest = dest_path + '.tmp'
  try:
    stdout = run_gh_command(
        ['api', f'repos/{DEFAULT_REPOSITORY}/actions/jobs/{job_id}/logs'])
    with open(temp_dest, 'w', encoding='utf-8') as f:
      f.write(stdout)
    os.replace(temp_dest, dest_path)
    return True
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Failed to download log for job {job_id}: {e}', file=sys.stderr)
    if os.path.exists(temp_dest):
      try:
        os.remove(temp_dest)
      except Exception:  # pylint: disable=broad-exception-caught
        pass
    return False


def download_test_results(run_id, dest_dir):
  """Downloads test results from GHA runs."""
  os.makedirs(dest_dir, exist_ok=True)
  try:
    run_gh_command([
        'run',
        'download',
        str(run_id),
        '-p',
        '*_test_results*',
        '-D',
        dest_dir,
        '-R',
        DEFAULT_REPOSITORY,
    ])
    return True
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(
        f'No test results artifacts downloaded for run {run_id} '
        f'(might not exist): {e}',
        file=sys.stderr,
    )
    return False


def _determine_cached_log_type(log_path, system_log_path):
  """Determines the log type of a cached log file."""
  try:
    with open(log_path, 'r', encoding='utf-8') as f:
      first_line = f.readline()
      if first_line.startswith('JUnit Failure:'):
        return 'synthetic'
  except Exception:  # pylint: disable=broad-exception-caught
    pass
  if os.path.exists(system_log_path):
    return 'test_log'
  return 'gha_log'


def _extract_logs_from_artifacts(run_temp_dir, platform, target_name, *,
                                 dest_log_path, dest_system_log_path, job):
  """Extracts target-specific logs and system logs from run artifacts."""
  found_logs = False
  if platform:
    log_pattern = os.path.join(run_temp_dir, f'**/{platform}',
                               f'**/{target_name}_log.txt')
    system_log_patterns = [
        os.path.join(run_temp_dir, f'**/{platform}',
                     f'**/{target_name}_device_logcat.txt'),
        os.path.join(run_temp_dir, f'**/{platform}',
                     f'**/{target_name}_device_log.txt'),
    ]
  else:
    log_pattern = os.path.join(run_temp_dir, f'**/{target_name}_log.txt')
    system_log_patterns = [
        os.path.join(run_temp_dir, f'**/{target_name}_device_logcat.txt'),
        os.path.join(run_temp_dir, f'**/{target_name}_device_log.txt'),
    ]

  log_files = glob.glob(log_pattern, recursive=True)
  if log_files:
    temp_dest = dest_log_path + '.tmp'
    try:
      shutil.copy(log_files[0], temp_dest)
      os.replace(temp_dest, dest_log_path)
      job['local_log_path'] = dest_log_path
      job['log_type'] = 'test_log'
      found_logs = True
      print(
          f'Found test log for {target_name} in artifact: {log_files[0]}',
          file=sys.stderr,
      )
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'Failed to copy test log: {e}', file=sys.stderr)
      if os.path.exists(temp_dest):
        try:
          os.remove(temp_dest)
        except Exception:  # pylint: disable=broad-exception-caught
          pass

  for pattern in system_log_patterns:
    system_log_files = glob.glob(pattern, recursive=True)
    if system_log_files:
      temp_system_dest = dest_system_log_path + '.tmp'
      try:
        shutil.copy(system_log_files[0], temp_system_dest)
        os.replace(temp_system_dest, dest_system_log_path)
        job['device_system_log_path'] = dest_system_log_path
        print(
            f'Found device system log for {target_name} in artifact: '
            f'{system_log_files[0]}',
            file=sys.stderr,
        )
        break
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(f'Failed to copy system log: {e}', file=sys.stderr)
        if os.path.exists(temp_system_dest):
          try:
            os.remove(temp_system_dest)
          except Exception:  # pylint: disable=broad-exception-caught
            pass

  return found_logs


def parse_job_name(job_name):
  """Parses job name to determine if it is a test job and extract details."""
  target_name = None
  if ':' in job_name:
    target_name = job_name.split(':')[-1].strip()

  platform = None
  if '/' in job_name:
    platform = job_name.split('/')[0].strip()

  is_test = ('test' in job_name.lower() or 'results' in job_name.lower() or
             target_name is not None)
  return is_test, platform, target_name


def is_job_cached(job_id, cache_dir):
  """Checks if a job log is already cached."""
  dest_log_path = os.path.join(cache_dir, f'{job_id}.log')
  return os.path.exists(dest_log_path) and os.path.getsize(dest_log_path) > 0


def process_job(job, cache_dir, run_temp_dir):
  """Processes a single GHA job, downloading logs."""
  job_id = job['job_id']
  job_name = job['name']

  dest_log_path = os.path.join(cache_dir, f'{job_id}.log')
  dest_system_log_path = os.path.join(cache_dir, f'{job_id}_system_log.txt')

  if is_job_cached(job_id, cache_dir):
    print(
        f'Log file already exists in cache for job {job_name} ({job_id}). '
        f'Skipping download.',
        file=sys.stderr,
    )
    job['local_log_path'] = dest_log_path
    if os.path.exists(dest_system_log_path):
      job['device_system_log_path'] = dest_system_log_path
    job['log_type'] = _determine_cached_log_type(dest_log_path,
                                                 dest_system_log_path)
    return

  is_test_job, platform, target_name = parse_job_name(job_name)

  if is_test_job and run_temp_dir:
    print(
        f'Job {job_name} ({job_id}) classified as Test Job. '
        f'Looking in artifacts...',
        file=sys.stderr,
    )
    found_logs = False
    if target_name:
      found_logs = _extract_logs_from_artifacts(
          run_temp_dir,
          platform,
          target_name,
          dest_log_path=dest_log_path,
          dest_system_log_path=dest_system_log_path,
          job=job,
      )

    if not found_logs:
      if target_name:
        xml_pattern = os.path.join(run_temp_dir,
                                   f'**/{target_name}_testoutput.xml')
      else:
        xml_pattern = os.path.join(run_temp_dir, '**/*.xml')
      xml_files = glob.glob(xml_pattern, recursive=True)
      failures = []
      for xml_file in xml_files:
        failures.extend(parse_junit_xml(xml_file))

      if failures:
        print(
            f'Found {len(failures)} failures in JUnit XMLs. '
            f'Writing synthetic log.',
            file=sys.stderr,
        )
        write_synthetic_log(failures, dest_log_path)
        job['local_log_path'] = dest_log_path
        job['log_type'] = 'synthetic'
        found_logs = True

    if found_logs:
      return
    else:
      print(
          f'No logs found in artifacts for job {job_name} ({job_id}).',
          file=sys.stderr)
      job['device_logs_status'] = 'MISSING'

  elif is_test_job and not run_temp_dir:
    print(
        f'Job {job_name} ({job_id}) is a Test Job but no artifacts downloaded.',
        file=sys.stderr)
    job['device_logs_status'] = 'MISSING'

  print(
      f'Downloading full GHA log for job {job_name} ({job_id})...',
      file=sys.stderr)
  if download_job_log(job_id, dest_log_path):
    job['local_log_path'] = dest_log_path
    job['log_type'] = 'gha_log'
  else:
    job['local_log_path'] = ''
    job['log_type'] = ''


# ==========================================
# Main Execution
# ==========================================


def process_run(run_id, run_data, cache_dir):
  """Processes a single run, downloading artifacts if needed."""
  need_artifacts = False
  for job in run_data['jobs']:
    is_test, _, _ = parse_job_name(job['name'])
    if is_test and not is_job_cached(job['job_id'], cache_dir):
      need_artifacts = True
      break

  if need_artifacts:
    print(f'Downloading artifacts for run {run_id}...', file=sys.stderr)
    with tempfile.TemporaryDirectory() as temp_dir_name:
      download_success = download_test_results(run_id, temp_dir_name)
      run_temp_dir = temp_dir_name if download_success else None
      for job in run_data['jobs']:
        process_job(job, cache_dir, run_temp_dir)
  else:
    print(
        f'Skipping artifact download for run {run_id} (no uncached failed'
        ' test jobs)',
        file=sys.stderr,
    )
    for job in run_data['jobs']:
      process_job(job, cache_dir, None)


def main():
  parser = argparse.ArgumentParser(
      description='Discover failed GHA runs and download their logs.')
  parser.add_argument(
      '--build-status',
      default='cobalt/BUILD_STATUS.md',
      help='Path to BUILD_STATUS.md (used if not --run-id)',
  )
  parser.add_argument(
      '--run-id',
      type=int,
      help='Specific run ID to triage.',
  )
  parser.add_argument(
      '--output',
      required=True,
      help='Path to write the results JSON.',
  )
  parser.add_argument(
      '--cache-dir',
      help=(
          'Path to store cached job logs. Defaults to'
          ' ~/.cache/github_gardener_{user}'
      ),
  )
  args = parser.parse_args()

  # 1. Discover
  if args.run_id:
    print(f'Discovering run ID {args.run_id} via gh CLI...', file=sys.stderr)
    data = discover_run_by_id(args.run_id)
  else:
    build_status_path = args.build_status
    if not os.path.exists(build_status_path):
      print(f'Error: {build_status_path} not found.', file=sys.stderr)
      sys.exit(1)

    print(f'Parsing build status from {build_status_path}...', file=sys.stderr)
    workflows = parse_build_status(build_status_path)
    print(
        f'Found {len(workflows)} active workflow configurations.',
        file=sys.stderr)
    print('Discovering latest completed runs via gh CLI...', file=sys.stderr)
    data = discover_runs(workflows)

  # Add source to root
  data['source'] = 'github'

  # 2. Download
  username = getpass.getuser()
  default_cache_dir = os.path.expanduser(
      os.path.join('~/.cache', f'github_gardener_{username}'))
  cache_dir = args.cache_dir or default_cache_dir
  os.makedirs(cache_dir, exist_ok=True)
  print(f'Using cache directory: {cache_dir}', file=sys.stderr)

  now = datetime.datetime.now(datetime.timezone.utc)
  run_tasks = {}
  for run in data.get('runs', []):
    run_id = run['run_id']
    # check_run_age expects 'createdAt' and 'event' (which are present)
    # We can use check_run_age from unified_analyzer, but to keep this
    # script independent of the triage skill code at runtime, we can
    # implement a simple version or import it.
    is_outdated, age_str = check_run_age(run, now)
    if is_outdated:
      print(
          f"Skipping download for outdated run {run_id} - run is {age_str}",
          file=sys.stderr)
      continue
    run_tasks[run_id] = {'run': run, 'jobs': run.get('failed_jobs', [])}

  print('Processing runs...', file=sys.stderr)
  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = [
        executor.submit(process_run, run_id, run_data, cache_dir)
        for run_id, run_data in run_tasks.items()
    ]
    for future in concurrent.futures.as_completed(futures):
      try:
        future.result()
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(f'Exception in process_run: {e}', file=sys.stderr)
        traceback.print_exc(file=sys.stderr)

  # Ensure output directory exists
  os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

  # Write output
  with open(args.output, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)

  print(f'Successfully wrote results to {args.output}', file=sys.stderr)


if __name__ == '__main__':
  main()
# test
