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
from contextlib import contextmanager
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
query($owner: String!, $name: String!, $pr: Int!,
      $checksAfter: String, $threadsAfter: String,
      $withMeta: Boolean!, $withChecks: Boolean!, $withThreads: Boolean!) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $pr) {
      number state isDraft reviewDecision mergeable
      headRefName baseRefName headRefOid author { login }
      baseRef @include(if: $withMeta) {
        branchProtectionRule { requiredStatusCheckContexts }
      }
      statusCheckRollup @include(if: $withChecks) {
        contexts(first: 100, after: $checksAfter) {
          nodes {
            __typename
            ... on CheckRun {
              name status conclusion detailsUrl databaseId
              isRequired(pullRequestNumber: $pr)
              checkSuite { workflowRun { databaseId } }
            }
            ... on StatusContext {
              context state targetUrl isRequired(pullRequestNumber: $pr)
            }
          }
          pageInfo { hasNextPage endCursor }
        }
      }
      reviewThreads(first: 100, after: $threadsAfter) @include(if: $withThreads) {
        nodes {
          isResolved path line
          comments(last: 10) { nodes { databaseId author { login } body } }
        }
        pageInfo { hasNextPage endCursor }
      }
    }
  }
}"""

REQUIRED_SUCCESS_STATES = {"SUCCESS", "NEUTRAL"}
PENDING_STATES = {
    "PENDING",
    "EXPECTED",
    "IN_PROGRESS",
    "QUEUED",
    "WAITING",
    "REQUESTED",
}
_LEADING_BADGE_RE = re.compile(r"^(?:!\[[^\]]*\]\([^)]*\)\s*)+")
_MAX_WATCH_ERRORS = 3


def is_failed_state(state: str) -> bool:
    return state not in REQUIRED_SUCCESS_STATES and state not in PENDING_STATES


def gh_run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["gh", *args],
                          capture_output=True,
                          text=True,
                          check=False)


def gh_call(*args: str) -> str:
    res = gh_run(*args)
    if res.returncode != 0:
        cmd = " ".join(args)
        raise RuntimeError(res.stderr.strip() or f"gh {cmd} failed")
    return res.stdout.strip()


def resolve_repo(repo_arg: str | None) -> tuple[str, str]:
    raw = repo_arg or gh_call("repo", "view", "--json", "nameWithOwner", "-q",
                              ".nameWithOwner")
    owner, name = raw.split("/")
    return owner, name


def resolve_prs(pr_arg: str | None, owner: str, repo_name: str) -> list[int]:
    if pr_arg:
        nums = [int(x.strip()) for x in pr_arg.split(",") if x.strip()]
        if nums:
            return nums
    return [
        int(
            gh_call("pr", "view", "--repo", f"{owner}/{repo_name}", "--json",
                    "number", "-q", ".number"))
    ]


def _gql_pr(owner: str,
            name: str,
            pr_number: int,
            *,
            with_meta: bool,
            with_checks: bool,
            with_threads: bool,
            checks_after: str | None = None,
            threads_after: str | None = None) -> dict:
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
        "-F",
        f"withMeta={json.dumps(with_meta)}",
        "-F",
        f"withChecks={json.dumps(with_checks)}",
        "-F",
        f"withThreads={json.dumps(with_threads)}",
    ]
    if checks_after:
        args.extend(["-f", f"checksAfter={checks_after}"])
    if threads_after:
        args.extend(["-f", f"threadsAfter={threads_after}"])
    pr_data = json.loads(gh_call(*args))["data"]["repository"]["pullRequest"]
    if pr_data is None:
        raise RuntimeError(f"Pull request #{pr_number} not found")
    return pr_data


def fetch_pr_status(owner: str, name: str,
                    pr_number: int) -> tuple[dict, list[dict], list[dict]]:
    pr_data = _gql_pr(owner,
                      name,
                      pr_number,
                      with_meta=True,
                      with_checks=True,
                      with_threads=True)
    contexts: list[dict] = []
    rollup = pr_data.get("statusCheckRollup")
    if rollup:
        conn = rollup["contexts"]
        contexts.extend(conn["nodes"])
        while conn["pageInfo"]["hasNextPage"]:
            nxt = _gql_pr(owner,
                          name,
                          pr_number,
                          with_meta=False,
                          with_checks=True,
                          with_threads=False,
                          checks_after=conn["pageInfo"]["endCursor"])
            conn = nxt["statusCheckRollup"]["contexts"]
            contexts.extend(conn["nodes"])

    t_conn = pr_data.get("reviewThreads") or {}
    threads = list(t_conn.get("nodes") or [])
    while (t_conn.get("pageInfo") or {}).get("hasNextPage"):
        nxt = _gql_pr(owner,
                      name,
                      pr_number,
                      with_meta=False,
                      with_checks=False,
                      with_threads=True,
                      threads_after=t_conn["pageInfo"]["endCursor"])
        t_conn = nxt["reviewThreads"]
        threads.extend(t_conn["nodes"])
    return pr_data, contexts, threads


@contextmanager
def _staged_replace(target_dir: str):
    parent = os.path.dirname(os.path.abspath(target_dir))
    os.makedirs(parent, exist_ok=True)
    stage = tempfile.mkdtemp(prefix=".stage_", dir=parent)
    try:
        yield stage
        has_files = bool(os.listdir(stage))
        if os.path.exists(target_dir):
            shutil.rmtree(target_dir)
        if has_files:
            os.replace(stage, target_dir)
        else:
            shutil.rmtree(stage, ignore_errors=True)
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise


def parse_shepherd_report(filepath: str) -> dict:
    with open(filepath, "r", encoding="utf-8") as report_file:
        data = json.load(report_file)
    failing_jobs = [
        j for j, s in data["checks"].items() if s in ("failure", "cancelled")
    ]
    raw_tests = data.get("test_failures") or {}
    tests_by_target = raw_tests.get("failing_tests", raw_tests)
    test_failures = [{
        "name": item["name"],
        "message": (item.get("message") or "").strip(),
    } for items in tests_by_target.values() for item in items]
    return {
        "platform": data["platform"],
        "workflow": data.get("workflow", ""),
        "run_attempt": int(data.get("run_attempt") or 1),
        "failing_jobs": failing_jobs,
        "test_failures": test_failures,
        "has_failures": bool(failing_jobs or test_failures),
    }


def fetch_ci_shepherd_reports(owner: str,
                              name: str,
                              run_ids: list[str],
                              results_dir: str | None = None) -> list[dict]:
    if not run_ids:
        if results_dir and os.path.exists(results_dir):
            shutil.rmtree(results_dir)
        return []
    winners = {}
    with tempfile.TemporaryDirectory(prefix="shepherd_reports_") as tmpdir:
        for run_id in run_ids:
            gh_run("run", "download", run_id, "--repo",
                   f"{owner}/{name}", "-p", "shepherd-report-*", "-D",
                   os.path.join(tmpdir, run_id))
        for path in glob.glob(os.path.join(tmpdir, "**",
                                           "shepherd_report.json"),
                              recursive=True):
            part0 = os.path.relpath(path, tmpdir).split(os.sep, maxsplit=1)[0]
            report = parse_shepherd_report(path)
            key = (report["run_attempt"], int(part0) if part0.isdigit() else 0)
            prev = winners.get(report["platform"])
            if prev is None or key >= prev[0]:
                winners[report["platform"]] = (key, report,
                                               os.path.dirname(path))
        failing = [(r, d) for _, r, d in winners.values() if r["has_failures"]]
        if results_dir:
            with _staged_replace(results_dir) as stage:
                for _, rdir in failing:
                    shutil.copytree(rdir,
                                    os.path.join(stage,
                                                 os.path.basename(rdir)),
                                    dirs_exist_ok=True)
    return [r for r, _ in failing]


def download_failed_logs(owner: str, name: str, failed_checks: dict[str, dict],
                         logs_dir: str) -> None:
    repo = f"{owner}/{name}"
    seen: set[tuple[str, str]] = set()
    with _staged_replace(logs_dir) as stage:
        for check_name, info in failed_checks.items():
            job_id, run_id = info["job_id"], info["run_id"]
            if job_id:
                target, suffix = ("job", job_id), job_id
                gh_args = ("run", "view", "--job", job_id, "--log-failed",
                           "--repo", repo)
            elif run_id:
                target, suffix = ("run", run_id), f"run_{run_id}"
                gh_args = ("run", "view", run_id, "--log-failed", "--repo",
                           repo)
            else:
                continue
            if target in seen:
                continue
            seen.add(target)
            res = gh_run(*gh_args)
            if res.returncode == 0 and res.stdout:
                safe = re.sub(r"[^a-zA-Z0-9_.-]", "_",
                              check_name).strip("_") or "check"
                with open(os.path.join(stage, f"{safe}_{suffix}.log"),
                          "w",
                          encoding="utf-8") as log_file:
                    log_file.write(res.stdout)


def _collect_checks(pr: dict, contexts: list[dict]) -> dict[str, dict]:
    checks: dict[str, dict] = {}
    for node in contexts:
        if node["__typename"] == "CheckRun":
            cname = node["name"]
            state = (node["conclusion"] or "PENDING"
                     ) if node["status"] == "COMPLETED" else node["status"]
            url = node.get("detailsUrl") or ""
            db_id = int(node.get("databaseId") or 0)
            jm = re.search(r"/job/(\d+)", url)
            rm = re.search(r"/actions/runs/(\d+)", url)
            job_id = str(db_id) if db_id else (jm.group(1) if jm else "")
            wf_run = (node.get("checkSuite") or {}).get("workflowRun") or {}
            run_id = (str(wf_run["databaseId"]) if wf_run.get("databaseId")
                      else (rm.group(1) if rm else ""))
        else:
            cname, state = node["context"], node["state"]
            url, db_id, job_id, run_id = node.get("targetUrl") or "", 0, "", ""

        entry = {
            "state": state,
            "url": url,
            "job_id": job_id,
            "run_id": run_id,
            "db_id": db_id,
            "is_required": bool(node.get("isRequired")),
        }
        prev = checks.get(cname)
        if prev is None:
            checks[cname] = entry
        else:
            merged_req = prev["is_required"] or entry["is_required"]
            if entry["db_id"] >= prev["db_id"]:
                entry["is_required"] = merged_req
                checks[cname] = entry
            else:
                prev["is_required"] = merged_req

    rule = (pr.get("baseRef") or {}).get("branchProtectionRule") or {}
    for ctx in rule.get("requiredStatusCheckContexts") or []:
        if ctx in checks:
            checks[ctx]["is_required"] = True
        else:
            checks[ctx] = {
                "state": "EXPECTED",
                "url": "",
                "job_id": "",
                "run_id": "",
                "db_id": 0,
                "is_required": True,
            }
    return checks


def _collect_threads(threads: list[dict], pr_author: str | None) -> list[dict]:
    unresolved = []
    for thread in threads:
        if thread["isResolved"]:
            continue
        comments = (thread.get("comments") or {}).get("nodes") or []
        picked = next(
            (c for c in reversed(comments) if not pr_author or (
                c.get("author") or {}).get("login") != pr_author),
            comments[-1] if comments else {},
        )
        body = next(
            (s for line in (picked.get("body") or "").splitlines()
             if (s := _LEADING_BADGE_RE.sub("", line.strip()).strip())),
            "",
        )[:120]
        path, line = thread.get("path") or "", thread.get("line")
        unresolved.append({
            "comment_id":
            str(picked.get("databaseId") or ""),
            "location":
            f"{path}:{line}" if line else path,
            "author": (picked.get("author") or {}).get("login", "unknown"),
            "body":
            body,
        })
    return unresolved


def evaluate_pr(owner: str,
                name: str,
                pr_number: int,
                artifacts_dir: str | None = None,
                ignore_comment_ids: set[str] | None = None) -> dict:
    pr, contexts, threads = fetch_pr_status(owner, name, pr_number)
    checks = _collect_checks(pr, contexts)
    req = {k: v for k, v in checks.items() if v["is_required"]}
    gated = req or checks
    failed_checks = {
        k: v
        for k, v in gated.items() if is_failed_state(v["state"])
    }
    has_pending = any(v["state"] in PENDING_STATES for v in gated.values())

    failed_run_ids = sorted(
        {v["run_id"]
         for v in failed_checks.values() if v["run_id"]}, key=int)
    results_dir = (os.path.join(artifacts_dir, "test_results")
                   if artifacts_dir else None)
    ci_reports = fetch_ci_shepherd_reports(
        owner, name, failed_run_ids,
        results_dir=results_dir) if failed_checks else []

    has_failures = bool(failed_checks or ci_reports)
    downloaded_to = None
    if has_failures and artifacts_dir:
        download_failed_logs(owner, name, failed_checks,
                             os.path.join(artifacts_dir, "logs"))
        downloaded_to = artifacts_dir

    pr_author = (pr.get("author") or {}).get("login")
    all_threads = _collect_threads(threads, pr_author)
    ignored = ignore_comment_ids or set()
    unignored = [
        t for t in all_threads
        if not t["comment_id"] or t["comment_id"] not in ignored
    ]
    decision, mergeable = pr["reviewDecision"] or "NONE", pr["mergeable"]
    ci_green = (bool(gated) and not has_failures and not has_pending
                and mergeable == "MERGEABLE" and not pr["isDraft"]
                and pr["state"] == "OPEN")
    is_green = ci_green and not all_threads and decision != "CHANGES_REQUESTED"
    has_blocker = (has_failures or mergeable == "CONFLICTING" or pr["isDraft"]
                   or pr["state"] != "OPEN"
                   or (bool(unignored) and not ci_green))

    return {
        "pr": pr_number,
        "state": pr["state"],
        "is_draft": pr["isDraft"],
        "head_ref": pr["headRefName"] or "",
        "base_ref": pr.get("baseRefName") or "",
        "head_oid": pr["headRefOid"] or "",
        "decision": decision,
        "mergeable": mergeable,
        "is_green": is_green,
        "ci_green": ci_green,
        "has_pending": has_pending,
        "has_blocker": has_blocker,
        "failed_checks": failed_checks,
        "ci_reports": ci_reports,
        "all_threads": all_threads,
        "unignored_threads": unignored,
        "downloaded_to": downloaded_to,
    }


def print_pr_block(res: dict, include_ignored_threads: bool = False) -> None:
    pr_num, head, base, sha = (res["pr"], res["head_ref"], res["base_ref"],
                               res["head_oid"])
    st, draft, mrg, dec = (res["state"], res["is_draft"], res["mergeable"],
                           res["decision"])
    print(f"[PR #{pr_num}] {head} -> {base} (sha: {sha}) "
          f"state={st} draft={draft} mergeable={mrg} decision={dec}")
    for cname, info in res["failed_checks"].items():
        ids = " ".join(f"{k}={info[k]}" for k in ("run_id", "job_id", "url")
                       if info[k])
        suffix = f" {ids}" if ids else ""
        cstate = info["state"]
        print(f"  Failed check: {cname} [{cstate}]{suffix}")
    for rep in res["ci_reports"]:
        plat, wf = rep["platform"], rep["workflow"] or "unknown"
        parts = []
        if rep["failing_jobs"]:
            parts.append("failing_jobs=" + ", ".join(rep["failing_jobs"]))
        if rep["test_failures"]:
            tests = "; ".join(
                tf["name"] +
                (": " + tf["message"].splitlines()[0] if tf["message"] else "")
                for tf in rep["test_failures"][:10])
            num_tests = len(rep["test_failures"])
            parts.append(f"failing_tests({num_tests}): {tests}")
        summary = "; ".join(parts)
        print(f"  Report [{plat}] ({wf}): {summary}")
    threads = (res["all_threads"]
               if include_ignored_threads else res["unignored_threads"])
    for thread in threads:
        raw_cid = thread["comment_id"]
        cid = f" [comment_id={raw_cid}]" if raw_cid else ""
        loc, author, body = thread["location"], thread["author"], thread[
            "body"]
        print(f"  Unresolved thread{cid} {loc} by @{author}: {body}")
    if res["downloaded_to"]:
        root = res["downloaded_to"]
        tr_dir = os.path.join(root, "test_results")
        logs_dir = os.path.join(root, "logs")
        print(
            f"  Artifacts: {root} (test_results: {tr_dir}, logs: {logs_dir})")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="PR Shepherd: Minimal PR monitor.")
    parser.add_argument("--repo", help="Repository in owner/name format.")
    parser.add_argument("--pr", help="PR number(s), comma-separated.")
    parser.add_argument("--watch",
                        action="store_true",
                        help="Watch until green or actionable blocker.")
    parser.add_argument("--interval",
                        type=int,
                        default=30,
                        help="Poll interval in seconds.")
    parser.add_argument("--artifacts-dir",
                        "--download-dir",
                        dest="artifacts_dir",
                        default="artifacts",
                        help="Directory for failure logs and test results.")
    parser.add_argument("--no-download",
                        action="store_true",
                        help="Disable downloading failure artifacts and logs.")
    parser.add_argument("--ignore-comment-ids",
                        action="append",
                        default=[],
                        help="Comment databaseId(s) to ignore during --watch.")
    args = parser.parse_args()
    ignore_ids = {
        item.strip()
        for raw in args.ignore_comment_ids
        for item in raw.split(",") if item.strip()
    }

    try:
        owner, repo_name = resolve_repo(args.repo)
        pr_numbers = resolve_prs(args.pr, owner, repo_name)
    except (RuntimeError, ValueError, json.JSONDecodeError) as err:
        print(f"Error: {err}", file=sys.stderr)
        sys.exit(4)

    consecutive_errors = 0
    while True:
        results, poll_error = [], False
        for pr_num in pr_numbers:
            target_dir = None if args.no_download else (
                os.path.join(args.artifacts_dir, f"pr_{pr_num}")
                if len(pr_numbers) > 1 else args.artifacts_dir)
            try:
                results.append(
                    evaluate_pr(owner,
                                repo_name,
                                pr_num,
                                artifacts_dir=target_dir,
                                ignore_comment_ids=ignore_ids))
            except (RuntimeError, json.JSONDecodeError) as err:
                print(f"[PR #{pr_num}] Error: {err}", file=sys.stderr)
                poll_error = True

        if poll_error:
            consecutive_errors += 1
            if not args.watch or consecutive_errors >= _MAX_WATCH_ERRORS:
                sys.exit(4)
            time.sleep(args.interval)
            continue
        consecutive_errors = 0

        if all(r["is_green"] for r in results):
            print("SUPER GREEN! Multipass authorized.")
            sys.exit(0)
        if all(r["ci_green"] for r in results):
            for res in results:
                if not res["is_green"]:
                    print_pr_block(res, include_ignored_threads=True)
            print("CI GREEN. Awaiting human on review threads/decision "
                  "(agents: escalate only, never reply or resolve).")
            sys.exit(3)

        if (not args.watch or any(r["has_blocker"] for r in results)
                or not any(r["has_pending"] for r in results)):
            for res in results:
                if not res["is_green"]:
                    print_pr_block(res)
            sys.exit(1)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
