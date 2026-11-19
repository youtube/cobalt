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

import json
import os
import subprocess
import sys
import tempfile
import unittest

_SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
_SCRIPT_PATH = os.path.join(_SCRIPTS_DIR, "shepherd_pr.py")

_MOCK_GH_SCRIPT = """#!/usr/bin/env python3
import json, os, sys

with open(os.environ["MOCK_GH_CONFIG"], "r", encoding="utf-8") as config_file:
  config = json.load(config_file)

argv = sys.argv[1:]
if argv[:2] == ["api", "graphql"]:
  if config.get("error"):
    sys.stderr.write(config["error"])
    sys.exit(1)
  count_path = config["count_path"]
  idx = 0
  if os.path.exists(count_path):
    with open(count_path, "r", encoding="utf-8") as count_file:
      idx = int(count_file.read())
  with open(count_path, "w", encoding="utf-8") as count_file:
    count_file.write(str(idx + 1))
  pages = config["graphql_pages"]
  sys.stdout.write(json.dumps(pages[min(idx, len(pages) - 1)]))
  sys.exit(0)

if argv[:2] == ["run", "list"]:
  sys.stdout.write(json.dumps(config.get("runs", [])))
  sys.exit(0)

if argv[:2] == ["run", "download"]:
  run_id = argv[2]
  dest_dir = argv[argv.index("-D") + 1]
  for entry in config.get("reports", []):
    if entry.get("run_id", run_id) == run_id:
      report_dir = os.path.join(dest_dir, f"shepherd-report-{entry['platform']}")
      os.makedirs(report_dir, exist_ok=True)
      with open(os.path.join(report_dir, "shepherd_report.json"), "w", encoding="utf-8") as out:
        json.dump(entry["data"], out)
  sys.exit(0)

if argv[:2] == ["run", "view"]:
  if "--job" in argv:
    job_id = argv[argv.index("--job") + 1]
    logs = config.get("logs", {})
    if job_id in logs:
      sys.stdout.write(logs[job_id])
      sys.exit(0)
  if "--json" in argv and "jobs" in argv:
    run_id = argv[2]
    sys.stdout.write(json.dumps({"jobs": config.get("run_jobs", {}).get(run_id, [])}))
    sys.exit(0)
  sys.exit(1)

sys.stderr.write(f"Unexpected command: {argv}\\n")
sys.exit(1)
"""


def _pr_payload(
    number,
    checks,
    mergeable="MERGEABLE",
    merge_state="CLEAN",
    review_decision="APPROVED",
    has_next_page=False,
    end_cursor=None,
):
  return {
      "data": {
          "repository": {
              "pullRequest": {
                  "number": number,
                  "title": f"PR {number}",
                  "state": "OPEN",
                  "isDraft": False,
                  "reviewDecision": review_decision,
                  "url": f"https://github.com/org/repo/pull/{number}",
                  "headRefOid": f"sha_{number}",
                  "headRefName": f"branch-{number}",
                  "mergeable": mergeable,
                  "mergeStateStatus": merge_state,
                  "statusCheckRollup": {
                      "contexts": {
                          "nodes": checks,
                          "pageInfo": {
                              "hasNextPage": has_next_page,
                              "endCursor": end_cursor,
                          },
                      }
                  },
                  "reviewThreads": {"nodes": []},
              }
          }
      }
  }


def _check_run(name, status="COMPLETED", conclusion="SUCCESS", url="", job_id=None):
  node = {
      "__typename": "CheckRun",
      "name": name,
      "status": status,
      "conclusion": conclusion,
      "detailsUrl": url,
  }
  if job_id is not None:
    node["databaseId"] = job_id
  return node


class TestPRShepherdCLI(unittest.TestCase):
  """Black-box command-line integration test suite for shepherd_pr.py."""

  def setUp(self):
    super().setUp()
    self.test_dir = tempfile.TemporaryDirectory()
    self.bin_dir = os.path.join(self.test_dir.name, "bin")
    os.makedirs(self.bin_dir, exist_ok=True)
    mock_gh = os.path.join(self.bin_dir, "gh")
    with open(mock_gh, "w", encoding="utf-8") as gh_file:
      gh_file.write(_MOCK_GH_SCRIPT)
    os.chmod(mock_gh, 0o755)

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def run_cli(self, args, **mock_spec):
    config_path = os.path.join(self.test_dir.name, "mock_gh.json")
    count_path = os.path.join(self.test_dir.name, "gql_count.txt")
    if os.path.exists(count_path):
      os.remove(count_path)
    with open(config_path, "w", encoding="utf-8") as config_file:
      json.dump({"count_path": count_path, **mock_spec}, config_file)

    env = os.environ.copy()
    env["PATH"] = f"{self.bin_dir}:{env['PATH']}"
    env["MOCK_GH_CONFIG"] = config_path

    return subprocess.run(
        [sys.executable, _SCRIPT_PATH, *args],
        capture_output=True,
        text=True,
        env=env,
        cwd=self.test_dir.name,
        check=False,
    )

  def test_cli_single_run_green(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "101"],
        graphql_pages=[_pr_payload(101, [_check_run("build", url="https://url/build")])],
    )
    self.assertEqual(res.returncode, 0, msg=res.stderr)
    self.assertIn("[PR #101] Tracking started.", res.stdout)
    self.assertIn("SUPER GREEN! Multipass authorized.", res.stdout)

  def test_cli_ci_shepherd_report_download_and_triage(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "102"],
        graphql_pages=[_pr_payload(
            102,
            [_check_run("validate-result (linux-modular)", conclusion="FAILURE", url="https://url/check")],
            merge_state="BLOCKED",
        )],
        runs=[{"databaseId": 501, "workflowName": "linux"}],
        reports=[{
            "run_id": "501",
            "platform": "linux-modular",
            "data": {
                "platform": "linux-modular",
                "workflow": "linux",
                "run_url": "https://github.com/org/repo/actions/runs/501",
                "checks": {"initialize": "success", "build": "success", "on-host-test": "failure"},
                "test_failures": {
                    "failing_tests": {
                        "results/test_results.xml": [{
                            "name": "OzoneTest.CursorCheck",
                            "message": "Expected 1, got 0\nStack trace line",
                        }]
                    }
                },
            },
        }],
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("[PR #102] Tracking started.", res.stdout)
    self.assertIn("Failed check 'validate-result (linux-modular)' is FAILURE", res.stdout)
    self.assertIn("CI Shepherd Report [linux-modular] (linux)", res.stdout)
    self.assertIn("Failing jobs: on-host-test", res.stdout)
    self.assertIn("OzoneTest.CursorCheck", res.stdout)

  def test_cli_run_attempt_deduplication(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "103", "--no-download"],
        graphql_pages=[_pr_payload(
            103,
            [_check_run("validate-result (linux)", conclusion="FAILURE", url="https://url/check")],
            merge_state="BLOCKED",
        )],
        runs=[
            {"databaseId": 10, "workflowName": "linux"},
            {"databaseId": 11, "workflowName": "linux"},
        ],
        reports=[
            {
                "run_id": "10",
                "platform": "linux",
                "data": {"platform": "linux", "workflow": "linux", "run_attempt": 1, "checks": {"old-job": "failure"}},
            },
            {
                "run_id": "11",
                "platform": "linux",
                "data": {"platform": "linux", "workflow": "linux", "run_attempt": 2, "checks": {"new-job": "failure"}},
            },
        ],
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("Failing jobs: new-job", res.stdout)
    self.assertNotIn("old-job", res.stdout)

  def test_cli_status_check_rollup_pagination(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "104"],
        graphql_pages=[
            _pr_payload(104, [_check_run("build-1")], has_next_page=True, end_cursor="cursor_1"),
            _pr_payload(104, [_check_run("build-2")]),
        ],
    )
    self.assertEqual(res.returncode, 0, msg=res.stderr)
    self.assertIn("Checks: 2,", res.stdout)
    self.assertIn("SUPER GREEN! Multipass authorized.", res.stdout)

  def test_cli_merge_conflict_warning(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "300"],
        graphql_pages=[_pr_payload(300, [_check_run("build")], mergeable="CONFLICTING", merge_state="DIRTY")],
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("WARNING: PR has merge conflicts! (State: DIRTY)", res.stdout)

  def test_cli_gh_error_handling(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "400"],
        error="GraphQL error: Not found\n",
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("[PR #400] Error: GraphQL error: Not found", res.stderr)

  def test_cli_watch_breaks_on_initial_failed_job(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "500", "--watch", "--interval", "1"],
        graphql_pages=[_pr_payload(
            500,
            [_check_run("test-linux", conclusion="FAILURE", url="https://url/fail")],
            merge_state="BLOCKED",
        )],
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("[PR #500] Tracking started.", res.stdout)
    self.assertIn("Failed check 'test-linux' is FAILURE", res.stdout)
    self.assertIn("ACTION REQUIRED: Triage failure immediately!", res.stdout)

  def test_cli_watch_breaks_on_subsequent_failed_job(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "501", "--watch", "--interval", "1"],
        graphql_pages=[
            _pr_payload(501, [_check_run("build-job", status="IN_PROGRESS", conclusion="PENDING", url="https://url/build")]),
            _pr_payload(501, [_check_run("build-job", conclusion="FAILURE", url="https://url/build")], merge_state="BLOCKED"),
        ],
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("[PR #501] Tracking started.", res.stdout)
    self.assertIn("Check 'build-job' changed IN_PROGRESS -> FAILURE", res.stdout)
    self.assertIn("ACTION REQUIRED: Triage failure immediately!", res.stdout)

  def test_cli_watch_continues_pending_until_green(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "502", "--watch", "--interval", "1"],
        graphql_pages=[
            _pr_payload(502, [_check_run("build-job", status="IN_PROGRESS", conclusion="PENDING", url="https://url/build")]),
            _pr_payload(502, [_check_run("build-job", conclusion="SUCCESS", url="https://url/build")]),
        ],
    )
    self.assertEqual(res.returncode, 0, msg=res.stderr)
    self.assertIn("[PR #502] Tracking started.", res.stdout)
    self.assertIn("Check 'build-job' changed IN_PROGRESS -> SUCCESS", res.stdout)
    self.assertIn("SUPER GREEN! Multipass authorized.", res.stdout)

  def test_cli_downloads_failure_artifacts_and_logs(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "601"],
        graphql_pages=[_pr_payload(
            601,
            [_check_run(
                "test-linux-modular",
                conclusion="FAILURE",
                url="https://github.com/org/repo/actions/runs/801/job/1234567",
                job_id=1234567,
            )],
            merge_state="BLOCKED",
        )],
        runs=[{"databaseId": 801, "workflowName": "linux"}],
        reports=[{
            "run_id": "801",
            "platform": "linux-modular",
            "data": {
                "platform": "linux-modular",
                "workflow": "linux",
                "run_url": "https://github.com/org/repo/actions/runs/801",
                "checks": {"build": "success", "on-host-test": "failure"},
                "test_failures": {
                    "failing_tests": {
                        "results/test_results.xml": [{"name": "DemoTest.AssertionFailure", "message": "Expected true, got false"}]
                    }
                },
            },
        }],
        logs={"1234567": "FAILED STEP LOG FOR JOB 1234567\nAssertionError\n"},
        run_jobs={"801": [{"name": "on-host-test", "databaseId": 9999}]},
    )
    self.assertEqual(res.returncode, 1)
    self.assertIn("Downloaded test results and logs for failed jobs to 'artifacts'", res.stdout)
    log_file = os.path.join(self.test_dir.name, "artifacts", "logs", "test-linux-modular.log")
    with open(log_file, "r", encoding="utf-8") as log_handle:
      self.assertIn("FAILED STEP LOG FOR JOB 1234567", log_handle.read())
    report_file = os.path.join(
        self.test_dir.name, "artifacts", "test_results", "shepherd-report-linux-modular", "shepherd_report.json"
    )
    with open(report_file, "r", encoding="utf-8") as report_handle:
      self.assertEqual(json.load(report_handle)["platform"], "linux-modular")

  def test_cli_downloads_to_custom_artifacts_dir(self):
    for flag in ("--artifacts-dir", "--download-dir"):
      custom_dir = os.path.join(self.test_dir.name, f"custom_{flag.strip('-')}")
      res = self.run_cli(
          ["--repo", "org/repo", "--pr", "602", flag, custom_dir],
          graphql_pages=[_pr_payload(
              602,
              [_check_run(
                  "build-check",
                  conclusion="FAILURE",
                  url="https://github.com/org/repo/actions/runs/802/job/7654321",
                  job_id=7654321,
              )],
              merge_state="BLOCKED",
          )],
          logs={"7654321": "COMPILATION ERROR: file.cc:10: error\n"},
      )
      self.assertEqual(res.returncode, 1)
      self.assertIn(f"Downloaded test results and logs for failed jobs to '{custom_dir}'", res.stdout)
      log_file = os.path.join(custom_dir, "logs", "build-check.log")
      with open(log_file, "r", encoding="utf-8") as log_handle:
        self.assertIn("COMPILATION ERROR", log_handle.read())

  def test_cli_no_download_flag(self):
    res = self.run_cli(
        ["--repo", "org/repo", "--pr", "603", "--no-download"],
        graphql_pages=[_pr_payload(
            603,
            [_check_run(
                "failing-job",
                conclusion="FAILURE",
                url="https://github.com/org/repo/actions/runs/803/job/11111",
                job_id=11111,
            )],
            merge_state="BLOCKED",
        )],
    )
    self.assertEqual(res.returncode, 1)
    self.assertNotIn("Downloaded test results and logs", res.stdout)
    self.assertFalse(os.path.exists(os.path.join(self.test_dir.name, "artifacts")))


if __name__ == "__main__":
  unittest.main()
