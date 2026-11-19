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
"""PR Shepherd: Minimal GitHub PR monitor for CI checks and review comments."""

# pylint: disable=bad-indentation

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

GQL_QUERY = """
query($owner: String!, $name: String!, $pr: Int!, $after: String) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $pr) {
      number title state isDraft reviewDecision url mergeable mergeStateStatus
      headRefName headRefOid
      statusCheckRollup {
        contexts(first: 100, after: $after) {
          nodes {
            __typename
            ... on CheckRun { name status conclusion detailsUrl databaseId }
            ... on StatusContext { context state targetUrl }
          }
          pageInfo { hasNextPage endCursor }
        }
      }
      reviewThreads(first: 100) {
        nodes {
          isResolved path line
          comments(last: 1) { nodes { databaseId author { login } body url } }
        }
      }
    }
  }
}"""

SUCCESS_STATES = {"SUCCESS", "NEUTRAL", "SKIPPED"}
PENDING_STATES = {"PENDING", "EXPECTED", "IN_PROGRESS", "QUEUED"}
EXCLUDED_CI_WORKFLOWS = {
    "lint",
    "gemini commit message generator",
    "scorecards supply-chain security",
    "workflow debug",
    "outside collaborator",
    "auto label kokoro",
    "pr badges",
    "nightly_trigger",
    "label cherry pick",
}


def is_failed_state(state: str) -> bool:
    return state not in SUCCESS_STATES and state not in PENDING_STATES


def gh_run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["gh", *args],
                          capture_output=True,
                          text=True,
                          check=False)


def gh_call(*args: str) -> str:
    result = gh_run(*args)
    if result.returncode != 0:
        cmd_str = " ".join(args)
        raise RuntimeError(result.stderr.strip()
                           or f"Command failed: gh {cmd_str}")
    return result.stdout.strip()


def resolve_repo(repo_arg: str | None) -> tuple[str, str]:
    raw = repo_arg or gh_call("repo", "view", "--json", "nameWithOwner", "-q",
                              ".nameWithOwner")
    owner, name = raw.split("/")
    return owner, name


def resolve_prs(pr_arg: str | None) -> list[int]:
    if pr_arg:
        return [
            int(item.strip()) for item in pr_arg.split(",") if item.strip()
        ]
    return [int(gh_call("pr", "view", "--json", "number", "-q", ".number"))]


def parse_test_failures(raw_tests: dict | None) -> list[dict]:
    if not raw_tests:
        return []
    tests_by_target = raw_tests.get("failing_tests", raw_tests)
    failures = []
    for items in tests_by_target.values():
        for item in items:
            failures.append({
                "name": item["name"],
                "message": (item.get("message") or "").strip(),
            })
    return failures


def parse_shepherd_report(filepath: str) -> dict:
    with open(filepath, "r", encoding="utf-8") as report_file:
        data = json.load(report_file)
    checks = data["checks"]
    failing_jobs = [
        job for job, status in checks.items()
        if status in ("failure", "cancelled")
    ]
    test_failures = parse_test_failures(data.get("test_failures"))
    return {
        "platform": data["platform"],
        "workflow": data.get("workflow", ""),
        "run_url": data.get("run_url", ""),
        "run_attempt": int(data.get("run_attempt") or 1),
        "failing_jobs": failing_jobs,
        "test_failures": test_failures,
        "has_failures": bool(failing_jobs or test_failures),
    }


def fetch_ci_shepherd_reports(owner: str,
                              name: str,
                              head_sha: str,
                              results_dir: str | None = None) -> list[dict]:
    runs = json.loads(
        gh_call(
            "run",
            "list",
            "--repo",
            f"{owner}/{name}",
            "--commit",
            head_sha,
            "--json",
            "databaseId,workflowName",
        ))
    winners = {}

    with tempfile.TemporaryDirectory(prefix="shepherd_reports_") as tmpdir:
        for run in runs:
            if (run["workflowName"] or "").lower() in EXCLUDED_CI_WORKFLOWS:
                continue
            run_id = str(run["databaseId"])
            gh_run(
                "run",
                "download",
                run_id,
                "--repo",
                f"{owner}/{name}",
                "-p",
                "shepherd-report-*",
                "-D",
                os.path.join(tmpdir, run_id),
            )

        for report_path in sorted(
                glob.glob(os.path.join(tmpdir, "**", "shepherd_report.json"),
                          recursive=True)):
            report = parse_shepherd_report(report_path)
            platform = report["platform"]
            prev = winners.get(platform)
            if prev is None or report["run_attempt"] >= prev[0]["run_attempt"]:
                winners[platform] = (report, os.path.dirname(report_path))

        if results_dir and winners:
            os.makedirs(results_dir, exist_ok=True)
            for report, report_dir in winners.values():
                shutil.copytree(
                    report_dir,
                    os.path.join(results_dir, os.path.basename(report_dir)),
                    dirs_exist_ok=True,
                )

    return [report for report, report_dir in winners.values()]


def format_failed_report(report: dict) -> list[str]:
    workflow = report["workflow"] or "unknown"
    platform = report["platform"]
    run_url = report["run_url"]
    url_str = f" - {run_url}" if run_url else ""
    lines = [f"CI Shepherd Report [{platform}] ({workflow}){url_str}:"]
    if report["failing_jobs"]:
        failing_jobs = ", ".join(report["failing_jobs"])
        lines.append(f"  Failing jobs: {failing_jobs}")
    if report["test_failures"]:
        num_failures = len(report["test_failures"])
        lines.append(f"  Failing tests ({num_failures}):")
        for test_fail in report["test_failures"]:
            test_name = test_fail["name"]
            msg_lines = test_fail["message"].splitlines()
            if msg_lines and msg_lines[0]:
                first_line = msg_lines[0][:80]
                lines.append(f"    - {test_name}: {first_line}")
            else:
                lines.append(f"    - {test_name}")
    return lines


def fetch_pr_status(owner: str, name: str,
                    pr_number: int) -> tuple[dict, list[dict]]:
    pr_data = None
    all_nodes = []
    after = None
    while True:
        args = [
            "api",
            "graphql",
            "-f",
            f"query={GQL_QUERY}",
            "-f",
            f"owner={owner}",
            "-f",
            f"name={name}",
            "-F",
            f"pr={pr_number}",
        ]
        if after:
            args.extend(["-f", f"after={after}"])
        pull_request = json.loads(
            gh_call(*args))["data"]["repository"]["pullRequest"]
        if pr_data is None:
            pr_data = pull_request
        rollup = pull_request["statusCheckRollup"]
        if rollup is None:
            break
        contexts = rollup["contexts"]
        all_nodes.extend(contexts["nodes"])
        page_info = contexts["pageInfo"]
        if not page_info["hasNextPage"]:
            break
        after = page_info["endCursor"]
    return pr_data, all_nodes


def _save_log(logs_dir: str, filename_base: str, *gh_args: str) -> None:
    result = gh_run(*gh_args)
    if result.returncode == 0 and result.stdout:
        os.makedirs(logs_dir, exist_ok=True)
        safe_name = re.sub(r"[^a-zA-Z0-9_.-]", "_", filename_base).strip("_")
        file_stem = safe_name or "check"
        with open(os.path.join(logs_dir, f"{file_stem}.log"),
                  "w",
                  encoding="utf-8") as log_file:
            log_file.write(result.stdout)


def download_failed_logs(owner: str, name: str, checks: dict[str, dict],
                         ci_reports: list[dict], logs_dir: str) -> None:
    repo = f"{owner}/{name}"
    for check_name, check_info in checks.items():
        if not is_failed_state(check_info["state"]):
            continue
        url = check_info["url"]
        job_id = check_info["job_id"]
        if not job_id:
            job_match = re.search(r"/job/(\d+)", url)
            if job_match:
                job_id = job_match.group(1)
        if job_id:
            _save_log(logs_dir, check_name, "run", "view", "--job", job_id,
                      "--log-failed", "--repo", repo)
        else:
            run_match = re.search(r"/actions/runs/(\d+)", url)
            if run_match:
                _save_log(logs_dir, check_name, "run", "view",
                          run_match.group(1), "--log-failed", "--repo", repo)

    for report in ci_reports:
        if not report["has_failures"] or not report["failing_jobs"]:
            continue
        run_match = re.search(r"/actions/runs/(\d+)", report["run_url"])
        if not run_match:
            continue
        jobs_res = gh_run("run", "view", run_match.group(1), "--json", "jobs",
                          "--repo", repo)
        if jobs_res.returncode != 0 or not jobs_res.stdout:
            continue
        jobs_by_name = {
            job["name"]: str(job["databaseId"])
            for job in json.loads(jobs_res.stdout)["jobs"]
        }
        for job_name in report["failing_jobs"]:
            if job_name in jobs_by_name:
                platform = report["platform"]
                _save_log(
                    logs_dir,
                    f"{platform}_{job_name}",
                    "run",
                    "view",
                    "--job",
                    jobs_by_name[job_name],
                    "--log-failed",
                    "--repo",
                    repo,
                )


def evaluate_pr(owner: str,
                name: str,
                pr_number: int,
                artifacts_dir: str | None = None) -> dict:
    pr, contexts = fetch_pr_status(owner, name, pr_number)
    head_sha = pr["headRefOid"] or ""

    checks = {}
    for node in contexts:
        if node["__typename"] == "CheckRun":
            check_name = node["name"]
            state = (node["conclusion"] or "PENDING"
                     ) if node["status"] == "COMPLETED" else node["status"]
            url = node["detailsUrl"] or ""
            job_id = str(node["databaseId"]) if node.get("databaseId") else ""
        else:
            check_name = node["context"]
            state = node["state"]
            url = node["targetUrl"] or ""
            job_id = ""
        checks[check_name] = {"state": state, "url": url, "job_id": job_id}

    unresolved = []
    for thread in pr["reviewThreads"]["nodes"]:
        if thread["isResolved"]:
            continue
        path = thread["path"]
        line = thread["line"]
        location = f"{path}:{line}" if line else path
        comments = thread["comments"]["nodes"]
        last_comment = comments[-1] if comments else {}
        author = (last_comment.get("author") or {}).get("login", "unknown")
        raw_body = last_comment.get("body") or ""
        body = raw_body.splitlines()[0][:80] if raw_body else ""
        unresolved.append(f"{location} by @{author}: {body}")

    decision = pr["reviewDecision"] or "NONE"
    mergeable = pr["mergeable"]
    has_failed_checks = any(
        is_failed_state(info["state"]) for info in checks.values())
    ci_reports = []
    if has_failed_checks and head_sha:
        results_dir = os.path.join(artifacts_dir,
                                   "test_results") if artifacts_dir else None
        ci_reports = fetch_ci_shepherd_reports(owner,
                                               name,
                                               head_sha,
                                               results_dir=results_dir)

    has_failures = has_failed_checks or any(report["has_failures"]
                                            for report in ci_reports)
    downloaded_to = None
    if has_failures and artifacts_dir:
        download_failed_logs(owner, name, checks, ci_reports,
                             os.path.join(artifacts_dir, "logs"))
        downloaded_to = artifacts_dir

    is_green = (bool(checks) and all(info["state"] in SUCCESS_STATES
                                     for info in checks.values())
                and not unresolved and decision != "CHANGES_REQUESTED"
                and not pr["isDraft"] and pr["state"] == "OPEN"
                and mergeable == "MERGEABLE")

    return {
        "pr": pr_number,
        "is_green": is_green,
        "has_failures": has_failures,
        "downloaded_to": downloaded_to,
        "checks": checks,
        "ci_shepherd_reports": ci_reports,
        "unresolved_threads": unresolved,
        "decision": decision,
        "mergeable": mergeable,
        "merge_state_status": pr["mergeStateStatus"],
    }


def print_initial_summary(pr_num: int, curr: dict) -> None:
    num_checks = len(curr["checks"])
    num_threads = len(curr["unresolved_threads"])
    mergeable = curr["mergeable"]
    merge_state = curr["merge_state_status"]
    downloaded_to = curr["downloaded_to"]
    print(f"[PR #{pr_num}] Tracking started. Checks: {num_checks}, "
          f"Threads: {num_threads}, Mergeable: {mergeable}")
    if mergeable == "CONFLICTING":
        print(f"[PR #{pr_num}] WARNING: PR has merge conflicts! "
              f"(State: {merge_state})")
    for check_name, check_info in curr["checks"].items():
        state = check_info["state"]
        if is_failed_state(state):
            url = check_info["url"]
            url_str = f" (URL: {url})" if url else ""
            print(f"[PR #{pr_num}] Failed check '{check_name}' "
                  f"is {state}{url_str}")
    for report in curr["ci_shepherd_reports"]:
        if report["has_failures"]:
            for line in format_failed_report(report):
                print(f"[PR #{pr_num}] {line}")
    for thread in curr["unresolved_threads"]:
        print(f"[PR #{pr_num}] Open thread: {thread}")
    if downloaded_to:
        print(f"[PR #{pr_num}] Downloaded test results and logs "
              f"for failed jobs to '{downloaded_to}'")


def print_diff(pr_num: int, prev: dict, curr: dict) -> None:
    changes = []
    for check_name, check_info in curr["checks"].items():
        state = check_info["state"]
        url = check_info["url"]
        url_str = f" (URL: {url})" if is_failed_state(state) and url else ""
        if check_name not in prev["checks"]:
            changes.append(f"New check '{check_name}' is {state}{url_str}")
        elif prev["checks"][check_name] != check_info:
            prev_state = prev["checks"][check_name]["state"]
            changes.append(f"Check '{check_name}' changed {prev_state} "
                           f"-> {state}{url_str}")

    prev_reports = {
        report["platform"]: report
        for report in prev["ci_shepherd_reports"]
    }
    for report in curr["ci_shepherd_reports"]:
        platform = report["platform"]
        prev_report = prev_reports.get(platform)
        if (not prev_report
                or prev_report["failing_jobs"] != report["failing_jobs"]
                or len(prev_report["test_failures"]) != len(
                    report["test_failures"])):
            if report["has_failures"]:
                changes.extend(format_failed_report(report))
            elif prev_report and prev_report["has_failures"]:
                changes.append(f"CI Shepherd Report [{platform}]: "
                               "Failures resolved (ALL PASS)")

    for thread in sorted(
            set(curr["unresolved_threads"]) - set(prev["unresolved_threads"])):
        changes.append(f"New thread: {thread}")
    for thread in sorted(
            set(prev["unresolved_threads"]) - set(curr["unresolved_threads"])):
        changes.append(f"Resolved thread: {thread}")

    prev_decision = prev["decision"]
    curr_decision = curr["decision"]
    if prev_decision != curr_decision:
        changes.append(f"Review decision: {prev_decision} -> {curr_decision}")

    prev_mergeable = prev["mergeable"]
    curr_mergeable = curr["mergeable"]
    prev_merge_state = prev["merge_state_status"]
    curr_merge_state = curr["merge_state_status"]
    if prev_mergeable != curr_mergeable or prev_merge_state != curr_merge_state:
        msg = (
            f"Mergeable status changed: {prev_mergeable} "
            f"({prev_merge_state}) -> {curr_mergeable} ({curr_merge_state})")
        if curr_mergeable == "CONFLICTING":
            msg += " - WARNING: Merge conflicts detected!"
        changes.append(msg)

    downloaded_to = curr["downloaded_to"]
    if downloaded_to:
        changes.append(f"Downloaded test results and logs "
                       f"for failed jobs to '{downloaded_to}'")

    for change in changes:
        print(f"[PR #{pr_num}] {change}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="PR Shepherd: Minimal PR monitor.")
    parser.add_argument("--repo", help="Repository in owner/name format.")
    parser.add_argument("--pr", help="PR number(s), comma-separated.")
    parser.add_argument("--watch",
                        action="store_true",
                        help="Watch until green.")
    parser.add_argument("--interval",
                        type=int,
                        default=30,
                        help="Poll interval in seconds.")
    parser.add_argument(
        "--artifacts-dir",
        "--download-dir",
        dest="artifacts_dir",
        default="artifacts",
        help="Directory for downloaded failure logs and test results.",
    )
    parser.add_argument(
        "--no-download",
        action="store_true",
        help="Disable automatic download of failure artifacts and logs.",
    )
    args = parser.parse_args()

    owner, repo_name = resolve_repo(args.repo)
    pr_numbers = resolve_prs(args.pr)
    prev_states = {}

    while True:
        all_green = True
        has_failure = False
        for pr_num in pr_numbers:
            if args.no_download:
                target_dir = None
            elif len(pr_numbers) > 1:
                target_dir = os.path.join(args.artifacts_dir, f"pr_{pr_num}")
            else:
                target_dir = args.artifacts_dir

            try:
                curr = evaluate_pr(owner,
                                   repo_name,
                                   pr_num,
                                   artifacts_dir=target_dir)
            except (RuntimeError, json.JSONDecodeError) as err:
                print(f"[PR #{pr_num}] Error: {err}", file=sys.stderr)
                all_green = False
                continue

            all_green = all_green and curr["is_green"]
            has_failure = has_failure or curr["has_failures"]
            if pr_num not in prev_states:
                print_initial_summary(pr_num, curr)
            else:
                print_diff(pr_num, prev_states[pr_num], curr)
            prev_states[pr_num] = curr

        if all_green:
            print("SUPER GREEN! Multipass authorized.")
            sys.exit(0)
        if not args.watch:
            sys.exit(1)
        if has_failure:
            print("ACTION REQUIRED: Triage failure immediately!")
            sys.exit(1)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
