#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Tests for github_discover.py."""

import json
import os
import sys
import tempfile
import unittest
from unittest import mock

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
if _scripts_dir not in sys.path:
  sys.path.append(_scripts_dir)
# pylint: disable=wrong-import-position
import github_discover


class TestGitHubDiscover(unittest.TestCase):
  """Unit tests for github_discover functions."""

  def setUp(self):
    super().setUp()
    self.test_dir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def test_parse_build_status(self):
    content = """# Build Status
## Main
| Platform      | Build Status                             | Nightly Build                                            |
| :-------------| :----------------------------------------| :--------------------------------------------------------|
| **android**   | [![Status][android-badge]][android-link] | [![Status][android-nightly-badge]][android-nightly-link] |

[android-badge]: https://github.com/youtube/cobalt/actions/workflows/android.yaml/badge.svg?event=push&branch=main
[android-nightly-badge]: https://github.com/youtube/cobalt/actions/workflows/android.yaml/badge.svg?event=workflow_dispatch&branch=main
[android-link]: https://github.com/youtube/cobalt/actions/workflows/android.yaml?query=event%3Apush+branch%3Amain

### 27.lts
| Platform      | Build Status                             |
| :-------------| :----------------------------------------|
| **android**   | [![Status][android-27-badge]][android-27-link] |

[android-27-badge]: https://github.com/youtube/cobalt/actions/workflows/android_27.lts.yaml/badge.svg?event=push&branch=27.lts
[android-27-link]: https://github.com/youtube/cobalt/actions/workflows/android_27.lts.yaml?query=event%3Apush+branch%3A27.lts
"""
    path = os.path.join(self.test_dir.name, "BUILD_STATUS.md")
    with open(path, "w", encoding="utf-8") as f:
      f.write(content)

    workflows = github_discover.parse_build_status(path)

    expected = [
        {
            "workflow": "android.yaml",
            "event": "push",
            "branch": "main"
        },
        {
            "workflow": "android.yaml",
            "event": "workflow_dispatch",
            "branch": "main",
        },
        {
            "workflow": "android_27.lts.yaml",
            "event": "push",
            "branch": "27.lts",
        },
    ]
    self.assertEqual(workflows, expected)

  @mock.patch.object(github_discover.gardener_utils, "run_gh_command")
  def test_discover_runs(self, mock_gh):
    # We have two workflows
    workflows = [
        {
            "workflow": "android.yaml",
            "event": "push",
            "branch": "main"
        },
        {
            "workflow": "linux.yaml",
            "event": "push",
            "branch": "main"
        },
    ]

    # Define mock responses
    # First call to run_gh_command for run list of android.yaml (failed)
    # Second call to run_gh_command for jobs of android.yaml run
    # Third call to run_gh_command for run list of linux.yaml (success)
    # Fourth call to run_gh_command for jobs of linux.yaml run

    run_android = [{
        "databaseId": 123,
        "conclusion": "failure",
        "status": "completed",
        "url": "url1",
        "createdAt": "time1",
    }]
    jobs_android = {
        "jobs": [
            {
                "databaseId": 1001,
                "name": "build",
                "conclusion": "failure",
                "status": "completed",
                "url": "joburl1",
            },
            {
                "databaseId": 1002,
                "name": "test",
                "conclusion": "success",
                "status": "completed",
                "url": "joburl2",
            },
        ]
    }

    run_linux = [{
        "databaseId": 456,
        "conclusion": "success",
        "status": "completed",
        "url": "url2",
        "createdAt": "time2",
    }]
    jobs_linux = {
        "jobs": [{
            "databaseId": 2001,
            "name": "build",
            "conclusion": "success",
            "status": "completed",
            "url": "joburl3",
        }]
    }

    def side_effect(args):
      if "run" in args and "list" in args:
        if "android.yaml" in args:
          return json.dumps(run_android)
        elif "linux.yaml" in args:
          return json.dumps(run_linux)
      elif "run" in args and "view" in args:
        if "123" in args:
          return json.dumps(jobs_android)
        elif "456" in args:
          return json.dumps(jobs_linux)
      raise ValueError(f"Unexpected args: {args}")

    mock_gh.side_effect = side_effect

    results = github_discover.discover_runs(workflows)

    expected_runs = [
        {
            "run_id":
                123,
            "workflow_name":
                "android.yaml",
            "branch":
                "main",
            "event":
                "push",
            "createdAt":
                "time1",
            "url":
                "url1",
            "failed_jobs": [{
                "job_id": 1001,
                "name": "build",
                "url": "joburl1"
            }],
        },
        {
            "run_id": 456,
            "workflow_name": "linux.yaml",
            "branch": "main",
            "event": "push",
            "createdAt": "time2",
            "url": "url2",
            "failed_jobs": [],
        },
    ]

    # total_jobs_fetched = len(jobs_android) + len(jobs_linux) = 2 + 1 = 3
    self.assertEqual(results["total_jobs_fetched"], 3)
    self.assertEqual(results["runs"], expected_runs)

  @mock.patch.object(github_discover.gardener_utils, "run_gh_command")
  def test_discover_run_by_id(self, mock_gh):
    mock_response = {
        "createdAt": "time1",
        "databaseId": 87654321,
        "event": "push",
        "headBranch": "main",
        "jobs": [
            {
                "conclusion": "failure",
                "databaseId": 1001,
                "name": "build",
                "url": "joburl1",
            },
            {
                "conclusion": "success",
                "databaseId": 1002,
                "name": "test",
                "url": "joburl2",
            },
        ],
        "url": "url1",
        "workflowName": "android.yaml",
    }
    mock_gh.return_value = json.dumps(mock_response)

    results = github_discover.discover_run_by_id(87654321)

    expected_runs = [{
        "run_id": 87654321,
        "workflow_name": "android.yaml",
        "branch": "main",
        "event": "push",
        "createdAt": "time1",
        "url": "url1",
        "failed_jobs": [{
            "job_id": 1001,
            "name": "build",
            "url": "joburl1"
        }],
        "ignore_age": True,
    }]

    self.assertEqual(results["total_jobs_fetched"], 2)
    self.assertEqual(results["runs"], expected_runs)
    mock_gh.assert_called_once_with([
        "run",
        "view",
        "87654321",
        "--json",
        "databaseId,workflowName,headBranch,event,createdAt,url,jobs",
        "-R",
        "youtube/cobalt",
    ])

  def test_parse_build_status_not_found(self):
    # Pass a non-existent path
    workflows = github_discover.parse_build_status(
        os.path.join(self.test_dir.name, "does_not_exist.md"))
    self.assertEqual(workflows, [])

  @mock.patch.object(github_discover.gardener_utils, "run_gh_command")
  def test_discover_runs_empty(self, mock_gh):
    workflows = [{
        "workflow": "android.yaml",
        "event": "push",
        "branch": "main"
    }]
    # gh run list returns empty list
    mock_gh.return_value = "[]"
    results = github_discover.discover_runs(workflows)
    self.assertEqual(results["total_jobs_fetched"], 0)
    self.assertEqual(results["runs"], [])

  @mock.patch.object(github_discover.gardener_utils, "run_gh_command")
  def test_discover_runs_gh_failure_continues(self, mock_gh):
    # We have two workflows, one fails to query, the other succeeds
    workflows = [
        {
            "workflow": "fail.yaml",
            "event": "push",
            "branch": "main"
        },
        {
            "workflow": "success.yaml",
            "event": "push",
            "branch": "main"
        },
    ]

    run_success = [{
        "databaseId": 123,
        "conclusion": "failure",
        "status": "completed",
        "url": "url1",
        "createdAt": "time1",
    }]
    jobs_success = {
        "jobs": [{
            "databaseId": 1001,
            "name": "build",
            "conclusion": "failure",
            "status": "completed",
            "url": "joburl1",
        }]
    }

    def side_effect(args):
      if "fail.yaml" in args:
        raise Exception("gh API error")  # pylint: disable=broad-exception-raised
      if "success.yaml" in args:
        return json.dumps(run_success)
      if "123" in args:
        return json.dumps(jobs_success)
      raise ValueError(f"Unexpected args: {args}")

    mock_gh.side_effect = side_effect

    # This should not raise an exception, but log and continue
    results = github_discover.discover_runs(workflows)

    # It should successfully process 'success.yaml'
    self.assertEqual(results["total_jobs_fetched"], 1)
    self.assertEqual(len(results["runs"]), 1)
    self.assertEqual(results["runs"][0]["run_id"], 123)


if __name__ == "__main__":
  unittest.main()
