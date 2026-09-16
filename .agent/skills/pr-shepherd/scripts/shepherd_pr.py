#!/usr/bin/env python3
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
          pageInfo {{
            hasNextPage
            endCursor
          }}
        }}
      }}
      reviewThreads(first: 100) {{
        nodes {{
          isResolved path line
          comments(last: 1) {{
            nodes {{ databaseId author {{ login }} body url }}
          }}
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
          pageInfo {{
            hasNextPage
            endCursor
          }}
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


def parse_test_failures(test_failures: Any) -> List[Dict[str, str]]:
    """Extract a list of failed tests from the test_failures field."""
    results = []
    if not isinstance(test_failures, dict):
        return results

    raw_tests = test_failures.get("failing_tests", test_failures)
    if not isinstance(raw_tests, dict):
        return results

    for key, val in raw_tests.items():
        if isinstance(val, list):
            for item in val:
                if isinstance(item, dict):
                    name = item.get("name", "Unknown test")
                    msg = item.get("message", "").strip()
                    results.append({
                        "name": name,
                        "message": msg,
                        "target": key
                    })
                elif isinstance(item, str):
                    results.append({
                        "name": item,
                        "message": "",
                        "target": key
                    })
        elif isinstance(val, dict):
            name = val.get("name", key)
            msg = val.get("message", "").strip()
            results.append({"name": name, "message": msg, "target": key})
        elif isinstance(val, str):
            results.append({"name": key, "message": val.strip(), "target": ""})
    return results


def parse_shepherd_report(data: Dict[str, Any]) -> Dict[str, Any]:
    """Parse a shepherd_report.json dictionary into a normalized structure."""
    platform = data.get("platform") or data.get("platform_name") or "unknown"
    checks = data.get("checks", {})
    if not isinstance(checks, dict):
        checks = {}
    failing_jobs = [
        k for k, v in checks.items() if v in ("failure", "cancelled")
    ]
    test_failures = parse_test_failures(data.get("test_failures"))

    return {
        "platform": platform,
        "platform_name": data.get("platform_name", ""),
        "workflow": data.get("workflow", ""),
        "run_id": str(data.get("run_id", "")),
        "run_url": data.get("run_url", ""),
        "run_attempt": str(data.get("run_attempt", "")),
        "completed_at": data.get("completed_at", ""),
        "checks": checks,
        "failing_jobs": failing_jobs,
        "test_failures": test_failures,
        "has_failures": len(failing_jobs) > 0 or len(test_failures) > 0,
    }


def fetch_ci_shepherd_reports(owner: str, name: str,
                              head_sha: str) -> List[Dict[str, Any]]:
    """Fetch and parse shepherd_report.json artifacts for a commit."""
    if not head_sha:
        return []

    try:
        raw_runs = gh_call([
            "gh",
            "run",
            "list",
            "--repo",
            f"{owner}/{name}",
            "--commit",
            head_sha,
            "--json",
            "databaseId,workflowName,status,conclusion",
        ])
        runs = json.loads(raw_runs)
    except Exception:
        return []

    reports = []
    with tempfile.TemporaryDirectory(prefix="shepherd_reports_") as tmpdir:
        for r in runs:
            wf_name = (r.get("workflowName") or "").lower()
            if wf_name in EXCLUDED_CI_WORKFLOWS:
                continue
            run_id = str(r.get("databaseId"))
            dest_dir = os.path.join(tmpdir, run_id)
            try:
                res = subprocess.run(
                    [
                        "gh",
                        "run",
                        "download",
                        run_id,
                        "--repo",
                        f"{owner}/{name}",
                        "-p",
                        "shepherd-report-*",
                        "-D",
                        dest_dir,
                    ],
                    capture_output=True,
                    text=True,
                )
                if res.returncode != 0:
                    continue
            except Exception:
                continue

        report_files = glob.glob(os.path.join(tmpdir, "**",
                                              "shepherd_report.json"),
                                 recursive=True)
        for rf in sorted(report_files):
            try:
                with open(rf, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    parsed = parse_shepherd_report(data)
                    reports.append(parsed)
            except Exception:
                continue

    return reports


def format_ci_shepherd_report_summary(reports: List[Dict[str, Any]],
                                      only_failures: bool = True) -> List[str]:
    """Format CI Shepherd reports into concise summary lines."""
    lines = []
    for r in reports:
        has_fail = r.get("has_failures", False)
        if only_failures and not has_fail:
            continue

        plat = r.get("platform", "unknown")
        wf = r.get("workflow") or "unknown"
        run_url = r.get("run_url")
        url_str = f" - {run_url}" if run_url else ""

        if not has_fail:
            lines.append(
                f"CI Shepherd Report [{plat}] ({wf}){url_str}: ALL PASS")
            continue

        lines.append(f"CI Shepherd Report [{plat}] ({wf}){url_str}:")
        if r.get("failing_jobs"):
            lines.append(f"  Failing jobs: {', '.join(r['failing_jobs'])}")
        if r.get("test_failures"):
            lines.append(f"  Failing tests ({len(r['test_failures'])}):")
            for tf in r["test_failures"][:5]:
                name = tf.get("name", "test")
                msg = tf.get("message", "")
                first_line = msg.splitlines()[0][:80] if msg else ""
                lines.append(f"    - {name}: {first_line}"
                             if first_line else f"    - {name}")
            if len(r["test_failures"]) > 5:
                lines.append(
                    f"    ... and {len(r['test_failures']) - 5} more failing tests"
                )
    return lines


def gh_call(cmd: List[str]) -> str:
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError(res.stderr.strip()
                           or f"Command failed: {' '.join(cmd)}")
    return res.stdout.strip()


def resolve_repo(repo_arg: Optional[str]) -> Tuple[str, str]:
    if repo_arg:
        cleaned = repo_arg.rstrip("/")
        if cleaned.endswith(".git"):
            cleaned = cleaned[:-4]
        parts = cleaned.split("/")
        if len(parts) == 1:
            raw = gh_call([
                "gh", "repo", "view", "--json", "nameWithOwner", "-q",
                ".nameWithOwner"
            ])
            owner = raw.split("/")[0]
            return owner, parts[0]
        return parts[-2], parts[-1]
    raw = gh_call([
        "gh", "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"
    ])
    owner, repo = raw.split("/")
    return owner, repo


def resolve_prs(pr_arg: Optional[str]) -> List[int]:
    if pr_arg:
        return [int(p.strip()) for p in str(pr_arg).split(",") if p.strip()]
    raw = gh_call(["gh", "pr", "view", "--json", "number", "-q", ".number"])
    return [int(raw)]


def fetch_pr_status(owner: str, name: str, pr_number: int) -> Dict[str, Any]:
    query = GQL_QUERY.format(owner=owner, name=name, pr=pr_number)
    out = gh_call(["gh", "api", "graphql", "-f", f"query={query}"])
    pr = json.loads(out)["data"]["repository"]["pullRequest"]

    rollup = pr.get("statusCheckRollup")
    if not rollup:
        return pr

    contexts_conn = rollup.get("contexts")
    if not contexts_conn:
        return pr

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

        page_rollup = page_pr.get("statusCheckRollup")
        if not page_rollup:
            break
        page_contexts = page_rollup.get("contexts")
        if not page_contexts:
            break

        nodes.extend(page_contexts.get("nodes", []))
        page_info = page_contexts.get("pageInfo", {})

    contexts_conn["nodes"] = nodes
    return pr


def evaluate_pr(
    owner: str,
    name: str,
    pr_number: int,
    ignore_patterns: List[str] = None,
    fetch_ci_reports: bool = False,
) -> Dict[str, Any]:
    pr = fetch_pr_status(owner, name, pr_number)
    head_sha = pr.get("headRefOid") or ""

    contexts = (pr.get("statusCheckRollup") or {}).get("contexts",
                                                       {}).get("nodes", [])
    checks = {}
    for c in contexts:
        if c.get("__typename") == "CheckRun":
            c_name = c.get("name", "check")
            st = c.get("status")
            conc = c.get("conclusion")
            state = conc if st == "COMPLETED" else (st or "PENDING")
            url = c.get("detailsUrl") or ""
        else:
            c_name = c.get("context", "status")
            state = c.get("state", "PENDING")
            url = c.get("targetUrl") or ""
        checks[c_name] = {"state": state, "url": url}

    threads = (pr.get("reviewThreads") or {}).get("nodes", [])
    unresolved = []
    for t in threads:
        if not t.get("isResolved"):
            path = t.get("path", "")
            line = t.get("line") or ""
            loc = f"{path}:{line}" if line else path
            cmts = (t.get("comments") or {}).get("nodes", [])
            last = cmts[-1] if cmts else {}
            body = (last.get("body")
                    or "").splitlines()[0][:80] if last.get("body") else ""
            author = (last.get("author") or {}).get("login", "unknown")
            unresolved.append(f"{loc} by @{author}: {body}")

    decision = pr.get("reviewDecision") or "NONE"
    is_draft = pr.get("isDraft", False)
    state = pr.get("state", "OPEN")
    mergeable = pr.get("mergeable") or "UNKNOWN"
    merge_state_status = pr.get("mergeStateStatus") or "UNKNOWN"

    has_failed_checks = any(
        is_failed_state(v["state"]) for k, v in checks.items()
        if not any(p in k for p in (ignore_patterns or [])))

    ci_reports = []
    if (fetch_ci_reports or has_failed_checks) and head_sha:
        ci_reports = fetch_ci_shepherd_reports(owner, name, head_sha)

    is_green = (len(checks) > 0
                and all(v["state"] in SUCCESS_STATES
                        for k, v in checks.items()
                        if not any(p in k for p in (ignore_patterns or [])))
                and not any(r.get("has_failures")
                            for r in ci_reports) and len(unresolved) == 0
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
        "merge_state_status": merge_state_status,
    }


def print_initial_summary(pr_num: int,
                          curr: Dict[str, Any],
                          show_all_ci: bool = False):
    print(f"[PR #{pr_num}] Tracking started. "
          f"Checks: {len(curr['checks'])}, "
          f"Threads: {len(curr['unresolved_threads'])}, "
          f"Mergeable: {curr['mergeable']}")
    if curr["mergeable"] == "CONFLICTING":
        print(f"[PR #{pr_num}] WARNING: PR has merge conflicts! "
              f"(State: {curr['merge_state_status']})")
    for k, v in curr['checks'].items():
        if is_failed_state(v["state"]):
            url_str = f" (URL: {v['url']})" if v.get("url") else ""
            print(
                f"[PR #{pr_num}] Failed check '{k}' is {v['state']}{url_str}")
    if curr.get("ci_shepherd_reports"):
        ci_lines = format_ci_shepherd_report_summary(
            curr["ci_shepherd_reports"], only_failures=not show_all_ci)
        for line in ci_lines:
            print(f"[PR #{pr_num}] {line}")
    for t in curr['unresolved_threads']:
        print(f"[PR #{pr_num}] Open thread: {t}")


def print_diff(pr_num: int,
               prev: Dict[str, Any],
               curr: Dict[str, Any],
               show_all_ci: bool = False):
    changes = []

    prev_checks = prev.get("checks", {})
    curr_checks = curr.get("checks", {})
    for k, v in curr_checks.items():
        is_failure = is_failed_state(v["state"])
        if k not in prev_checks:
            msg = f"New check '{k}' is {v['state']}"
            if is_failure and v.get("url"):
                msg += f" (URL: {v['url']})"
            changes.append(msg)
        elif (prev_checks[k]["state"] != v["state"]
              or prev_checks[k]["url"] != v["url"]):
            msg = (f"Check '{k}' changed "
                   f"{prev_checks[k]['state']} -> {v['state']}")
            if is_failure and v.get("url"):
                msg += f" (URL: {v['url']})"
            changes.append(msg)

    # Compare CI shepherd reports
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
        if (not prev_r or prev_r.get("failing_jobs") != r.get("failing_jobs")
                or len(prev_r.get("test_failures", [])) != len(
                    r.get("test_failures", []))):
            if r.get("has_failures"):
                lines = format_ci_shepherd_report_summary([r],
                                                          only_failures=True)
                changes.extend(lines)
            elif prev_r and prev_r.get(
                    "has_failures") and not r.get("has_failures"):
                changes.append(
                    f"CI Shepherd Report [{plat}]: Failures resolved (ALL PASS)"
                )

    prev_threads = set(prev.get("unresolved_threads", []))
    curr_threads = set(curr.get("unresolved_threads", []))
    new_threads = curr_threads - prev_threads
    resolved_threads = prev_threads - curr_threads

    for t in new_threads:
        changes.append(f"New thread: {t}")
    for t in resolved_threads:
        changes.append(f"Resolved thread: {t}")

    if prev.get("decision") != curr.get("decision"):
        changes.append(f"Review decision: {prev.get('decision')} -> "
                       f"{curr.get('decision')}")

    if (prev.get("mergeable") != curr.get("mergeable") or
            prev.get("merge_state_status") != curr.get("merge_state_status")):
        msg = (
            f"Mergeable status changed: "
            f"{prev.get('mergeable')} ({prev.get('merge_state_status')}) -> "
            f"{curr.get('mergeable')} ({curr.get('merge_state_status')})")
        if curr.get("mergeable") == "CONFLICTING":
            msg += " - WARNING: Merge conflicts detected!"
        changes.append(msg)

    for c in changes:
        print(f"[PR #{pr_num}] {c}")


def main():
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
    parser.add_argument("--run-once",
                        "--check",
                        action="store_true",
                        dest="run_once",
                        help="Run once, output JSON status, and exit 0.")
    parser.add_argument(
        "--ignore-checks",
        help="Comma-separated list of substrings to ignore in check names.")
    parser.add_argument("--ci-report",
                        action="store_true",
                        help="Fetch and display CI Shepherd JSON reports.")
    args = parser.parse_args()
    ignore_patterns = [p.strip() for p in args.ignore_checks.split(",")
                       ] if args.ignore_checks else []

    try:
        owner, repo_name = resolve_repo(args.repo)
        pr_numbers = resolve_prs(args.pr)
    except Exception as err:
        sys.exit(f"Error initializing: {err}")

    prev_states = {}

    try:
        while True:
            all_green = True
            results = []
            for p in pr_numbers:
                try:
                    curr = evaluate_pr(owner,
                                       repo_name,
                                       p,
                                       ignore_patterns,
                                       fetch_ci_reports=args.ci_report)
                    if not curr["is_green"]:
                        all_green = False

                    if args.run_once:
                        results.append(curr)
                    else:
                        if p not in prev_states:
                            print_initial_summary(p,
                                                  curr,
                                                  show_all_ci=args.ci_report)
                        else:
                            print_diff(p,
                                       prev_states[p],
                                       curr,
                                       show_all_ci=args.ci_report)

                    prev_states[p] = curr
                except Exception as err:
                    if args.run_once:
                        results.append({
                            "pr": p,
                            "error": str(err),
                            "is_green": False
                        })
                    else:
                        print(f"[PR #{p}] Error: {err}", file=sys.stderr)
                    all_green = False

            if args.run_once:
                print(json.dumps(results, indent=2))
                sys.exit(0)

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
