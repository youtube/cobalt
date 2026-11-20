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
"""Black-box integration test suite for shepherd_pr.py."""

# pylint: disable=bad-indentation

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

_SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
_SCRIPT_PATH = os.path.join(_SCRIPTS_DIR, "shepherd_pr.py")

_MOCK_GH_SCRIPT = """#!/usr/bin/env python3
import json, os, sys

with open(os.environ["MOCK_GH_CONFIG"], "r", encoding="utf-8") as f:
  config = json.load(f)

argv = sys.argv[1:]
calls_path = config["calls_path"]
calls = []
if os.path.exists(calls_path):
  with open(calls_path, "r", encoding="utf-8") as f:
    calls = json.load(f)
calls.append(argv)
with open(calls_path, "w", encoding="utf-8") as f:
  json.dump(calls, f)

if argv[:2] == ["pr", "view"]:
  if "--repo" not in argv:
    sys.stderr.write("Missing --repo in gh pr view\\n")
    sys.exit(1)
  sys.stdout.write(str(config.get("default_pr", 101)))
  sys.exit(0)

if argv[:2] == ["api", "graphql"]:
  count_path = config["count_path"]
  idx = 0
  if os.path.exists(count_path):
    with open(count_path, "r", encoding="utf-8") as f:
      idx = int(f.read())
  with open(count_path, "w", encoding="utf-8") as f:
    f.write(str(idx + 1))

  errors = config.get("graphql_errors", [])
  if idx < len(errors) and errors[idx]:
    sys.stderr.write(errors[idx])
    sys.exit(1)
  if config.get("error"):
    sys.stderr.write(config["error"])
    sys.exit(1)
  pages = config["graphql_pages"]
  sys.stdout.write(json.dumps(pages[min(idx, len(pages) - 1)]))
  sys.exit(0)

if argv[:2] == ["run", "download"]:
  run_id = argv[2]
  if run_id in config.get("forbidden_run_ids", []):
    sys.stderr.write(f"Forbidden download for run {run_id}\\n")
    sys.exit(1)
  dest_dir = argv[argv.index("-D") + 1]
  for entry in config.get("reports", []):
    if entry.get("run_id", run_id) == run_id:
      rdir = os.path.join(dest_dir, f"shepherd-report-{entry['platform']}")
      os.makedirs(rdir, exist_ok=True)
      with open(os.path.join(rdir, "shepherd_report.json"), "w", encoding="utf-8") as out:
        json.dump(entry["data"], out)
  sys.exit(0)

if argv[:2] == ["run", "view"] and "--job" in argv:
  job_id = argv[argv.index("--job") + 1]
  logs = config.get("logs", {})
  if job_id in logs:
    sys.stdout.write(logs[job_id])
    sys.exit(0)
  sys.exit(1)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""


def _pr_payload(
    number,
    checks,
    *,
    state="OPEN",
    is_draft=False,
    mergeable="MERGEABLE",
    merge_state="CLEAN",
    review_decision="APPROVED",
    author="pr-author",
    required_contexts=None,
    has_next_page=False,
    end_cursor=None,
    threads=None,
    threads_has_next=False,
    threads_end_cursor=None,
):
    return {
        "data": {
            "repository": {
                "pullRequest": {
                    "number": number,
                    "state": state,
                    "isDraft": is_draft,
                    "reviewDecision": review_decision,
                    "headRefName": f"branch-{number}",
                    "baseRefName": "main",
                    "headRefOid": f"sha_{number}",
                    "author": {
                        "login": author
                    },
                    "mergeable": mergeable,
                    "mergeStateStatus": merge_state,
                    "baseRef": {
                        "branchProtectionRule": {
                            "requiredStatusCheckContexts": required_contexts
                            or []
                        }
                    },
                    "statusCheckRollup": {
                        "contexts": {
                            "nodes": checks,
                            "pageInfo": {
                                "hasNextPage": has_next_page,
                                "endCursor": end_cursor,
                            },
                        }
                    },
                    "reviewThreads": {
                        "nodes": threads or [],
                        "pageInfo": {
                            "hasNextPage": threads_has_next,
                            "endCursor": threads_end_cursor,
                        },
                    },
                }
            }
        }
    }


def _check_run(name,
               status="COMPLETED",
               conclusion="SUCCESS",
               *,
               url="",
               job_id=None,
               run_id=None,
               is_required=True):
    node = {
        "__typename": "CheckRun",
        "name": name,
        "status": status,
        "conclusion": conclusion,
        "detailsUrl": url,
        "isRequired": is_required,
    }
    if job_id is not None:
        node["databaseId"] = job_id
    if run_id is not None:
        node["checkSuite"] = {"workflowRun": {"databaseId": int(run_id)}}
    return node


def _thread(path, line, comments, *, is_resolved=False):
    return {
        "isResolved": is_resolved,
        "path": path,
        "line": line,
        "comments": {
            "nodes": comments
        },
    }


def _comment(cid, author, body):
    return {
        "databaseId": cid,
        "author": {
            "login": author
        },
        "body": body,
        "url": f"https://github.com/org/repo/pull/1#discussion_r{cid}",
    }


class TestPRShepherdCLI(unittest.TestCase):
    """Black-box command-line integration test suite for shepherd_pr.py."""

    def setUp(self):
        super().setUp()
        self.test_dir = tempfile.mkdtemp()
        self.bin_dir = os.path.join(self.test_dir, "bin")
        os.makedirs(self.bin_dir, exist_ok=True)
        mock_gh = os.path.join(self.bin_dir, "gh")
        with open(mock_gh, "w", encoding="utf-8") as gh_file:
            gh_file.write(_MOCK_GH_SCRIPT)
        os.chmod(mock_gh, 0o755)
        self.calls_path = os.path.join(self.test_dir, "calls.json")
        self.count_path = os.path.join(self.test_dir, "gql_count.txt")

    def tearDown(self):
        shutil.rmtree(self.test_dir, ignore_errors=True)
        super().tearDown()

    def run_cli(self, args, **mock_spec):
        config_path = os.path.join(self.test_dir, "mock_gh.json")
        for path in (self.calls_path, self.count_path):
            if os.path.exists(path):
                os.remove(path)
        with open(config_path, "w", encoding="utf-8") as config_file:
            json.dump(
                {
                    "count_path": self.count_path,
                    "calls_path": self.calls_path,
                    **mock_spec,
                },
                config_file,
            )

        env = os.environ.copy()
        curr_path = env["PATH"]
        env["PATH"] = f"{self.bin_dir}:{curr_path}"
        env["MOCK_GH_CONFIG"] = config_path

        return subprocess.run(
            [sys.executable, _SCRIPT_PATH, *args],
            capture_output=True,
            text=True,
            env=env,
            cwd=self.test_dir,
            check=False,
        )

    def _read_calls(self):
        with open(self.calls_path, "r", encoding="utf-8") as calls_file:
            return json.load(calls_file)

    def test_cli_required_gating_ignores_non_required_failures(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "101"],
            graphql_pages=[
                _pr_payload(
                    101,
                    [
                        _check_run("build", is_required=True),
                        _check_run("optional-flake",
                                   conclusion="FAILURE",
                                   is_required=False),
                    ],
                )
            ],
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertEqual(res.stdout.strip(),
                         "SUPER GREEN! Multipass authorized.")

    def test_cli_expected_seeding_and_neutral_success(self):
        res = self.run_cli(
            [
                "--repo", "org/repo", "--pr", "102", "--watch", "--interval",
                "0"
            ],
            graphql_pages=[
                _pr_payload(
                    102,
                    [_check_run("req-a", conclusion="NEUTRAL")],
                    required_contexts=["req-a", "req-b"],
                ),
                _pr_payload(
                    102,
                    [
                        _check_run("req-a", conclusion="NEUTRAL"),
                        _check_run("req-b", conclusion="SUCCESS"),
                    ],
                    required_contexts=["req-a", "req-b"],
                ),
            ],
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertEqual(res.stdout.strip(),
                         "SUPER GREEN! Multipass authorized.")

    def test_cli_waiting_and_requested_are_pending(self):
        res = self.run_cli(
            [
                "--repo", "org/repo", "--pr", "103", "--watch", "--interval",
                "0"
            ],
            graphql_pages=[
                _pr_payload(
                    103,
                    [
                        _check_run(
                            "req-wait", status="WAITING", conclusion=None),
                        _check_run(
                            "req-req", status="REQUESTED", conclusion=None),
                    ],
                ),
                _pr_payload(
                    103,
                    [
                        _check_run("req-wait", conclusion="SUCCESS"),
                        _check_run("req-req", conclusion="SUCCESS"),
                    ],
                ),
            ],
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertEqual(res.stdout.strip(),
                         "SUPER GREEN! Multipass authorized.")

    def test_cli_skipped_rejected_on_required_check(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "104", "--no-download"],
            graphql_pages=[
                _pr_payload(
                    104,
                    [_check_run("validate-result", conclusion="SKIPPED")],
                )
            ],
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn("Failed check: validate-result [SKIPPED]", res.stdout)
        self.assertNotIn("SUPER GREEN", res.stdout)

    def test_cli_deduplicates_checks_by_highest_db_id_and_ors_required(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "105"],
            graphql_pages=[
                _pr_payload(
                    105,
                    [
                        _check_run("lint",
                                   conclusion="SUCCESS",
                                   job_id=200,
                                   is_required=False),
                        _check_run("lint",
                                   conclusion="FAILURE",
                                   job_id=100,
                                   is_required=True),
                    ],
                )
            ],
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        self.assertEqual(res.stdout.strip(),
                         "SUPER GREEN! Multipass authorized.")

    def test_cli_pagination_checks_and_threads_without_refetch(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "106"],
            graphql_pages=[
                _pr_payload(
                    106,
                    [_check_run("build-1")],
                    has_next_page=True,
                    end_cursor="chk_1",
                    threads=[
                        _thread("a.py",
                                10, [_comment(10, "rev", "Resolved ok")],
                                is_resolved=True)
                    ],
                    threads_has_next=True,
                    threads_end_cursor="thr_1",
                ),
                _pr_payload(106, [_check_run("build-2")]),
                _pr_payload(
                    106,
                    [],
                    threads=[
                        _thread("b.py", 20,
                                [_comment(20, "rev", "Please rename var")])
                    ],
                ),
            ],
        )
        self.assertEqual(res.returncode, 3, msg=res.stderr)
        self.assertIn("Unresolved thread [comment_id=20] b.py:20", res.stdout)
        self.assertIn("CI GREEN. Awaiting human", res.stdout)
        self.assertNotIn("SUPER GREEN", res.stdout)

        gql_queries = [
            " ".join(c) for c in self._read_calls()
            if c[:2] == ["api", "graphql"]
        ]
        self.assertEqual(len(gql_queries), 3)
        self.assertIn("checksAfter=chk_1", gql_queries[1])
        self.assertIn("withThreads=false", gql_queries[1])
        self.assertIn("threadsAfter=thr_1", gql_queries[2])
        self.assertIn("withChecks=false", gql_queries[2])

    def test_cli_ci_green_changes_requested_exits_3(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "107"],
            graphql_pages=[
                _pr_payload(
                    107,
                    [_check_run("build")],
                    review_decision="CHANGES_REQUESTED",
                )
            ],
        )
        self.assertEqual(res.returncode, 3, msg=res.stderr)
        self.assertIn("CI GREEN. Awaiting human", res.stdout)
        self.assertNotIn("SUPER GREEN", res.stdout)

    def test_cli_badge_stripping_and_non_author_comment_selection(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "108", "--no-download"],
            graphql_pages=[
                _pr_payload(
                    108,
                    [
                        _check_run(
                            "build", status="IN_PROGRESS", conclusion=None)
                    ],
                    author="oxve",
                    threads=[
                        _thread(
                            "foo.cc",
                            42,
                            [
                                _comment(
                                    111,
                                    "gemini-code-assist",
                                    "![high](https://example.com/high.svg)\n\n"
                                    "Check null pointer before dereference.",
                                ),
                                _comment(222, "oxve", "Will fix shortly."),
                            ],
                        )
                    ],
                )
            ],
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn(
            "Unresolved thread [comment_id=111] foo.cc:42 by "
            "@gemini-code-assist: Check null pointer before dereference.",
            res.stdout,
        )

    def test_cli_ignore_comment_ids_watch_then_ci_green(self):
        thread = _thread("foo.cc", 12, [_comment(111, "rev", "Fix style")])
        res = self.run_cli(
            [
                "--repo", "org/repo", "--pr", "109", "--watch", "--interval",
                "0", "--ignore-comment-ids", "111"
            ],
            graphql_pages=[
                _pr_payload(
                    109,
                    [
                        _check_run(
                            "build", status="IN_PROGRESS", conclusion=None)
                    ],
                    threads=[thread],
                ),
                _pr_payload(109, [_check_run("build")], threads=[thread]),
            ],
        )
        self.assertEqual(res.returncode, 3, msg=res.stderr)
        self.assertIn("CI GREEN. Awaiting human", res.stdout)
        self.assertNotIn("SUPER GREEN", res.stdout)

        # A new unignored comment still interrupts --watch immediately.
        res_new = self.run_cli(
            [
                "--repo", "org/repo", "--pr", "109", "--watch", "--interval",
                "0", "--ignore-comment-ids", "111"
            ],
            graphql_pages=[
                _pr_payload(
                    109,
                    [
                        _check_run(
                            "build", status="IN_PROGRESS", conclusion=None)
                    ],
                    threads=[
                        thread,
                        _thread("bar.cc", 9,
                                [_comment(333, "rev", "New ask")]),
                    ],
                )
            ],
        )
        self.assertEqual(res_new.returncode, 1)
        self.assertIn("comment_id=333", res_new.stdout)
        self.assertNotIn("comment_id=111", res_new.stdout)

    def test_cli_targeted_downloads_collision_safe_logs_and_clean_staging(
            self):
        stale_log = os.path.join(self.test_dir, "artifacts", "logs",
                                 "stale.log")
        stale_rep = os.path.join(self.test_dir, "artifacts", "test_results",
                                 "stale_dir", "shepherd_report.json")
        os.makedirs(os.path.dirname(stale_log), exist_ok=True)
        os.makedirs(os.path.dirname(stale_rep), exist_ok=True)
        with open(stale_log, "w", encoding="utf-8") as f:
            f.write("old")
        with open(stale_rep, "w", encoding="utf-8") as f:
            f.write("{}")

        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "601"],
            graphql_pages=[
                _pr_payload(
                    601,
                    [
                        _check_run("non-req",
                                   conclusion="FAILURE",
                                   run_id=700,
                                   job_id=7001,
                                   is_required=False),
                        _check_run(
                            "linux / test",
                            conclusion="FAILURE",
                            url=("https://github.com/org/repo/actions/"
                                 "runs/801/job/1234567"),
                            run_id=801,
                            job_id=1234567,
                        ),
                        _check_run(
                            "linux / test",
                            conclusion="FAILURE",
                            run_id=801,
                            job_id=7654321,
                        ),
                    ],
                    merge_state="BLOCKED",
                )
            ],
            forbidden_run_ids=["700"],
            reports=[
                {
                    "run_id": "801",
                    "platform": "passing-sibling",
                    "data": {
                        "platform": "passing-sibling",
                        "workflow": "linux",
                        "checks": {
                            "build": "success"
                        },
                    },
                },
                {
                    "run_id": "801",
                    "platform": "linux-modular",
                    "data": {
                        "platform": "linux-modular",
                        "workflow": "linux",
                        "checks": {
                            "build": "success",
                            "on-host-test": "failure"
                        },
                        "test_failures": {
                            "failing_tests": {
                                "results/test_results.xml": [{
                                    "name":
                                    "DemoTest.AssertionFailure",
                                    "message":
                                    "Expected true, got false",
                                }]
                            }
                        },
                    },
                },
            ],
            logs={
                "7654321": "FAILED STEP LOG FOR JOB 7654321\nAssertionError\n"
            },
        )
        self.assertEqual(res.returncode, 1)
        self.assertIn("[PR #601] branch-601 -> main (sha: sha_601)",
                      res.stdout)
        self.assertIn("run_id=801 job_id=7654321", res.stdout)
        self.assertIn(
            "Report [linux-modular] (linux): failing_jobs=on-host-test",
            res.stdout)
        self.assertIn("DemoTest.AssertionFailure", res.stdout)
        self.assertIn("Artifacts: artifacts", res.stdout)

        self.assertFalse(os.path.exists(stale_log))
        self.assertFalse(os.path.exists(stale_rep))
        self.assertFalse(
            os.path.exists(
                os.path.join(self.test_dir, "artifacts", "test_results",
                             "shepherd-report-passing-sibling")))
        log_file = os.path.join(self.test_dir, "artifacts", "logs",
                                "linux___test_7654321.log")
        self.assertTrue(os.path.exists(log_file))
        report_file = os.path.join(self.test_dir, "artifacts", "test_results",
                                   "shepherd-report-linux-modular",
                                   "shepherd_report.json")
        self.assertTrue(os.path.exists(report_file))

    def test_cli_no_download_flag(self):
        res = self.run_cli(
            ["--repo", "org/repo", "--pr", "603", "--no-download"],
            graphql_pages=[
                _pr_payload(
                    603,
                    [
                        _check_run("failing-job",
                                   conclusion="FAILURE",
                                   run_id=803,
                                   job_id=11111)
                    ],
                )
            ],
        )
        self.assertEqual(res.returncode, 1)
        self.assertNotIn("Artifacts:", res.stdout)
        self.assertFalse(
            os.path.exists(os.path.join(self.test_dir, "artifacts")))

    def test_cli_watch_exits_immediately_on_conflict_draft_closed(self):
        for kwargs in (
            {
                "mergeable": "CONFLICTING",
                "merge_state": "DIRTY"
            },
            {
                "is_draft": True
            },
            {
                "state": "CLOSED"
            },
        ):
            res = self.run_cli(
                [
                    "--repo", "org/repo", "--pr", "300", "--watch",
                    "--interval", "0"
                ],
                graphql_pages=[
                    _pr_payload(
                        300,
                        [
                            _check_run(
                                "build", status="IN_PROGRESS", conclusion=None)
                        ],
                        **kwargs,
                    )
                ],
            )
            self.assertEqual(res.returncode, 1)

    def test_cli_bounded_errors_exit_4_and_recovery(self):
        res_single = self.run_cli(
            ["--repo", "org/repo", "--pr", "400"],
            error="GraphQL error: Not found\n",
        )
        self.assertEqual(res_single.returncode, 4)

        res_watch = self.run_cli(
            [
                "--repo", "org/repo", "--pr", "401", "--watch", "--interval",
                "0"
            ],
            error="transient network timeout\n",
        )
        self.assertEqual(res_watch.returncode, 4)
        with open(self.count_path, "r", encoding="utf-8") as f:
            self.assertEqual(int(f.read()), 3)

        res_recover = self.run_cli(
            [
                "--repo", "org/repo", "--pr", "402", "--watch", "--interval",
                "0"
            ],
            graphql_errors=["err1\n", "err2\n"],
            graphql_pages=[
                _pr_payload(402, [_check_run("build")]),
                _pr_payload(402, [_check_run("build")]),
                _pr_payload(402, [_check_run("build")]),
            ],
        )
        self.assertEqual(res_recover.returncode, 0, msg=res_recover.stderr)

    def test_cli_resolve_prs_passes_repo(self):
        res = self.run_cli(
            ["--repo", "org/repo"],
            default_pr=222,
            graphql_pages=[_pr_payload(222, [_check_run("build")])],
        )
        self.assertEqual(res.returncode, 0, msg=res.stderr)
        pr_view_calls = [
            c for c in self._read_calls() if c[:2] == ["pr", "view"]
        ]
        self.assertEqual(len(pr_view_calls), 1)
        self.assertIn("--repo", pr_view_calls[0])
        self.assertIn("org/repo", pr_view_calls[0])


if __name__ == "__main__":
    unittest.main()
