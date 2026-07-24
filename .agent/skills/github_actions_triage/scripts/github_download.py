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
import sys
import tempfile
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


def process_job(run_id, job, cache_dir):
  """Processes a single GHA job.

  Args:
    run_id: ID of the GHA run.
    job: Job metadata dict to update with local log path.
    cache_dir: Directory where logs should be stored.
  """
  job_id = job['job_id']
  job_name = job['name']

  dest_log_path = os.path.join(cache_dir, f'{job_id}.log')

  if os.path.exists(dest_log_path) and os.path.getsize(dest_log_path) > 0:
    print(
        f'Log file already exists in cache for job {job_name} ({job_id}).'
        ' Skipping download.',
        file=sys.stderr,
    )
    job['local_log_path'] = dest_log_path
    return

  is_test_job = 'test' in job_name.lower() or 'results' in job_name.lower()

  if is_test_job:
    print(
        f'Job {job_name} ({job_id}) classified as Test Job. Attempting XML'
        ' download...',
        file=sys.stderr,
    )
    # Use a separate temp dir for this run's artifacts to avoid mixing
    with tempfile.TemporaryDirectory() as temp_dl_dir:
      if download_test_results(run_id, temp_dl_dir):
        # Find all XMLs
        xml_files = glob.glob(
            os.path.join(temp_dl_dir, '**/*.xml'), recursive=True)
        failures = []
        for xml_file in xml_files:
          failures.extend(parse_junit_xml(xml_file))

        if failures:
          print(
              f'Found {len(failures)} failures in JUnit XMLs. Writing synthetic'
              ' log.',
              file=sys.stderr,
          )
          write_synthetic_log(failures, dest_log_path)
          job['local_log_path'] = dest_log_path
          return
        else:
          print(
              f'No failures found in XMLs for job {job_name} ({job_id}).'
              ' Falling back to full log.',
              file=sys.stderr,
          )
      else:
        print(
            f'Failed to download XMLs for job {job_name} ({job_id}). Falling'
            ' back to full log.',
            file=sys.stderr,
        )

  # Fallback to full log download
  print(
      f'Downloading full log for job {job_name} ({job_id})...', file=sys.stderr)
  if download_job_log(job_id, dest_log_path):
    job['local_log_path'] = dest_log_path
  else:
    job['local_log_path'] = ''


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
  print(f'Using cache directory: {cache_dir}', file=sys.stderr)

  now = datetime.datetime.now(datetime.timezone.utc)
  # Collect all tasks to run in parallel
  tasks = []
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
    for job in run.get('failed_jobs', []):
      tasks.append((run_id, job))

  print(f'Processing {len(tasks)} failed jobs in parallel...', file=sys.stderr)
  with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = [
        executor.submit(process_job, run_id, job, cache_dir)
        for run_id, job in tasks
    ]
    # Wait for all to complete
    concurrent.futures.wait(futures)

  # Rename key if we want to follow the final schema
  # The output is basically the same but with local_log_path populated in
  # failed_jobs.
  with open(args.output, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)

  print(f'Successfully wrote results to {args.output}', file=sys.stderr)


if __name__ == '__main__':
  main()
