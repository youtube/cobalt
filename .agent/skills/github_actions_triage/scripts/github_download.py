#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Downloads logs and test results for a GHA run."""

import argparse
import concurrent.futures
import datetime
import getpass
import glob
import json
import os
import shutil
import sys
import tempfile
import traceback
import xml.etree.ElementTree as ET

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
if _scripts_dir not in sys.path:
  sys.path.append(_scripts_dir)
# pylint: disable=wrong-import-position
import gardener_utils

DEFAULT_REPOSITORY = 'youtube/cobalt'


def parse_junit_xml(xml_path):
  """Parses a JUnit XML file to extract test failures.

  Args:
    xml_path: Path to the XML file.

  Returns:
    A list of dicts representing test failures.
  """
  failures = []
  try:
    tree = ET.parse(xml_path)
    root = tree.getroot()
    # JUnit XML can have <testsuites> or directly <testsuite> at root
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
  """Writes a synthetic log for test failures.

  Args:
    failures: List of dicts representing failures.
    dest_path: Destination path for the log.
  """
  os.makedirs(os.path.dirname(dest_path), exist_ok=True)
  with open(dest_path, 'w', encoding='utf-8') as f:
    for fail in failures:
      f.write(f"JUnit Failure: {fail['testsuite']}.{fail['testcase']}\n")
      if fail['message']:
        f.write(f"Message: {fail['message']}\n")
      if fail['details']:
        f.write(f"Details:\n{fail['details']}\n")
      f.write('-' * 40 + '\n')


def download_job_log(job_id, dest_path):
  """Downloads the log of a job.

  Args:
    job_id: ID of the job to download.
    dest_path: Location to write the log.

  Returns:
    True if successfully written.
  """
  os.makedirs(os.path.dirname(dest_path), exist_ok=True)
  try:
    # gh api repos/youtube/cobalt/actions/jobs/<job_id>/logs returns the log
    # content directly
    stdout = gardener_utils.run_gh_command(
        ['api', f'repos/{DEFAULT_REPOSITORY}/actions/jobs/{job_id}/logs'])
    with open(dest_path, 'w', encoding='utf-8') as f:
      f.write(stdout)
    return True
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Failed to download log for job {job_id}: {e}', file=sys.stderr)
    return False


def download_test_results(run_id, dest_dir):
  """Downloads test results from GHA runs.

  Args:
    run_id: ID of the run.
    dest_dir: Where to save.

  Returns:
    True if successfully downloaded.
  """
  os.makedirs(dest_dir, exist_ok=True)
  try:
    # Download artifacts matching *_test_results*
    # Note: gh run download might fail if no matching artifacts are found,
    # which is expected for some runs.
    gardener_utils.run_gh_command([
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
        f'No test results artifacts downloaded for run {run_id} (might not'
        f' exist): {e}',
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


# pylint: disable=too-many-positional-arguments
def _extract_logs_from_artifacts(run_temp_dir, platform, target_name,
                                 dest_log_path, dest_system_log_path, job):
  """Extracts target-specific logs and system logs from run artifacts.

  Args:
    run_temp_dir: Directory where run artifacts are downloaded.
    platform: Platform name (e.g. 'android-arm64').
    target_name: Target name (e.g. 'skia_unittests').
    dest_log_path: Path where target log should be copied.
    dest_system_log_path: Path where system log should be copied.
    job: Job metadata dict to update.

  Returns:
    True if at least one log was found and copied, False otherwise.
  """
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

  # Search for target-specific log
  log_files = glob.glob(log_pattern, recursive=True)
  if log_files:
    shutil.copy(log_files[0], dest_log_path)
    job['local_log_path'] = dest_log_path
    job['log_type'] = 'test_log'
    found_logs = True
    print(
        f'Found test log for {target_name} in artifact: {log_files[0]}',
        file=sys.stderr,
    )

  # Search for device system log
  for pattern in system_log_patterns:
    system_log_files = glob.glob(pattern, recursive=True)
    if system_log_files:
      shutil.copy(system_log_files[0], dest_system_log_path)
      job['device_system_log_path'] = dest_system_log_path
      print(
          f'Found device system log for {target_name} in artifact:'
          f' {system_log_files[0]}',
          file=sys.stderr,
      )
      break

  return found_logs


def process_job(job, cache_dir, run_temp_dir):
  """Processes a single GHA job.

  Args:
    job: Job metadata dict to update with local log path.
    cache_dir: Directory where logs should be stored.
    run_temp_dir: Directory where run artifacts are downloaded, or None.
  """
  job_id = job['job_id']
  job_name = job['name']

  dest_log_path = os.path.join(cache_dir, f'{job_id}.log')
  dest_system_log_path = os.path.join(cache_dir, f'{job_id}_system_log.txt')

  if os.path.exists(dest_log_path) and os.path.getsize(dest_log_path) > 0:
    print(
        f'Log file already exists in cache for job {job_name} ({job_id}).'
        ' Skipping download.',
        file=sys.stderr,
    )
    job['local_log_path'] = dest_log_path
    # Also check if system log exists in cache
    if os.path.exists(dest_system_log_path):
      job['device_system_log_path'] = dest_system_log_path
    job['log_type'] = _determine_cached_log_type(dest_log_path,
                                                 dest_system_log_path)
    return

  # Extract target name and platform if possible
  target_name = None
  if ':' in job_name:
    target_name = job_name.split(':')[-1].strip()

  platform = None
  if '/' in job_name:
    platform = job_name.split('/')[0].strip()

  is_test_job = 'test' in job_name.lower() or 'results' in job_name.lower(
  ) or target_name is not None

  if is_test_job and run_temp_dir:
    print(
        f'Job {job_name} ({job_id}) classified as Test Job '
        f'(target: {target_name}, platform: {platform}).'
        ' Looking for logs in downloaded artifacts...',
        file=sys.stderr,
    )
    found_logs = False
    if target_name:
      found_logs = _extract_logs_from_artifacts(run_temp_dir, platform,
                                                target_name, dest_log_path,
                                                dest_system_log_path, job)

    if not found_logs:
      # Fallback to old behavior: parse all XMLs and write synthetic log
      xml_pattern = os.path.join(
          run_temp_dir,
          f'**/{target_name}_testoutput.xml') if target_name else os.path.join(
              run_temp_dir, '**/*.xml')
      xml_files = glob.glob(xml_pattern, recursive=True)
      failures = []
      for xml_file in xml_files:
        failures.extend(parse_junit_xml(xml_file))

      if failures:
        print(
            f'Found {len(failures)} failures in JUnit XMLs. '
            'Writing synthetic log.',
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
          f'No logs or failures found in artifacts for job '
          f'{job_name} ({job_id}).'
          ' Marking device logs as MISSING.',
          file=sys.stderr,
      )
      job['device_logs_status'] = 'MISSING'

  elif is_test_job and not run_temp_dir:
    print(
        f'Job {job_name} ({job_id}) is a Test Job but '
        'no artifacts were downloaded.'
        ' Marking device logs as MISSING.',
        file=sys.stderr,
    )
    job['device_logs_status'] = 'MISSING'

  # Fallback to full log download (GHA job log)
  print(
      f'Downloading full GHA log for job {job_name} ({job_id})...',
      file=sys.stderr)
  if download_job_log(job_id, dest_log_path):
    job['local_log_path'] = dest_log_path
    job['log_type'] = 'gha_log'
  else:
    job['local_log_path'] = ''
    job['log_type'] = ''


def main():
  parser = argparse.ArgumentParser(
      description='Download logs and test results for discovered failed jobs.')
  parser.add_argument('github_json', help='Path to github_jobs.json')
  parser.add_argument(
      '--output',
      default='github_results.json',
      help='Path to write the results JSON.',
  )
  args = parser.parse_args()

  if not os.path.exists(args.github_json):
    print(f'Error: {args.github_json} not found.', file=sys.stderr)
    sys.exit(1)

  try:
    with open(args.github_json, 'r', encoding='utf-8') as f:
      data = json.load(f)
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Error reading JSON: {e}', file=sys.stderr)
    sys.exit(1)

  username = getpass.getuser()
  cache_dir = os.path.join('/tmp', f'github_gardener_{username}')
  os.makedirs(cache_dir, exist_ok=True)
  print(f'Using cache directory: {cache_dir}', file=sys.stderr)

  now = datetime.datetime.now(datetime.timezone.utc)
  # Group jobs by run_id
  run_tasks = {}
  for run in data.get('runs', []):
    run_id = run['run_id']
    is_outdated, age_str = gardener_utils.check_run_age(run, now)
    if is_outdated:
      print(
          f'Skipping download for outdated run {run_id}'
          f" ({run.get('workflow_name')}) - run is {age_str}",
          file=sys.stderr,
      )
      continue
    run_tasks[run_id] = {'run': run, 'jobs': run.get('failed_jobs', [])}

  temp_dirs = {}
  futures = []

  try:
    print('Processing runs...', file=sys.stderr)
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
      for run_id, run_data in run_tasks.items():
        print(f'Downloading artifacts for run {run_id}...', file=sys.stderr)
        temp_dir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        temp_dirs[run_id] = temp_dir

        download_success = download_test_results(run_id, temp_dir.name)
        run_temp_dir = temp_dir.name if download_success else None

        for job in run_data['jobs']:
          futures.append(
              executor.submit(process_job, job, cache_dir, run_temp_dir))

      # Wait for all to complete and check for exceptions
      for future in concurrent.futures.as_completed(futures):
        try:
          future.result()
        except Exception as e:  # pylint: disable=broad-exception-caught
          print(f'Exception in process_job: {e}', file=sys.stderr)
          traceback.print_exc(file=sys.stderr)
  finally:
    # Cleanup temp dirs
    for temp_dir in temp_dirs.values():
      temp_dir.cleanup()

  # Rename key if we want to follow the final schema
  # The output is basically the same but with local_log_path populated in
  # failed_jobs.
  with open(args.output, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)

  print(f'Successfully wrote results to {args.output}', file=sys.stderr)


if __name__ == '__main__':
  main()
