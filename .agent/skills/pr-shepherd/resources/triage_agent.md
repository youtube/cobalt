# PR Shepherd Watch & Triage Guide

This guide is for the **Watch Coordinator** (`pr-shepherd-watcher`) and
**Issue Worker** (`pr-shepherd-fixer`) subagents. It defines the watch loop,
active interruption dispatch, fast-path log triage, CI Shepherd JSON report
analysis, the Flake / Infra / Base Drift protocol, merge conflict and review
comment handling, and the synchronized **Action Ledger**
(`shepherding_report.md`).

> [!IMPORTANT]
> **SCOPE LIMITATION & TEST FILTERING PROHIBITION**:
> Fixes by the PR Shepherd are **strictly limited to issues caused by the PR**.
> - **PR-Caused Failures**: Build errors, compile failures, broken tests,
>   linter issues, merge conflicts, and review comments caused by the PR diff
>   **must be fixed**.
> - **No Autonomous Test Filtering**: You must **NEVER** autonomously add
>   failing or flaky tests to test filter manifests (e.g., `*_filter.json`,
>   `filter.txt`), disable tests, or add test skip annotations, and **never in
>   a PR with an unrelated feature**.
> - **Flaky Tests & Infrastructure Issues**: CI runner issues, network errors,
>   timeouts, infrastructure outages, or unrelated flaky tests **must NOT be
>   fixed** via code or CI config workarounds. Follow the Flake / Infra / Base
>   Drift protocol below.

---

## 🛑 MANDATORY COMPLETION GATE: GREEN STATUS

> [!IMPORTANT]
> **STRICT COMPLETION INVARIANT**:
> Shepherding **CANNOT FINISH** until all monitored PRs achieve **All Green**
> status (or a verified non-PR base/infra failure is escalated to the user).

To satisfy the completion invariant, **ALL** of the following conditions must
be met:
1. **100% of CI Checks Completed & Green**:
   `green_checks == total_checks > 0`, with 0 failed and 0 running/pending.
2. **Zero Unaddressed Comments**: All top-level PR comments and inline
   file/diff review threads are addressed with code fixes (and replied to
   in-thread/resolved when explicitly instructed by the user).
3. **No Pending Changes Requested**: All review decisions are `APPROVED` or have
   no outstanding `CHANGES_REQUESTED`, and mergeability is `MERGEABLE`.
4. **Verified Script Output**: `shepherd_pr.py` outputs:
   ```text
   SUPER GREEN! Multipass authorized.
   ```

---

## ⚡ IMMEDIATE ACTION ON FAILURE (ACTIVE INTERRUPTION)

> [!IMPORTANT]
> **DO NOT WAIT FOR MATRIX RUNS TO COMPLETE WHEN FAILURES OCCUR**:
> The moment **ANY** check, job, or step fails, or an unresolved comment or
> merge conflict appears, **GET TO WORK IMMEDIATELY**:
> - **Active Interruption (Zero Passive Waiting)**: If 1 job fails while 15
>   other matrix jobs are still running or pending, **DO NOT WAIT** for the
>   remaining jobs or overall CI run to reach a terminal state. Interrupt
>   monitoring immediately and dispatch/run triage on the failed job.
> - **Immediate Step-Log Diagnosis (Fast Path)**: For early setup,
>   configuration, workflow syntax, `depot_tools`, action schema evaluation, or
>   early build step failures, `shepherd_report.json` will not exist. **DO NOT
>   WAIT** for `shepherd_report.json`! Immediately inspect the downloaded step
>   logs or fetch them via `gh run view --job <JOB_ID> --log-failed` and
>   diagnose the error directly.
> - **Rapid Fix Cycle**: Once diagnosed as PR-caused, commit and push the fix
>   immediately. Pushing a new revision cancels obsolete pending jobs and
>   restarts CI on the fixed commit.

---

## 🚫 STRICT PROHIBITIONS (PUBLIC-REPO SAFE POLICIES)

### 1. DO NOT MERGE PULL REQUESTS

> [!CAUTION]
> **NEVER MERGE PRS**:
> You must **NEVER merge pull requests** or trigger PRs to be merged under any
> circumstances (e.g., via `gh pr merge`, GitHub GraphQL/REST API merge
> endpoints, or enabling auto-merge).
> - **Completion Boundary**: Your responsibility ends at **SUPER GREEN** status
>   (or structured human escalation).
> - **User Delegation Only**: PR merging is strictly reserved for the user and
>   requires explicit, unambiguous user instructions in chat.

### 2. PREVENT AUTOMATIC COMMENTING ON GITHUB

> [!CAUTION]
> **NO AUTOMATIC COMMENTS OR REPLIES**:
> You must **NEVER post comments or replies** on GitHub pull requests or issues
> autonomously without explicit user instruction in chat.
> - **No Autonomous Commenting**: Never post PR comments, inline review
>   replies, or issue comments autonomously.
> - **Single-Use Consent**: Approval granted by the user to post a comment or
>   reply is **strictly single-use and non-persistent**; it expires immediately
>   after being used once. Mandatory re-approval is required for subsequent
>   comments or replies.
> - **In-Thread Replies When Instructed**: Only when explicit single-use
>   consent has been granted for a specific reply, reply directly to the
>   inline review comment thread via:
>   `gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/<COMMENT_ID>/replies`

---

## 1. Watch Coordinator Loop (`pr-shepherd-watcher`)

The Watch Coordinator owns the monitoring loop and delegates every failure,
unresolved review comment, or merge conflict to an Issue Worker
(`pr-shepherd-fixer`) running in an isolated workspace (`Workspace: "branch"`
or WISP worktree).

### Running `shepherd_pr.py`

```bash
# Watch a PR (downloads failure test_results/ and logs/ to artifacts/ by default)
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --watch

# Watch stacked PRs with custom interval and artifacts directory
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr 101,102,103 --watch --interval 45 \
  --artifacts-dir /tmp/shepherd_artifacts

# One-shot check without downloading artifacts/logs
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --no-download
```

### CLI Flags & Artifact Layout
- `--repo <owner>/<name>`: Target repository (auto-detected if omitted).
- `--pr <num[,num...]>`: Single PR or comma-separated PR list.
- `--watch`: Poll continuously every `--interval` seconds until green or failure.
- `--interval <sec>`: Poll interval in seconds (default: `30`).
- `--artifacts-dir` / `--download-dir <path>`: Where failure artifacts are
  saved (default: `artifacts`). Single PR uses `<artifacts_dir>/test_results/`
  and `<artifacts_dir>/logs/`; multiple PRs use
  `<artifacts_dir>/pr_<num>/test_results/` and `<artifacts_dir>/pr_<num>/logs/`.
- `--no-download`: Skip downloading `shepherd_report.json` and failed step logs.

### Watch Dispatch & Context-Limit Handoff Protocol
1. **Initialize Ledger**: Create `shepherding_report.md` at the workspace root
   immediately (using `shepherding_report.lock` + atomic replacement).
2. **Run `--watch`**: Start `shepherd_pr.py --watch` (in background or active
   monitoring).
3. **Active Interruption Dispatch**:
   - When `shepherd_pr.py --watch` detects a failure, it downloads failure
     artifacts/logs, prints `ACTION REQUIRED: Triage failure immediately!`, and
     exits `1`. Immediately spawn an Issue Worker (`Role: "pr-shepherd-fixer"`)
     in an isolated workspace (`Workspace: "branch"` or WISP).
   - When `shepherd_pr.py --watch` emits `Open thread:`, `New thread:`, or
     `WARNING: ... merge conflicts` while other checks are still running,
     **do not wait** for the matrix to finish—immediately spawn an Issue Worker
     to handle the thread or conflict.
   - Instruct every Issue Worker to read this guide (`resources/triage_agent.md`)
     first, pass the exact artifact directory / PR / check / thread context,
     and state the single-use comment reply restriction.
4. **Loop Until Done**: After the Issue Worker updates the Action Ledger and
   reports completion, update `shepherding_report.md` if needed and restart
   `shepherd_pr.py --watch` until exit `0` (`SUPER GREEN! Multipass authorized.`)
   or a mandatory human escalation occurs.
5. **Clean Context Handoff**: If approaching context limits during a prolonged
   watch, flush all latest state into `shepherding_report.md` (with lock +
   atomic replacement) so a replacement watcher can resume seamlessly without
   re-investigating past iterations.

---

## 2. GitHub CLI (`gh`) Commands for Triage & Log Retrieval

```bash
# 1. Inspect PR status & CI Shepherd report findings directly:
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER>

# 2. Download CI Shepherd JSON report artifacts (~500 bytes per target):
gh run download <RUN_ID> -p shepherd-report-* -D /tmp/shepherd_reports \
  --repo <owner>/<repo>

# 3. Fetch failed step logs only (fast & token-efficient for builds):
gh run view --job <JOB_ID> --log-failed --repo <owner>/<repo>

# 4. Fetch full job log (only if step log is incomplete):
gh run view --job <JOB_ID> --log --repo <owner>/<repo>

# 5. View overall workflow run status and job list:
gh run view <RUN_ID> --repo <owner>/<repo>

# 6. Rerun failed jobs in a workflow run (for transient infra flakes):
gh run rerun <RUN_ID> --failed --repo <owner>/<repo>

# 7. Reply to a review comment in-thread (ONLY with explicit single-use consent):
gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/<COMMENT_ID>/replies \
  --field body="Done. <brief explanation>"
```

---

## 3. Efficient Log Triage & Root-Cause Extraction

> [!IMPORTANT]
> **PR Context & Investigating Root Cause**
> 1. **Understand the PR**: Inspect the PR description and diff
>    (`gh pr view <PR_NUMBER> --repo <owner>/<repo>`, `gh pr diff <PR_NUMBER>`).
> 2. **Check Pre-Downloaded Artifacts First**: Look inside `<artifacts_dir>/test_results/`
>    and `<artifacts_dir>/logs/` populated by `shepherd_pr.py` before making
>    redundant network calls.
> 3. **Determine if PR Caused Failure**: Check whether the failure touches
>    modified files, changed symbols, or affected subsystems.
> 4. **Distinguish PR Failures vs. Flakes / Infra / Base Drift**:
>    - **PR-Caused**: Compiler errors on modified code, unit/integration test
>      failures for modified functionality, linter errors, or merge conflicts.
>    - **Flake / Infra / Base-Drift**: Network/HTTP timeouts, runner disk
>      exhaustion, machine disconnects, external service outages, or
>      pre-existing/upstream failures on the base branch.

### ⚡ Fast-Path: Immediate Triage for Early Step & Workflow Failures

When a job fails early (e.g., workflow syntax errors, missing inputs,
`depot_tools`/environment setup failures, action schema evaluation, or early
build failures before test report artifacts are generated), `shepherd_report.json`
will **not** exist:

1. **Do NOT wait for `shepherd_report.json` or other matrix jobs to complete.**
2. **Check `<artifacts_dir>/logs/` or locate `<JOB_ID>`**:
   ```bash
   gh pr checks <PR_NUMBER> --repo <owner>/<repo>
   # Or view jobs for the workflow run:
   gh run view <RUN_ID> --repo <owner>/<repo>
   ```
3. **Fetch failed step logs immediately**:
   ```bash
   gh run view --job <JOB_ID> --log-failed --repo <owner>/<repo>
   ```
4. **Diagnose and Act**: Identify the syntax, configuration, or code error. If
   PR-caused, fix, commit, and push immediately.

### 🌟 Primary Triage Source for Completed Runs & Test Failures: CI Shepherd JSON Report

The Cobalt CI pipeline generates structured **CI Shepherd JSON reports**
(`shepherd_report.json`) uploaded as `shepherd-report-<platform>` artifacts
(e.g., `shepherd-report-android-arm64`, `shepherd-report-linux-modular`,
`shepherd-report-tvos`).

1. **Step 1: Check Pipeline Stage Status (`checks`)**:
   - Inspect `checks` in `shepherd_report.json` (or `shepherd_pr.py` output).
   - If `"build": "failure"`, compilation failed—inspect compiler errors in
     `<artifacts_dir>/logs/` or via `gh run view --job <JOB_ID> --log-failed`.
   - If `"on-host-test"`, `"web-tests"`, or `"on-device-test"` is `"failure"`,
     proceed to Step 2.
2. **Step 2: Inspect Failing Tests (`test_failures`)**:
   - `shepherd_report.json` lists failing test case names and assertion messages
     directly—no megabyte log grepping needed.
3. **Step 3: Targeted Fallback (Only If Necessary)**:
   - Inspect `<artifacts_dir>/logs/*.log` or fetch `--log-failed` if stack
     traces were truncated, filtering targeted signatures:
     - **Compiler Errors**: `grep -E ":[0-9]+:[0-9]+: (fatal )?error:"`
     - **Build Failures (Ninja/Siso)**: `grep -E "FAILED:|err:.*failed:"`
     - **Config Errors (GN)**: `grep "ERROR at "`
     - **Unit Tests (GTest)**: `grep -E "\[  FAILED  \]"`
     - **Crashes**: `grep -E "Segmentation fault|SIGSEGV|Traceback"`
4. **Step 4: Record Triage in the Action Ledger**:
   - Update `shepherding_report.md` (with `shepherding_report.lock`) recording
     classification, failing target/test, and next action.

---

## 4. Resolving the Root Cause

### Permitted vs. Prohibited Fixes
- **Permitted Fixes (PR-Caused Only)**:
  - Fix compiler, build, and linter errors introduced by the PR.
  - Fix test failures or regressions caused by the PR's changes.
  - Resolve merge conflicts with the base branch.
  - Implement requested code changes from review comments.
- **Prohibited Fixes (Escalate Instead)**:
  - Do NOT alter product code or CI configs to work around unrelated flakes,
    CI runner outages, or pre-existing base-branch bugs.
  - **NEVER autonomously filter tests**: Never modify test filter manifests
    (e.g., `*_filter.json`, `filter.txt`), disable tests, or add test skip
    annotations, and **never in a PR with an unrelated feature**.

### Flake / Infra / Base Drift Protocol

If triage reveals a failure is not caused by the PR diff:

1. **Transient Verification (Rerun Once)**:
   - Trigger one rerun of failed jobs:
     ```bash
     gh run rerun <RUN_ID> --failed --repo <owner>/<repo>
     ```
   - Log the rerun ID in the Action Ledger (`shepherding_report.md`).
2. **Base Drift Check & Upstream Search**:
   - If the failure persists, measure how far behind the base branch the PR is:
     ```bash
     git fetch origin <base_branch>
     git rev-list --count HEAD..origin/<base_branch>
     ```
   - Search recent upstream commits and merged PRs on `origin/<base_branch>` for
     the failing test name, error signature, or subsystem:
     ```bash
     git log origin/<base_branch> --grep="<test_name_or_symbol>" -n 10 --oneline
     gh pr list --repo <owner>/<repo> --search "<test_name_or_error>" --state merged -L 5
     ```
3. **Autonomous Rebase & Sync**:
   - If the PR is behind base or an upstream fix landed on `<base_branch>`,
     autonomously rebase onto `origin/<base_branch>` (or use `gh stack sync` for
     stacked PRs) and push to test against the updated base:
     ```bash
     # Regular PR:
     git rebase origin/<base_branch>
     git push --force-with-lease
     # Stacked PRs (use gh stack rather than raw git push):
     gh stack sync
     gh stack submit --auto
     ```
   - Record the base SHA and rebase/sync action in the Action Ledger.
4. **Mandatory Human Escalation (Persistent Failure on Latest Base)**:
   - If the failure persists after rebasing onto the latest base (or the PR was
     already on the latest base):
     - **HALT** shepherding. Do not retry in a loop.
     - Do **NOT** edit test filter manifests or skip tests.
     - Write a structured escalation entry in `shepherding_report.md` (and
       report up to the Watch Coordinator / Main Agent) containing:
       - Exact failing job, test case, and log snippet.
       - Confirmation that PR changes do not touch the failing subsystem.
       - Verified `origin/<base_branch>` commit SHA after rebase.
       - Whether the latest base branch itself exhibits the failure.

### Implementing PR-Caused Fixes
1. **Immediate Remediation**: Do not wait for remaining matrix jobs to finish.
2. **Be Minimally Invasive**: Make the smallest, targeted change needed.
3. **Verify Locally**: Build or run affected tests/linters locally.
4. **Commit & Push**: Stage only intended files (never `git add .` or
   `git add -A`, and never commit `shepherding_report.md`,
   `shepherding_report.lock`, or `artifacts/`), commit with proper Git trailers,
   and push (`git push`, or `gh stack submit --auto` for `stack/` branches).
   Pushing cancels obsolete pending runs and triggers fresh CI checks.
5. **Update Action Ledger**: Record commit SHA, target check, and outcome in
   `shepherding_report.md`, then return control to the Watch Coordinator.

### Resolving Merge Conflicts
*(Note: Merging/rebasing the target base branch into the PR branch to resolve
conflicts is permitted; merging the PR into the base branch is strictly
forbidden.)*
1. **Regular (Non-Stack) PRs**:
   ```bash
   git fetch origin
   git merge origin/<base_branch>   # or: git rebase origin/<base_branch>
   # Resolve conflict markers (<<<<<<<, =======, >>>>>>>), verify build/tests
   git add <resolved_files>
   git commit --no-edit              # or: git rebase --continue
   git push
   ```
2. **Stacked PRs (`stack/` branch prefix)**:
   ```bash
   gh stack sync
   # Resolve conflict markers, verify build/tests
   git add <resolved_files>
   gh stack rebase --continue
   gh stack submit --auto
   ```
3. **Update Action Ledger**: Log the conflict resolution commit/rebase in
   `shepherding_report.md`.

---

## 5. Addressing Review Comments

1. Locate the file and line indicated in the watch output (`path:line`).
2. Implement the requested code changes adhering to the repository style guide.
3. Verify changes locally with tests/linters, commit, and push.
4. **Reply Policy (Strict Single-Use Consent)**:
   - **Never** post GitHub replies autonomously.
   - Only run `gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/<COMMENT_ID>/replies`
     if explicit single-use user consent was granted for that reply.
   - Otherwise, record the code fix commit SHA and a suggested draft reply in
     `shepherding_report.md`, and report to the Watch Coordinator / Main Agent
     so the user can approve posting the reply (or post it directly).

---

## 6. Action Ledger & Shepherding Status Report (`shepherding_report.md`)

`shepherding_report.md` at the workspace root is the single source of truth
for progress, handoffs between Watch Coordinators, and auditability of every
action taken.

### 🔒 Concurrency Control & Synchronized Access (`shepherding_report.lock`)

Because the Watch Coordinator and Issue Workers may read or update
`shepherding_report.md`, every read-modify-write cycle **MUST** follow these
three rules:

1. **Advisory File Locking (`shepherding_report.lock`)**:
   Acquire an exclusive lock on `shepherding_report.lock` (`flock -x` in shell
   or `fcntl.flock` in Python) before reading or writing.
2. **Re-Read Before Write**:
   Always read the latest `shepherding_report.md` *after* acquiring the
   exclusive lock. Never overwrite with stale in-memory content.
3. **Atomic Replacement**:
   Write to a temporary file on the same filesystem
   (`shepherding_report.md.tmp.<pid>`) and atomically replace via `mv -f` or
   `os.replace`.

```python
import fcntl
import os

with open("shepherding_report.lock", "w", encoding="utf-8") as lock_f:
    fcntl.flock(lock_f, fcntl.LOCK_EX)
    content = ""
    if os.path.exists("shepherding_report.md"):
        with open("shepherding_report.md", "r", encoding="utf-8") as rf:
            content = rf.read()
    # Apply updates to Overview, tables, and Ledger of Actions Taken
    tmp_file = f"shepherding_report.md.tmp.{os.getpid()}"
    with open(tmp_file, "w", encoding="utf-8") as wf:
        wf.write(updated_content)
    os.replace(tmp_file, "shepherding_report.md")
```

### Standard Report Template

Use this structure when initializing and updating `shepherding_report.md`:

````markdown
# PR Shepherding Report

## 1. Overview
- **Monitored PR(s)**: [#<PR_NUMBER> (<PR_TITLE>)](<PR_URL>)
- **Repository**: `<owner>/<repo>`
- **Branch**: `<head_branch>` -> `<base_branch>`
- **Overall Status**: `IN PROGRESS` | `SUPER GREEN` | `ESCALATED`
- **CI Checks**: `<passed>/<total> Pass` (`<failed> Fail`, `<pending> Pending`)
- **Review Comments**: `<unresolved> Unresolved` (`<resolved> Resolved`)
- **Flakes / Infrastructure Issues**: `<flake_count>`

---

## 2. Ledger of Actions Taken
| Timestamp / Iteration | Target PR & Check / Thread | Classification | Action Taken | Outcome / Verification |
| :--- | :--- | :--- | :--- | :--- |
| `YYYY-MM-DD HH:MM:SS` (Iter 1) | `PR #123 / <check_or_thread>` | `PR-Caused` \| `Flake` \| `Infra` \| `Base-Drift` \| `Review-Comment` \| `Merge-Conflict` | `<commit SHA / rerun run ID / rebase / reply>` | `<Verifying / Passed / Escalated>` |

---

## 3. CI Checks Status & Triage
| Platform / Check | Status | Failing Job / Tests | Classification | Action |
| :--- | :--- | :--- | :--- | :--- |
| `<platform>` | `FAILED` | `<failing job/test>` | `PR-Caused` | `<fix>` |

---

## 4. Review Comments
| Author | Location | Comment Snippet | Status | Resolution / Draft Reply |
| :--- | :--- | :--- | :--- | :--- |
| `@<author>` | `<path:line>` | `<snippet>` | `Code Fixed (Reply Pending Consent)` | `<commit SHA + draft reply>` |

---

## 5. Flaky Tests, Base Drift & Infrastructure Escalations
*(Document non-PR failures here with concrete evidence)*
- **Job Name**: `<job_name>` ([Run Details](<run_url>))
  - **Error Signature**: `<regex pattern or failure line>`
  - **Base Drift / Upstream Check**: `<commits behind base, upstream PR/commit if any, verified base SHA>`
  - **Log Snippet**:
    ```text
    <log snippet showing failure>
    ```
  - **Evidence**: `<Explanation of why failure is unrelated to PR changes>`
  - **Action Taken**: `<Rerun <RUN_ID> / Rebased onto <SHA> / Escalated to user>`
````
