#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Discovers recent GHA runs and jobs for Youtube Cobalt repository."""

import argparse
import concurrent.futures
import json
import os
import re
import sys
import urllib.parse

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
if _scripts_dir not in sys.path:
  sys.path.append(_scripts_dir)
# pylint: disable=wrong-import-position
import gardener_utils

DEFAULT_REPOSITORY = 'youtube/cobalt'


def parse_build_status(file_path):
  """Parses a Cobalt build status markdown file for workflow triggers.

  Args:
    file_path: Path to the BUILD_STATUS.md file.

  Returns:
    A list of dicts with workflow metadata.
  """
  workflows = []
  # Match badge URLs of the format:
  # [android-badge]: https://github.com/youtube/cobalt/actions/workflows/...
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
  """Queries GHA API for a single run of a workflow.

  Args:
    workflow_config: Metadata dict of the workflow.
    limit: Max number of runs to fetch.

  Returns:
    A tuple of (run_info, jobs_count).
  """
  workflow = workflow_config['workflow']
  branch = workflow_config['branch']
  event = workflow_config['event']
  try:
    # Query the latest completed run
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
    stdout = gardener_utils.run_gh_command(args)
    runs = json.loads(stdout)
    if not runs:
      return None, 0

    run_data = runs[0]
    run_id = run_data['databaseId']

    # Fetch jobs for this run
    jobs_stdout = gardener_utils.run_gh_command([
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
        'run_id': run_id,
        'workflow_name': workflow,
        'branch': branch,
        'event': event,
        'createdAt': run_data.get('createdAt'),
        'url': run_data.get('url'),
        'failed_jobs': failed_jobs,
    }, len(jobs)

  except Exception as e:  # pylint: disable=broad-exception-caught
    print(
        f'Failed to fetch runs for {workflow} on {branch} ({event}): {e}',
        file=sys.stderr,
    )
    return None, 0


def discover_runs(workflows, limit=1):
  """Discovers runs for a list of workflows in parallel.

  Args:
    workflows: List of workflow config dicts.
    limit: Max number of runs to fetch per workflow.

  Returns:
    A dict containing overall stats and found runs.
  """
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

  # Sort runs for deterministic output
  discovered_runs.sort(
      key=lambda x: (x['workflow_name'], x['branch'], x['event']))

  return {'total_jobs_fetched': total_jobs_fetched, 'runs': discovered_runs}


def discover_run_by_id(run_id):
  """Discovers a run by its GHA run ID.

  Args:
    run_id: The ID of the run to discover.

  Returns:
    A dict representing the discovered run info.
  """

  try:
    args = [
        'run',
        'view',
        str(run_id),
        '--json',
        'databaseId,workflowName,headBranch,event,createdAt,url,jobs',
        '-R',
        DEFAULT_REPOSITORY,
    ]
    stdout = gardener_utils.run_gh_command(args)
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
        'run_id': run_data.get('databaseId'),
        'workflow_name': run_data.get('workflowName'),
        'branch': run_data.get('headBranch'),
        'event': run_data.get('event'),
        'createdAt': run_data.get('createdAt'),
        'url': run_data.get('url'),
        'failed_jobs': failed_jobs,
        'ignore_age': True,
    }

    return {'total_jobs_fetched': len(jobs), 'runs': [run_info]}
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Failed to fetch run by ID {run_id}: {e}', file=sys.stderr)
    return {'total_jobs_fetched': 0, 'runs': []}


def main():
  parser = argparse.ArgumentParser(
      description='Discover latest failed runs from BUILD_STATUS.md.')
  parser.add_argument(
      '--build-status',
      default='cobalt/BUILD_STATUS.md',
      help='Path to BUILD_STATUS.md',
  )
  parser.add_argument(
      '--output',
      default='github_jobs.json',
      help='Path to write the output JSON.',
  )
  parser.add_argument(
      '--run-id',
      type=int,
      help='Specific run ID to triage.',
  )
  args = parser.parse_args()

  if args.run_id:
    print(f'Discovering run ID {args.run_id} via gh CLI...', file=sys.stderr)
    results = discover_run_by_id(args.run_id)
  else:
    build_status_path = args.build_status
    if not os.path.exists(build_status_path):
      # Maybe we are in a subdirectory? Try to find repo root.
      # We can look for .git
      # For simplicity, error out if not found.
      print(
          f'Error: {build_status_path} not found. Run from the repository'
          ' root.',
          file=sys.stderr,
      )
      sys.exit(1)

    print(f'Parsing build status from {build_status_path}...', file=sys.stderr)
    workflows = parse_build_status(build_status_path)
    print(
        f'Found {len(workflows)} active workflow configurations.',
        file=sys.stderr,
    )

    print('Discovering latest completed runs via gh CLI...', file=sys.stderr)
    results = discover_runs(workflows)

  with open(args.output, 'w', encoding='utf-8') as f:
    json.dump(results, f, indent=2)

  print(
      f'Successfully wrote discovery results to {args.output}', file=sys.stderr)


if __name__ == '__main__':
  main()
