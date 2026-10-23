#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Unit tests for minimal shepherd_pr.py."""
import json
import os
import tempfile
import unittest
from unittest.mock import patch
from cobalt.devinfra.github import shepherd_pr


class ShepherdPrTest(unittest.TestCase):
    """Comprehensive test suite for minimal shepherd_pr module."""

    def test_resolve_repo_explicit(self):
        self.assertEqual(shepherd_pr.resolve_repo("org/repo"), ("org", "repo"))
        self.assertEqual(shepherd_pr.resolve_repo("org/repo.git"),
                         ("org", "repo"))

    @patch("cobalt.devinfra.github.shepherd_pr.gh")
    def test_resolve_repo_default(self, mock_gh):
        mock_gh.return_value = "youtube/cobalt"
        self.assertEqual(shepherd_pr.resolve_repo(None), ("youtube", "cobalt"))

    def test_resolve_prs(self):
        self.assertEqual(shepherd_pr.resolve_prs("123,456"), [123, 456])
        self.assertEqual(shepherd_pr.resolve_prs("789"), [789])

    @patch("cobalt.devinfra.github.shepherd_pr.gh")
    def test_resolve_prs_default(self, mock_gh):
        mock_gh.return_value = "42"
        self.assertEqual(shepherd_pr.resolve_prs(None), [42])

    def test_parse_shepherd_report_file_failure(self):
        with tempfile.NamedTemporaryFile("w+", suffix=".json") as f:
            f.write(
                json.dumps({
                    "platform": "linux",
                    "workflow": "linux",
                    "run_url": "https://example.com/1",
                    "checks": {
                        "build": "success",
                        "test": "failure"
                    },
                    "test_failures": {
                        "target": ["test1"]
                    },
                }))
            f.flush()
            rep = shepherd_pr.parse_shepherd_report_file(f.name)
            self.assertIsNotNone(rep)
            self.assertTrue(rep["has_failures"])
            self.assertEqual(rep["platform"], "linux")
            self.assertIn("test", rep["failing_jobs"])

    def test_parse_shepherd_report_file_success(self):
        with tempfile.NamedTemporaryFile("w+", suffix=".json") as f:
            f.write(
                json.dumps({
                    "platform": "android",
                    "workflow": "android",
                    "run_url": "https://example.com/2",
                    "checks": {
                        "build": "success",
                        "test": "success"
                    },
                }))
            f.flush()
            rep = shepherd_pr.parse_shepherd_report_file(f.name)
            self.assertIsNotNone(rep)
            self.assertFalse(rep["has_failures"])
            self.assertEqual(rep["platform"], "android")
            self.assertEqual(rep["failing_jobs"], [])

    @patch("cobalt.devinfra.github.shepherd_pr.gh")
    @patch("cobalt.devinfra.github.shepherd_pr.fetch_ci_shepherd_reports")
    def test_evaluate_pr_green(self, mock_reports, mock_gh):
        mock_gh.return_value = json.dumps({
            "data": {
                "repository": {
                    "pullRequest": {
                        "headRefOid": "abc123",
                        "reviewDecision": "APPROVED",
                        "mergeable": "MERGEABLE",
                        "state": "OPEN",
                        "isDraft": False,
                        "reviewThreads": {
                            "nodes": []
                        },
                    }
                }
            }
        })
        mock_reports.return_value = ([{
            "platform": "linux",
            "workflow": "linux",
            "has_failures": False,
        }], None)
        with tempfile.TemporaryDirectory() as tmpdir:
            res = shepherd_pr.evaluate_pr("foo",
                                          "bar",
                                          123,
                                          artifacts_dir=tmpdir)
            self.assertTrue(res["is_green"])
            self.assertEqual(res["pr"], 123)
            self.assertFalse(res["has_failed_job"])
            self.assertTrue(
                os.path.exists(os.path.join(tmpdir, "summary.json")))

    @patch("cobalt.devinfra.github.shepherd_pr.gh")
    @patch("cobalt.devinfra.github.shepherd_pr.fetch_ci_shepherd_reports")
    def test_evaluate_pr_failed(self, mock_reports, mock_gh):
        mock_gh.return_value = json.dumps({
            "data": {
                "repository": {
                    "pullRequest": {
                        "headRefOid": "abc123",
                        "reviewDecision": "CHANGES_REQUESTED",
                        "mergeable": "CONFLICTING",
                        "state": "OPEN",
                        "isDraft": False,
                        "reviewThreads": {
                            "nodes": [{
                                "isResolved": False,
                                "path": "p",
                                "line": 1
                            }]
                        },
                    }
                }
            }
        })
        mock_reports.return_value = ([{
            "platform":
            "linux",
            "workflow":
            "linux",
            "has_failures":
            True,
            "failing_jobs": ["test"],
            "run_url":
            "https://github.com/actions/runs/12345",
        }], None)
        with tempfile.TemporaryDirectory() as tmpdir:
            res = shepherd_pr.evaluate_pr("foo",
                                          "bar",
                                          123,
                                          artifacts_dir=tmpdir)
            self.assertFalse(res["is_green"])
            self.assertTrue(res["has_failed_job"])
            self.assertEqual(len(res["unresolved_threads"]), 1)

    @patch("cobalt.devinfra.github.shepherd_pr.gh")
    def test_download_failure_artifacts(self, mock_gh):
        mock_gh.return_value = "log contents for failed build"
        reports = [{
            "platform": "linux",
            "workflow": "linux",
            "has_failures": True,
            "failing_jobs": ["build"],
            "run_url": "https://github.com/actions/runs/98765",
        }]
        with tempfile.TemporaryDirectory() as tmpdir:
            res = shepherd_pr.download_failure_artifacts("org",
                                                         "repo",
                                                         reports,
                                                         tmpdir,
                                                         temp_dir=None)
            self.assertEqual(len(res["logs"]), 1)
            self.assertTrue(os.path.exists(res["logs"][0]))
            with open(res["logs"][0], "r", encoding="utf-8") as f:
                self.assertIn("log contents", f.read())

    @patch("cobalt.devinfra.github.shepherd_pr.evaluate_pr")
    @patch("cobalt.devinfra.github.shepherd_pr.resolve_repo")
    @patch("cobalt.devinfra.github.shepherd_pr.resolve_prs")
    def test_main_cli_exit_zero(self, mock_prs, mock_repo, mock_eval):
        mock_repo.return_value = ("org", "repo")
        mock_prs.return_value = [100]
        mock_eval.return_value = {
            "pr": 100,
            "is_green": False,
            "ci_shepherd_reports": [],
            "unresolved_threads": [],
            "mergeable": "MERGEABLE",
        }
        with patch("sys.argv", ["shepherd_pr.py", "--exit-zero"]):
            with self.assertRaises(SystemExit) as cm:
                shepherd_pr.main()
            self.assertEqual(cm.exception.code, 0)

    @patch("cobalt.devinfra.github.shepherd_pr.evaluate_pr")
    @patch("cobalt.devinfra.github.shepherd_pr.resolve_repo")
    @patch("cobalt.devinfra.github.shepherd_pr.resolve_prs")
    def test_main_cli_green_success(self, mock_prs, mock_repo, mock_eval):
        mock_repo.return_value = ("org", "repo")
        mock_prs.return_value = [101]
        mock_eval.return_value = {
            "pr":
            101,
            "is_green":
            True,
            "ci_shepherd_reports": [{
                "platform": "linux",
                "workflow": "linux",
                "has_failures": False,
            }],
            "unresolved_threads": [],
            "mergeable":
            "MERGEABLE",
        }
        with patch("sys.argv", ["shepherd_pr.py"]):
            with self.assertRaises(SystemExit) as cm:
                shepherd_pr.main()
            self.assertEqual(cm.exception.code, 0)

    @patch("cobalt.devinfra.github.shepherd_pr.evaluate_pr")
    @patch("cobalt.devinfra.github.shepherd_pr.resolve_repo")
    @patch("cobalt.devinfra.github.shepherd_pr.resolve_prs")
    def test_main_cli_failure_exits_one(self, mock_prs, mock_repo, mock_eval):
        mock_repo.return_value = ("org", "repo")
        mock_prs.return_value = [102]
        mock_eval.return_value = {
            "pr":
            102,
            "is_green":
            False,
            "ci_shepherd_reports": [{
                "platform": "linux",
                "workflow": "linux",
                "has_failures": True,
            }],
            "unresolved_threads": [],
            "mergeable":
            "MERGEABLE",
        }
        with patch("sys.argv", ["shepherd_pr.py"]):
            with self.assertRaises(SystemExit) as cm:
                shepherd_pr.main()
            self.assertEqual(cm.exception.code, 1)


if __name__ == "__main__":
    unittest.main()
