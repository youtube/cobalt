# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Tests for gardener_utils.py."""

import datetime
import os
import sys
import unittest
from unittest import mock

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
if _scripts_dir not in sys.path:
  sys.path.append(_scripts_dir)
# pylint: disable=wrong-import-position
import gardener_utils


class TestGardenerUtils(unittest.TestCase):
  """Unit tests for gardener_utils functions."""

  def test_check_run_age(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)

    # 1. Recent nightly (23h ago) -> not outdated
    run_recent_nightly = {
        'createdAt': ((now - datetime.timedelta(hours=23)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'schedule',
    }
    is_outdated, age_str = gardener_utils.check_run_age(run_recent_nightly, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '23 hour(s) ago')

    # 2. Outdated nightly (25h ago) -> outdated
    run_outdated_nightly = {
        'createdAt': ((now - datetime.timedelta(hours=25)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'schedule',
    }
    is_outdated, age_str = gardener_utils.check_run_age(run_outdated_nightly,
                                                        now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, '1 day(s) ago')

    # 3. Recent postsubmit (6 days ago) -> not outdated
    run_recent_postsubmit = {
        'createdAt': ((now - datetime.timedelta(days=6)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'push',
    }
    is_outdated, age_str = gardener_utils.check_run_age(run_recent_postsubmit,
                                                        now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '6 day(s) ago')

    # 4. Outdated postsubmit (8 days ago) -> outdated
    run_outdated_postsubmit = {
        'createdAt': ((now - datetime.timedelta(days=8)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'push',
    }
    is_outdated, age_str = gardener_utils.check_run_age(run_outdated_postsubmit,
                                                        now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, '8 day(s) ago')

  def test_check_run_age_missing_created_at(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run = {'event': 'push'}
    is_outdated, age_str = gardener_utils.check_run_age(run, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, 'unknown age')

  def test_check_run_age_invalid_created_at(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run = {'createdAt': 'invalid-date-string', 'event': 'push'}
    # Redirect stderr to avoid cluttering test output
    with mock.patch('sys.stderr') as mock_stderr:
      is_outdated, age_str = gardener_utils.check_run_age(run, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, 'unknown age')
    mock_stderr.write.assert_called()

  def test_check_run_age_missing_event_defaults_to_push(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run_recent = {
        'createdAt': ((now - datetime.timedelta(days=6)).isoformat().replace(
            '+00:00', 'Z'))
    }
    is_outdated, age_str = gardener_utils.check_run_age(run_recent, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '6 day(s) ago')

    run_outdated = {
        'createdAt': ((now - datetime.timedelta(days=8)).isoformat().replace(
            '+00:00', 'Z'))
    }
    is_outdated, age_str = gardener_utils.check_run_age(run_outdated, now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, '8 day(s) ago')

  def test_check_run_age_naive_created_at(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run = {'createdAt': '2026-07-14T12:00:00', 'event': 'push'}
    is_outdated, age_str = gardener_utils.check_run_age(run, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '6 day(s) ago')

  @mock.patch.object(gardener_utils.subprocess, 'run')
  def test_run_gh_command_success(self, mock_run):
    mock_response = mock.MagicMock()
    mock_response.returncode = 0
    mock_response.stdout = 'some output'
    mock_run.return_value = mock_response

    output = gardener_utils.run_gh_command(['api', 'repos/youtube/cobalt'])
    self.assertEqual(output, 'some output')
    mock_run.assert_called_once_with(
        ['gh', 'api', 'repos/youtube/cobalt'],
        capture_output=True,
        text=True,
        check=False,
    )

  @mock.patch.object(gardener_utils.subprocess, 'run')
  def test_run_gh_command_failure(self, mock_run):
    mock_response = mock.MagicMock()
    mock_response.returncode = 1
    mock_response.stderr = 'some error'
    mock_run.return_value = mock_response

    with self.assertRaises(Exception) as context:
      gardener_utils.run_gh_command(['invalid'])
    self.assertIn('gh command failed: some error', str(context.exception))


if __name__ == '__main__':
  unittest.main()
