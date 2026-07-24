# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Utility functions for GHA status checks and run age verification."""

import datetime
import subprocess
import sys

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
