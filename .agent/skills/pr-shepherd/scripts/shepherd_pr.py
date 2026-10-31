#!/usr/bin/env python3
# Copyright 2026 Google LLC
# TAG=agy
"""PR Shepherd: Minimal GitHub PR monitor for CI checks and review comments."""

# pylint: disable=bad-indentation,inconsistent-quotes,broad-exception-caught,subprocess-run-check,unused-argument,line-too-long

import argparse
import glob
import json
import os
import subprocess
import sys
import tempfile
import time
from typing import Any, Dict, List, Optional, Tuple

GQL_QUERY = """{{
  repository(owner: "{owner}", name: "{name}") {{
    pullRequest(number: {pr}) {{
      number title state isDraft reviewDecision url mergeable mergeStateStatus headRefName headRefOid
      statusCheckRollup {{
        contexts(first: 100) {{
          nodes {{
            __typename
            ... on CheckRun {{ name status conclusion detailsUrl }}
            ... on StatusContext {{ context state targetUrl }}
          }}
          pageInfo {{ hasNextPage endCursor }}
        }}
      }}
      reviewThreads(first: 100) {{
        nodes {{
          isResolved path line
          comments(last: 1) {{ nodes {{ databaseId author {{ login }} body url }} }}
        }}
      }}
    }}
  }}
}}"""

GQL_QUERY_CONTEXTS_PAGE = """{{
  repository(owner: "{owner}", name: "{name}") {{
    pullRequest(number: {pr}) {{
      statusCheckRollup {{
        contexts(first: 100, after: "{after}") {{
          nodes {{
            __typename
            ... on CheckRun {{ name status conclusion detailsUrl }}
            ... on StatusContext {{ context state targetUrl }}
          }}
          pageInfo {{ hasNextPage endCursor }}
        }}
      }}
    }}
  }}
}}"""

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


def gh_call(cmd: List[str]) -> str:
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
        raise RuntimeError(res.stderr.strip()
                           or f"Command failed: {' '.join(cmd)}")
    return res.stdout.strip()


def resolve_repo(repo_arg: Optional[str]) -> Tuple[str, str]:
    if repo_arg:
        cleaned = repo_arg.rstrip("/").removesuffix(".git")
        parts = cleaned.split("/")
        if len(parts) == 2:
            return parts[0], parts[1]
    raw = gh_call([
        "gh", "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"
    ])
    owner, repo = raw.split("/")
    return owner, repo_arg.split("/")[-1] if repo_arg else repo


def resolve_prs(pr_arg: Optional[str]) -> List[int]:
    if pr_arg:
        return [int(p.strip()) for p in str(pr_arg).split(",") if p.strip()]
    return [
        int(gh_call(["gh", "pr", "view", "--json", "number", "-q", ".number"]))
    ]


def _parse_test_failures(raw_tests: Any) -> List[Dict[str, Any]]:
    """Parse and normalize raw test failure payload structures into standardized failure dictionaries."""
    if isinstance(raw_tests, dict):
        raw_tests = raw_tests.get("failing_tests", raw_tests)
    if not isinstance(raw_tests, dict):
        return []

    test_failures = []
    for target, val in raw_tests.items():
        items = val if isinstance(val, list) else [val]
        for item in items:
            if isinstance(item, dict):
                test_failures.append({
                    "name":
                    item.get("name", "Unknown test"),
                    "message": (item.get("message") or "").strip(),
                    "target":
                    target,
                })
            elif isinstance(item, str):
                test_failures.append({
                    "name": item,
                    "message": "",
                    "target": target,
                })
    return test_failures


def _parse_shepherd_report_file(filepath: str) -> Optional[Dict[str, Any]]:
    """Parse a single shepherd_report.json file into a standardized report structure."""
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, dict):
            return None

        platform = data.get("platform") or data.get(
            "platform_name") or "unknown"
        checks = data.get("checks") if isinstance(data.get("checks"),
                                                  dict) else {}
        failing_jobs = [
            k for k, v in checks.items() if v in ("failure", "cancelled")
        ]
        test_failures = _parse_test_failures(data.get("test_failures", {}))

        return {
            "platform": platform,
            "workflow": data.get("workflow", ""),
            "run_url": data.get("run_url", ""),
            "failing_jobs": failing_jobs,
            "test_failures": test_failures,
            "has_failures": bool(failing_jobs or test_failures),
        }
    except Exception:
        return None


def _download_run_artifacts(owner: str, name: str, run_id: str,
                            dest_dir: str) -> None:
    """Download shepherd-report-* artifacts for a specific workflow run."""
    try:
        subprocess.run([
            "gh", "run", "download", run_id, "--repo", f"{owner}/{name}", "-p",
            "shepherd-report-*", "-D", dest_dir
        ],
                       capture_output=True,
                       text=True,
                       check=False)
    except Exception:
        pass


def fetch_ci_shepherd_reports(owner: str, name: str,
                              head_sha: str) -> List[Dict[str, Any]]:
    """Fetch and parse shepherd_report.json artifacts for a commit in a single pass."""
    if not head_sha:
        return []
    try:
        raw_runs = gh_call([
            "gh", "run", "list", "--repo", f"{owner}/{name}", "--commit",
            head_sha, "--json", "databaseId,workflowName"
        ])
        runs = json.loads(raw_runs)
        if not isinstance(runs, list):
            return []
    except Exception:
        return []

    reports = []
    with tempfile.TemporaryDirectory(prefix="shepherd_reports_") as tmpdir:
        for r in runs:
            if not isinstance(r, dict):
                continue
            wf_name = (r.get("workflowName") or "").lower()
            if wf_name in EXCLUDED_CI_WORKFLOWS:
                continue
            run_id = str(r.get("databaseId"))
            _download_run_artifacts(owner, name, run_id,
                                    os.path.join(tmpdir, run_id))

        for rf in sorted(
                glob.glob(os.path.join(tmpdir, "**", "shepherd_report.json"),
                          recursive=True)):
            report = _parse_shepherd_report_file(rf)
            if report:
                reports.append(report)

    return reports


def format_ci_shepherd_report_summary(reports: List[Dict[str, Any]],
                                      only_failures: bool = True) -> List[str]:
    """Format CI Shepherd reports into concise summary lines."""
    lines = []
    for r in reports:
        has_fail = r.get("has_failures", False)
        if only_failures and not has_fail:
            continue

        plat, wf, run_url = r.get("platform", "unknown"), r.get(
            "workflow", "unknown"), r.get("run_url", "")
        url_str = f" - {run_url}" if run_url else ""

        if not has_fail:
            lines.append(
                f"CI Shepherd Report [{plat}] ({wf}){url_str}: ALL PASS")
        else:
            lines.append(f"CI Shepherd Report [{plat}] ({wf}){url_str}:")
            if r.get("failing_jobs"):
                lines.append(f"  Failing jobs: {', '.join(r['failing_jobs'])}")
            if r.get("test_failures"):
                lines.append(f"  Failing tests ({len(r['test_failures'])}):")
                for tf in r["test_failures"][:5]:
                    msg = (tf.get("message") or "").splitlines()
                    lines.append(f"    - {tf['name']}: {msg[0][:80]}"
                                 if msg and msg[0] else f"    - {tf['name']}")
                if len(r["test_failures"]) > 5:
                    lines.append(
                        f"    ... and {len(r['test_failures']) - 5} more failing tests"
                    )
    return lines


def fetch_pr_status(owner: str, name: str, pr_number: int) -> Dict[str, Any]:
    """Fetch PR details and handle paginated GraphQL statusCheckRollup contexts."""
    query = GQL_QUERY.format(owner=owner, name=name, pr=pr_number)
    out = gh_call(["gh", "api", "graphql", "-f", f"query={query}"])
    pr = json.loads(out)["data"]["repository"]["pullRequest"]

    rollup = pr.get("statusCheckRollup") or {}
    contexts_conn = rollup.get("contexts") or {}
    nodes = contexts_conn.get("nodes", [])
    page_info = contexts_conn.get("pageInfo", {})

    while page_info.get("hasNextPage"):
        after = page_info.get("endCursor")
        page_query = GQL_QUERY_CONTEXTS_PAGE.format(owner=owner,
                                                    name=name,
                                                    pr=pr_number,
                                                    after=after)
        page_out = gh_call(
            ["gh", "api", "graphql", "-f", f"query={page_query}"])
        page_pr = json.loads(page_out)["data"]["repository"]["pullRequest"]
        page_contexts = (page_pr.get("statusCheckRollup")
                         or {}).get("contexts") or {}
        nodes.extend(page_contexts.get("nodes", []))
        page_info = page_contexts.get("pageInfo", {})

    if rollup and contexts_conn:
        contexts_conn["nodes"] = nodes
    return pr


def evaluate_pr(
    owner: str,
    name: str,
    pr_number: int,
    fetch_ci_reports: bool = True,
) -> Dict[str, Any]:
    """Evaluate a PR's state, checks, review threads, and CI shepherd report failures."""
    pr = fetch_pr_status(owner, name, pr_number)
    head_sha = pr.get("headRefOid") or ""
    contexts = (pr.get("statusCheckRollup") or {}).get("contexts",
                                                       {}).get("nodes", [])

    checks = {}
    for c in contexts:
        if c.get("__typename") == "CheckRun":
            name_key = c.get("name", "check")
            st, conc = c.get("status"), c.get("conclusion")
            state = conc if st == "COMPLETED" else (st or "PENDING")
            url = c.get("detailsUrl") or ""
        else:
            name_key = c.get("context", "status")
            state = c.get("state", "PENDING")
            url = c.get("targetUrl") or ""
        checks[name_key] = {"state": state, "url": url}

    unresolved = []
    for t in (pr.get("reviewThreads") or {}).get("nodes", []):
        if not t.get("isResolved"):
            path = t.get("path", "")
            line = t.get("line")
            loc = f"{path}:{line}" if line else path

            comments = (t.get("comments") or {}).get("nodes") or []
            last_comment = comments[-1] if comments else {}
            author = (last_comment.get("author") or {}).get("login", "unknown")
            raw_body = last_comment.get("body") or ""
            body = raw_body.splitlines()[0][:80] if raw_body else ""

            unresolved.append(f"{loc} by @{author}: {body}")

    decision = pr.get("reviewDecision") or "NONE"
    is_draft = pr.get("isDraft", False)
    state = pr.get("state", "OPEN")
    mergeable = pr.get("mergeable") or "UNKNOWN"
    merge_status = pr.get("mergeStateStatus") or "UNKNOWN"

    has_failed_checks = any(
        is_failed_state(v["state"]) for v in checks.values())

    ci_reports = []
    if (fetch_ci_reports or has_failed_checks) and head_sha:
        ci_reports = fetch_ci_shepherd_reports(owner, name, head_sha)

    is_green = (bool(checks) and all(v["state"] in SUCCESS_STATES
                                     for v in checks.values())
                and not any(r.get("has_failures")
                            for r in ci_reports) and not unresolved
                and decision != "CHANGES_REQUESTED" and not is_draft
                and state == "OPEN" and mergeable == "MERGEABLE")

    return {
        "pr": pr_number,
        "head_sha": head_sha,
        "is_green": is_green,
        "checks": checks,
        "ci_shepherd_reports": ci_reports,
        "unresolved_threads": unresolved,
        "decision": decision,
        "is_draft": is_draft,
        "state": state,
        "mergeable": mergeable,
        "merge_state_status": merge_status,
    }


def print_initial_summary(pr_num: int, curr: Dict[str, Any]):
    """Print initial PR tracking status."""
    print(f"[PR #{pr_num}] Tracking started. Checks: {len(curr['checks'])},"
          f" Threads: {len(curr['unresolved_threads'])}, Mergeable:"
          f" {curr['mergeable']}")
    if curr["mergeable"] == "CONFLICTING":
        print(f"[PR #{pr_num}] WARNING: PR has merge conflicts! (State:"
              f" {curr['merge_state_status']})")
    for k, v in curr["checks"].items():
        if is_failed_state(v["state"]):
            url_str = f" (URL: {v['url']})" if v.get("url") else ""
            print(
                f"[PR #{pr_num}] Failed check '{k}' is {v['state']}{url_str}")
    if curr.get("ci_shepherd_reports"):
        for line in format_ci_shepherd_report_summary(
                curr["ci_shepherd_reports"], only_failures=True):
            print(f"[PR #{pr_num}] {line}")
    for t in curr["unresolved_threads"]:
        print(f"[PR #{pr_num}] Open thread: {t}")


def print_diff(pr_num: int, prev: Dict[str, Any], curr: Dict[str, Any]):
    """Print PR state changes between polling iterations."""
    changes = []
    prev_checks, curr_checks = prev.get("checks", {}), curr.get("checks", {})
    for k, v in curr_checks.items():
        is_fail = is_failed_state(v["state"])
        url_str = f" (URL: {v['url']})" if is_fail and v.get("url") else ""
        if k not in prev_checks:
            changes.append(f"New check '{k}' is {v['state']}{url_str}")
        elif prev_checks[k]["state"] != v["state"] or prev_checks[k][
                "url"] != v["url"]:
            changes.append(
                f"Check '{k}' changed {prev_checks[k]['state']} -> {v['state']}{url_str}"
            )

    prev_reports = {
        r.get("platform"): r
        for r in prev.get("ci_shepherd_reports", [])
    }
    curr_reports = {
        r.get("platform"): r
        for r in curr.get("ci_shepherd_reports", [])
    }
    for plat, r in curr_reports.items():
        prev_r = prev_reports.get(plat)
        if not prev_r or prev_r.get("failing_jobs") != r.get(
                "failing_jobs") or len(prev_r.get("test_failures", [])) != len(
                    r.get("test_failures", [])):
            if r.get("has_failures"):
                changes.extend(
                    format_ci_shepherd_report_summary([r], only_failures=True))
            elif prev_r and prev_r.get(
                    "has_failures") and not r.get("has_failures"):
                changes.append(
                    f"CI Shepherd Report [{plat}]: Failures resolved (ALL PASS)"
                )

    new_threads = set(curr.get("unresolved_threads", [])) - set(
        prev.get("unresolved_threads", []))
    resolved_threads = set(prev.get("unresolved_threads", [])) - set(
        curr.get("unresolved_threads", []))
    for t in new_threads:
        changes.append(f"New thread: {t}")
    for t in resolved_threads:
        changes.append(f"Resolved thread: {t}")

    if prev.get("decision") != curr.get("decision"):
        changes.append(
            f"Review decision: {prev.get('decision')} -> {curr.get('decision')}"
        )

    if prev.get("mergeable") != curr.get("mergeable") or prev.get(
            "merge_state_status") != curr.get("merge_state_status"):
        msg = (
            f"Mergeable status changed: {prev.get('mergeable')}"
            f" ({prev.get('merge_state_status')}) -> {curr.get('mergeable')}"
            f" ({curr.get('merge_state_status')})")
        if curr.get("mergeable") == "CONFLICTING":
            msg += " - WARNING: Merge conflicts detected!"
        changes.append(msg)

    for c in changes:
        print(f"[PR #{pr_num}] {c}")


def main():
    """CLI entry point for PR Shepherd."""
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
    args = parser.parse_args()

    try:
        owner, repo_name = resolve_repo(args.repo)
        pr_numbers = resolve_prs(args.pr)
    except Exception as err:
        sys.exit(f"Error initializing: {err}")

    prev_states = {}
    try:
        while True:
            all_green = True
            for p in pr_numbers:
                try:
                    fetch_ci = not args.watch or p not in prev_states
                    curr = evaluate_pr(owner,
                                       repo_name,
                                       p,
                                       fetch_ci_reports=fetch_ci)
                    if not curr["is_green"]:
                        all_green = False

                    if p not in prev_states:
                        print_initial_summary(p, curr)
                    else:
                        print_diff(p, prev_states[p], curr)
                    prev_states[p] = curr
                except Exception as err:
                    print(f"[PR #{p}] Error: {err}", file=sys.stderr)
                    all_green = False

            if all_green:
                print("SUPER GREEN! Multipass authorized.")
                sys.exit(0)

            if not args.watch:
                sys.exit(1)

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("Stopped by user.")
        sys.exit(130)


if __name__ == "__main__":
    main()
