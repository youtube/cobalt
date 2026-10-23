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
"""PR Shepherd: Minimal GitHub PR monitor for CI checks and triage."""
import argparse, glob, json, os, re, shutil, subprocess, sys, tempfile
GQL = "query($o:String!,$n:String!,$p:Int!){repository(owner:$o,name:$n){pullRequest(number:$p){headRefOid reviewDecision mergeable state isDraft reviewThreads(first:100){nodes{isResolved path line}}}}}"
EXCLUDED = {"lint", "gemini commit message generator", "pr badges", "scorecards supply-chain security", "outside collaborator", "workflow debug", "auto label kokoro", "nightly_trigger", "label cherry pick"}
def log(m: str) -> None: print(f">> {m}")
def gh(*a, **k) -> str: return subprocess.run(["gh"] + list(a), capture_output=True, text=True, check=k.pop("check", True), **k).stdout.strip()
def jq(f: str, j: str, *x) -> str: return subprocess.run(["jq"] + list(x) + [f], input=j, capture_output=True, text=True, check=True).stdout.strip()
def resolve_repo(repo_arg=None):
  if repo_arg and "/" in repo_arg: return tuple(repo_arg.rstrip("/").removesuffix(".git").split("/"))
  return tuple(gh("repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner").split("/"))
def resolve_prs(pr_arg=None):
  return [int(p.strip()) for p in str(pr_arg).split(",") if p.strip()] if pr_arg else [int(gh("pr", "view", "--json", "number", "-q", ".number"))]
def parse_shepherd_report_file(filepath: str):
  try:
    with open(filepath, "r", encoding="utf-8") as f: content = f.read()
    return json.loads(jq('{platform:(.platform//.platform_name//"unknown"),workflow:(.workflow//""),run_url:(.run_url//""),run_attempt:((.run_attempt // 1) | tonumber),failing_jobs:[(.checks//{})|to_entries[]|select(.value=="failure" or .value=="cancelled")|.key],test_failures:[((.test_failures//{}).failing_tests//(.test_failures//{}))|to_entries[]?|.key as $t|(.value|if type=="array" then .[] else . end)|if type=="string" then {name:.,message:"",target:$t} else {name:(.name//"Unknown test"),message:((.message//"")|rtrimstr("\\n")),target:$t} end]}|.+{has_failures:((.failing_jobs|length>0) or (.test_failures|length>0))}', content))
  except Exception: return None
_parse_shepherd_report_file = parse_shepherd_report_file
def fetch_ci_shepherd_reports(owner: str, name: str, head_sha: str):
  if not head_sha: return [], None
  try: runs = json.loads(gh("run", "list", "--repo", f"{owner}/{name}", "--commit", head_sha, "--json", "databaseId,workflowName"))
  except Exception: return [], None
  tmpdir = tempfile.mkdtemp(prefix="shepherd_reports_")
  for r in runs:
    if (r.get("workflowName") or "").lower() not in EXCLUDED:
      gh("run", "download", str(r.get("databaseId")), "--repo", f"{owner}/{name}", "-p", "shepherd-report-*", "-D", os.path.join(tmpdir, str(r.get("databaseId"))), check=False)
  reports = [rep for rf in glob.glob(os.path.join(tmpdir, "**", "shepherd_report.json"), recursive=True) if (rep := parse_shepherd_report_file(rf))]
  return (json.loads(jq("group_by(.platform)|map(max_by(.run_attempt))", json.dumps(reports))), tmpdir) if reports else ([], tmpdir)
def download_failure_artifacts(owner: str, name: str, ci_reports, artifacts_dir: str, temp_dir=None):
  logs_dir, results_dir = os.path.join(artifacts_dir, "logs"), os.path.join(artifacts_dir, "test_results")
  os.makedirs(logs_dir, exist_ok=True); os.makedirs(results_dir, exist_ok=True)
  if temp_dir and os.path.exists(temp_dir):
    for item in os.listdir(temp_dir):
      s, d = os.path.join(temp_dir, item), os.path.join(results_dir, item)
      shutil.copytree(s, d, dirs_exist_ok=True) if os.path.isdir(s) else shutil.copy2(s, d)
  downloaded_logs = []
  for rep in ci_reports:
    if not rep.get("has_failures"): continue
    m = re.search(r"/actions/runs/(\d+)", rep.get("run_url", ""))
    run_id = m.group(1) if m else None
    for j in rep.get("failing_jobs", []):
      safe = re.sub(r"[^a-zA-Z0-9_.-]", "_", f"{rep['platform']}_{j}").strip("_")
      log_path = os.path.join(logs_dir, f"{safe}.log")
      txt = gh("run", "view", run_id, "--log-failed", "--repo", f"{owner}/{name}", check=False) if run_id else ""
      if not txt: txt = f"Failed Job: {j}\nPlatform: {rep['platform']}\nWorkflow: {rep['workflow']}\n"
      with open(log_path, "w", encoding="utf-8") as f: f.write(txt)
      downloaded_logs.append(log_path)
  return {"dir": artifacts_dir, "logs": sorted(downloaded_logs)}
def evaluate_pr(owner: str, name: str, pr_num: int, artifacts_dir="artifacts"):
  raw_pr = gh("api", "graphql", "-F", f"query={GQL}", "-F", f"o={owner}", "-F", f"n={name}", "-F", f"p={pr_num}")
  meta = json.loads(jq('.data.repository.pullRequest|{head_sha:(.headRefOid//""),decision:(.reviewDecision//"NONE"),mergeable:(.mergeable//"UNKNOWN"),is_draft:(.isDraft//false),state:(.state//"OPEN"),unresolved:[(.reviewThreads.nodes[]?|select(.isResolved==false)|"\\(.path):\\(.line)")]}', raw_pr))
  reports, tmp_dir = fetch_ci_shepherd_reports(owner, name, meta["head_sha"])
  failed = any(r.get("has_failures") for r in reports)
  downloaded = download_failure_artifacts(owner, name, reports, artifacts_dir, tmp_dir) if (failed and artifacts_dir) else None
  if tmp_dir and os.path.exists(tmp_dir): shutil.rmtree(tmp_dir, ignore_errors=True)
  is_green = bool(reports) and not failed and not meta["unresolved"] and meta["decision"] != "CHANGES_REQUESTED" and not meta["is_draft"] and meta["state"] == "OPEN" and meta["mergeable"] == "MERGEABLE"
  res = {"pr": pr_num, "head_sha": meta["head_sha"], "is_green": is_green, "has_failed_job": failed, "ci_shepherd_reports": reports, "downloaded_artifacts": downloaded, "unresolved_threads": meta["unresolved"], "decision": meta["decision"], "mergeable": meta["mergeable"]}
  if artifacts_dir:
    os.makedirs(artifacts_dir, exist_ok=True)
    with open(os.path.join(artifacts_dir, "summary.json"), "w", encoding="utf-8") as f: json.dump(res, f, indent=2)
  return res
def main():
  p = argparse.ArgumentParser(description="PR Shepherd: Minimal PR monitor.")
  p.add_argument("--repo"); p.add_argument("--pr")
  p.add_argument("--artifacts-dir", "--download-dir", dest="artifacts_dir", default="artifacts")
  p.add_argument("--no-download", action="store_true"); p.add_argument("--exit-zero", action="store_true")
  args = p.parse_args()
  owner, repo_name = resolve_repo(args.repo)
  prs, all_green = resolve_prs(args.pr), True
  for pr in prs:
    target_dir = None if args.no_download else (os.path.join(args.artifacts_dir, f"pr_{pr}") if len(prs) > 1 else args.artifacts_dir)
    curr = evaluate_pr(owner, repo_name, pr, artifacts_dir=target_dir)
    log(f"[PR #{pr}] Reports: {len(curr['ci_shepherd_reports'])}, Threads: {len(curr['unresolved_threads'])}, Mergeable: {curr['mergeable']}")
    for r in curr["ci_shepherd_reports"]:
      plat, wf = r.get("platform", "unknown"), r.get("workflow", "unknown")
      log(f"[PR #{pr}] CI Shepherd Report [{plat}] ({wf}): {'FAIL' if r.get('has_failures') else 'ALL PASS'}")
    if not curr["is_green"]: all_green = False
  if all_green: log("SUPER GREEN! Multipass authorized.")
  sys.exit(0 if all_green or args.exit_zero else 1)
if __name__ == "__main__":
  main()
