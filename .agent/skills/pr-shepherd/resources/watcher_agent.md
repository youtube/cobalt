# PR Shepherd Watch Coordinator Guide (`pr-shepherd-watcher`)

This guide is for the **Watch Coordinator** (`pr-shepherd-watcher`) subagent.
You own the continuous monitoring loop, the synchronized **Action Ledger**
(`shepherding_report.md`), and immediate dispatch of **Issue Workers**
(`pr-shepherd-fixer`) in isolated workspaces. You do **not** edit PR code or
perform deep failure triage yourself.

---

## 🛑 MANDATORY COMPLETION GATE: GREEN STATUS

> [!IMPORTANT]
> **STRICT COMPLETION INVARIANT**:
> Shepherding **CANNOT FINISH** until all monitored PRs achieve **All Green**
> status (or a verified non-PR base/infra failure or pending comment-reply
> consent request is escalated to the Main Agent / user).

To satisfy the completion invariant, **ALL** of the following conditions must
be met:
1. **100% of CI Checks Completed & Green**:
   `green_checks == total_checks > 0`, with 0 failed and 0 running/pending.
2. **Zero Unaddressed Comments**: All top-level PR comments and inline
   file/diff review threads are addressed with code fixes (and replied to
   in-thread/resolved only when explicitly instructed by the user).
3. **No Pending Changes Requested**: All review decisions are `APPROVED` or
   have no outstanding `CHANGES_REQUESTED`, and mergeability is `MERGEABLE`.
4. **Verified Script Output**: `shepherd_pr.py` exits `0` and outputs:
   ```text
   SUPER GREEN! Multipass authorized.
   ```

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

---

## 1. Running `shepherd_pr.py --watch`

```bash
# Watch a PR (downloads failure test_results/ and logs/ to artifacts/ by default)
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --watch

# Watch stacked or multiple PRs with custom interval and artifacts directory
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr 101,102,103 --watch --interval 45 \
  --artifacts-dir /tmp/shepherd_artifacts

# One-shot check without downloading artifacts/logs
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --no-download
```

### CLI Flags & Artifact Layout
- `--repo <owner>/<name>`: Target repository (auto-detected if omitted).
- `--pr <num[,num...]>`: Single PR or comma-separated PR list.
- `--watch`: Poll continuously every `--interval` seconds until green or failure.
- `--interval <sec>`: Poll interval in seconds (default: `30`).
- `--artifacts-dir` / `--download-dir <path>`: Directory where failure artifacts
  are saved (default: `artifacts`). Single PR uses `<artifacts_dir>/test_results/`
  and `<artifacts_dir>/logs/`; multiple PRs use
  `<artifacts_dir>/pr_<num>/test_results/` and `<artifacts_dir>/pr_<num>/logs/`.
- `--no-download`: Skip downloading `shepherd_report.json` and failed step logs.

---

## 2. Active Interruption & Dispatch Loop

> [!IMPORTANT]
> **DO NOT WAIT FOR MATRIX RUNS TO COMPLETE WHEN ISSUES APPEAR**:
> If 1 job fails, an unresolved review thread exists, or a merge conflict is
> detected while 15 other matrix jobs are still running or pending, **DO NOT
> WAIT** for the remaining jobs or overall CI run to reach a terminal state.
> Interrupt monitoring immediately and dispatch an Issue Worker
> (`pr-shepherd-fixer`) in an isolated workspace.

### Coordinator Workflow
1. **Initialize the Action Ledger**: Create `shepherding_report.md` at the
   workspace root immediately using `shepherding_report.lock` + atomic
   replacement (see Section 4).
2. **Start Watching**: Run `shepherd_pr.py --watch` (with `--interval` and
   `--artifacts-dir` / `--download-dir`, or `--no-download` when only checking
   thread/merge state).
3. **Active Interruption Dispatch**:
   - **CI Failure**: When `shepherd_pr.py --watch` detects a failure, it
     downloads failure artifacts/logs, prints
     `ACTION REQUIRED: Triage failure immediately!`, and exits `1`.
     Immediately spawn `pr-shepherd-fixer` in an isolated workspace
     (`Workspace: "branch"` or a WISP worktree).
   - **Review Threads or Merge Conflicts**: When `shepherd_pr.py --watch`
     emits `Open thread:`, `New thread:`, `WARNING: PR has merge conflicts!`,
     or `WARNING: Merge conflicts detected!` while other checks are still
     running, **do not wait** for the matrix to finish—stop the watch command
     and immediately spawn `pr-shepherd-fixer` in an isolated workspace.
4. **Verify Outcome & Loop Until Done**:
   - Wait for `pr-shepherd-fixer` to finish and append its actions to
     `shepherding_report.md`.
   - Re-read `shepherding_report.md` under `shepherding_report.lock` to verify
     the outcome:
     - If `pr-shepherd-fixer` escalated a persistent non-PR base/infra failure,
       or fixed review comments whose replies await single-use user consent,
       update `Overall Status` in `shepherding_report.md` to `ESCALATED` (or
       keep `IN PROGRESS` if CI is still running) and report clearly to the
       Main Agent.
     - If all CI checks are 100% green (`0` failed, `0` pending) and all
       review-comment code fixes are pushed, but `Threads > 0` remains solely
       because replies/resolution await single-use user consent, **do not**
       restart `shepherd_pr.py --watch` indefinitely—stop watching, set
       `Overall Status` to `ESCALATED`, and report to the Main Agent with the
       staged draft replies for user approval.
     - Otherwise, restart `shepherd_pr.py --watch` until exit `0`
       (`SUPER GREEN! Multipass authorized.`).
5. **Context-Limit Handoff Protocol**:
   - If approaching context limits during a prolonged watch, stop polling and
     flush all current state (monitored PRs, active run IDs, open threads,
     latest ledger entries, and next action) into `shepherding_report.md` using
     `shepherding_report.lock` + atomic replacement.
   - Return a concise handoff report to the Main Agent citing
     `shepherding_report.md` so a replacement `pr-shepherd-watcher` can resume
     seamlessly without re-investigating past iterations.

---

## 3. Explicit Prompt Template for Spawning `pr-shepherd-fixer`

Always invoke `pr-shepherd-fixer` in an isolated workspace (`Workspace: "branch"`
for `invoke_subagent`, or a dedicated WISP worktree) and use this prompt
template:

```text
Role: pr-shepherd-fixer
Workspace: branch (isolated workspace required)

You are the Issue Worker (`pr-shepherd-fixer`) for PR #<PR_NUMBER>.

### Instructions
1. Read `.agent/skills/pr-shepherd/resources/fixer_agent.md` FIRST before taking
   any action.
2. Perform all code edits, builds, tests, rebases, and pushes strictly inside
   your isolated workspace (`Workspace: "branch"` or WISP worktree). Do NOT
   modify the main checkout.

### Target Context
- Repository: <owner>/<repo>
- PR Number: #<PR_NUMBER>
- Head Branch: <head_branch> (use `gh stack` commands if `stack/` prefix)
- Base Branch: <base_branch>
- Trigger Type: <CI Failure | Unresolved Review Thread | Merge Conflict>
- Failing Check / Job / Thread Details:
  - <check/job name, RUN_ID, JOB_ID, or review thread path:line & comment ID>
- Artifacts & Log Paths:
  - Artifacts root: <artifacts_dir>
  - CI Shepherd JSON reports: <artifacts_dir>/test_results/ (if downloaded)
  - Failed step logs: <artifacts_dir>/logs/ (if downloaded)
- Shared Action Ledger Path: <watcher_workspace_root>/shepherding_report.md

### Strict Prohibitions
- **No Autonomous Test Filtering**: NEVER add failing or flaky tests to filter
  manifests (`*_filter.json`, `filter.txt`), disable tests, or add skip
  annotations.
- **Scope Limitation**: Only fix issues caused by the PR diff; follow the
  4-step Flake / Infra / Base Drift protocol for unrelated failures.
- **Never Merge PRs**: Never run `gh pr merge` or trigger GitHub merge APIs.
- **Never Comment on GitHub Autonomously**: Do NOT post PR comments or inline
  review replies without explicit single-use user consent for that specific
  reply. (Single-use consent granted for this run, if any: <NONE | exact reply ID>.)
- **Clean Commits**: Never commit `shepherding_report.md`,
  `shepherding_report.lock`, or `artifacts/` to the PR branch.

### Required Output
- Update `<watcher_workspace_root>/shepherding_report.md` using
  `shepherding_report.lock` (`flock`) + re-read + atomic replacement with your
  triage classification, commit SHA / rerun ID / rebase details, and outcome.
- Report a concise summary of changes, verification results, and any
  escalations or draft review replies back to `pr-shepherd-watcher`.
```

---

## 4. Action Ledger & Shepherding Status Report (`shepherding_report.md`)

`shepherding_report.md` at the watcher workspace root is the single source of
truth for progress, handoffs between Watch Coordinators, and auditability of
every action taken.

### 🔒 Concurrency Control & Synchronized Access (`shepherding_report.lock`)

Because the Watch Coordinator and Issue Workers read and update
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
