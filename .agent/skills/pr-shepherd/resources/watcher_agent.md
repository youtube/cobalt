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
> Shepherding **CANNOT FINISH** until all monitored PRs achieve **SUPER GREEN**
> status, **CI GREEN (Awaiting Human)** status once review-comment code fixes
> are pushed/escalated, or a verified non-PR base/infra failure is escalated to
> the Main Agent / user.

To satisfy the completion invariant, **ALL** of the following conditions must
be met:
1. **100% of Required CI Checks Completed & Green**:
   All required checks (`isRequired` / `requiredStatusCheckContexts`) are in
   `{"SUCCESS", "NEUTRAL"}` with `0` failed and `0` pending (`"SKIPPED"` is
   rejected on required checks).
2. **Mergeable & Open**:
   `state == "OPEN"`, `isDraft == False`, and `mergeable == "MERGEABLE"`.
3. **Review Threads Addressed / Escalated (Never Replied or Resolved
   Autonomously)**:
   Clear code changes requested in review comments are implemented and pushed
   (or escalated if ambiguous), and draft replies are recorded in
   `shepherding_report.md`—never posted or resolved autonomously.
4. **Verified Script Output**:
   - **Exit `0` (`SUPER GREEN`)**: All required CI checks green, mergeable, `0`
     unresolved review threads, and no `CHANGES_REQUESTED`:
     ```text
     SUPER GREEN! Multipass authorized.
     ```
   - **Exit `3` (`CI GREEN`)**: All required CI checks green, mergeable, open,
     non-draft, and only unresolved review threads or `CHANGES_REQUESTED`
     remain:
     ```text
     CI GREEN. Awaiting human on review threads/decision (agents: escalate only, never reply or resolve).
     ```

---

## 🚫 STRICT PROHIBITIONS (PUBLIC-REPO SAFE POLICIES)

### 1. DO NOT MERGE PULL REQUESTS

> [!CAUTION]
> **NEVER MERGE PRS**:
> You must **NEVER merge pull requests** or trigger PRs to be merged under any
> circumstances (e.g., via `gh pr merge`, GitHub GraphQL/REST API merge
> endpoints, or enabling auto-merge).
> - **Completion Boundary**: Your responsibility ends at **SUPER GREEN** or
>   **CI GREEN** escalation.
> - **User Delegation Only**: PR merging is strictly reserved for the user and
>   requires explicit, unambiguous user instructions in chat.

### 2. DO NOT COMMENT, REPLY, SUBMIT REVIEWS, OR RESOLVE REVIEW THREADS

> [!CAUTION]
> **NO AUTONOMOUS COMMENTS, REPLIES, REVIEWS, OR THREAD RESOLUTION**:
> Neither `pr-shepherd` nor any worker/subagent has an exemption to post on
> GitHub. You must **NEVER** run `gh pr comment`, `gh api .../replies`,
> `gh pr review`, or GraphQL `resolveReviewThread`/`unresolveReviewThread`
> without explicit single-use user consent in chat for that exact action.
> - **Escalate Only**: Any review comment responses MUST be drafted in
>   `shepherding_report.md` and escalated to the Main Agent / user for review.
> - **Single-Use Consent**: Approval granted by the user for a specific reply or
>   action expires immediately after one use; mandatory re-approval is required
>   for any subsequent action.

---

## 1. Running `shepherd_pr.py --watch`

```bash
# Watch a PR (downloads failure test_results/ and logs/ to artifacts/ by default)
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --watch

# Re-watch CI after review-comment code fixes are pushed (ignoring already-fixed thread comment IDs)
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --watch \
  --ignore-comment-ids <COMMENT_ID_1>,<COMMENT_ID_2>

# Watch stacked or multiple PRs with custom interval and artifacts directory
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr 101,102,103 --watch --interval 45 \
  --artifacts-dir /tmp/shepherd_artifacts

# One-shot check without downloading artifacts/logs
python3 .agent/skills/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --no-download
```

### CLI Flags, Exit Codes & Compact Output
- `--repo <owner>/<name>`: Target repository (auto-detected if omitted).
- `--pr <num[,num...]>`: Single PR or comma-separated PR list.
- `--watch`: Poll silently every `--interval` seconds while required checks are
  pending and no actionable blocker exists.
- `--interval <sec>`: Poll interval in seconds (default: `30`).
- `--artifacts-dir` / `--download-dir <path>`: Directory where failure artifacts
  are saved (default: `artifacts`). Single PR uses `<artifacts_dir>/test_results/`
  and `<artifacts_dir>/logs/`; multiple PRs use
  `<artifacts_dir>/pr_<num>/test_results/` and `<artifacts_dir>/pr_<num>/logs/`.
- `--no-download`: Skip saving `shepherd_report.json` and failed step logs to
  disk.
- `--ignore-comment-ids <id[,id...]>`: Comma-separated (or repeated) comment
  `databaseId`s to ignore while `--watch` polls pending CI after review-comment
  code fixes are pushed. Does **not** suppress `CI GREEN` (exit `3`) once CI
  finishes if unresolved threads still exist on GitHub.

**Exit Codes**:
- **`0` (`SUPER GREEN`)**: Prints `SUPER GREEN! Multipass authorized.`
- **`1` (Actionable Blocker)**: Prints **only** the compact fixer-dispatch
  block (`headRefName -> baseRefName`, `headRefOid`, failed required checks with
  `run_id`/`job_id`, failing `shepherd_report` summary, unignored unresolved
  threads with `comment_id`, and `artifacts_dir` paths).
- **`3` (`CI GREEN`)**: Prints any remaining unresolved threads / PR state and
  `CI GREEN. Awaiting human on review threads/decision (agents: escalate only, never reply or resolve).`
- **`4` (API / CLI Error)**: Persistent `gh` or GraphQL failure (after 3
  consecutive failed polls in `--watch`, or on single-shot failure).

---

## 2. Active Interruption & Dispatch Loop

> [!IMPORTANT]
> **DO NOT WAIT FOR MATRIX RUNS TO COMPLETE WHEN ISSUES APPEAR**:
> If 1 required check fails, an unignored unresolved review thread exists, a
> merge conflict (`mergeable=CONFLICTING`), `draft=True`, or `state != OPEN` is
> detected while other matrix jobs are still pending, `shepherd_pr.py --watch`
> exits `1` **immediately**. Dispatch an Issue Worker (`pr-shepherd-fixer`) in
> an isolated workspace without waiting for the rest of the matrix.

### Coordinator Workflow
1. **Initialize the Action Ledger**: Create `shepherding_report.md` at the
   workspace root immediately using `shepherding_report.lock` + atomic
   replacement (see Section 4).
2. **Start Watching**: Run `shepherd_pr.py --watch` (passing
   `--ignore-comment-ids` for any unresolved thread `comment_id`s already marked
   `Code Fixed (Reply Pending Consent)` or `Escalated` in
   `shepherding_report.md`).
3. **Handle Exit Status**:
   - **Exit `0` (`SUPER GREEN! Multipass authorized.`)**: Update
     `Overall Status` to `SUPER GREEN` in `shepherding_report.md` and report to
     the Main Agent.
   - **Exit `3` (`CI GREEN. Awaiting human...`)**:
     - Re-read `shepherding_report.md` under `shepherding_report.lock`.
     - If any unresolved thread in the output has **not** yet been fixed or
       triaged in `shepherding_report.md`, spawn `pr-shepherd-fixer` for it (with
       strict prohibition on replying/resolving).
     - Otherwise (all threads already have `Code Fixed (Reply Pending Consent)`
       or `Escalated`, or only `CHANGES_REQUESTED` remains), **do not** loop
       `--watch`: set `Overall Status` to `CI GREEN (Awaiting Human)` in
       `shepherding_report.md` and report to the Main Agent with the drafted
       replies for user review.
   - **Exit `1` (Compact Fixer-Dispatch Block)**:
     - If `state != OPEN`, `draft=True`, or batch-wide zero-pending exhaustion
       occurred with no fixer-actionable issue, escalate to the Main Agent.
     - Otherwise (failed required check, `CONFLICTING`, or unignored unresolved
       thread), immediately spawn `pr-shepherd-fixer` in an isolated workspace
       (`Workspace: "branch"` or a WISP worktree), passing the compact block's
       `headRefName -> baseRefName`, `headRefOid`, `run_id`/`job_id`,
       `comment_id`, and `artifacts_dir` paths.
   - **Exit `4` (Persistent `gh`/GraphQL Error)**: Set `Overall Status` to
     `ESCALATED` in `shepherding_report.md` and report the API error to the Main
     Agent.
4. **Verify Outcome & Loop Until Done**:
   - Wait for `pr-shepherd-fixer` to finish and append its actions to
     `shepherding_report.md`.
   - Re-read `shepherding_report.md` under `shepherding_report.lock`:
     - If `pr-shepherd-fixer` escalated a persistent non-PR base/infra failure,
       set `Overall Status` to `ESCALATED` and report clearly to the Main Agent.
     - Otherwise, restart `shepherd_pr.py --watch`, passing
       `--ignore-comment-ids <comma-separated comment_ids>` for any unresolved
       threads already marked `Code Fixed (Reply Pending Consent)` or
       `Escalated`, until exit `0` or exit `3`.
5. **Context-Limit Handoff Protocol**:
   - If approaching context limits during a prolonged watch, stop polling and
     flush all current state (monitored PRs, active run IDs, open threads and
     ignored `comment_id`s, latest ledger entries, and next action) into
     `shepherding_report.md` using `shepherding_report.lock` + atomic
     replacement.
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
- Branch: <headRefName> -> <baseRefName> (HEAD SHA: <headRefOid>; use `gh stack`
  commands if `stack/` prefix)
- Trigger Type: <CI Failure | Unresolved Review Thread | Merge Conflict>
- Failing Check / Job / Thread Details:
  - <check/job name, run_id=<RUN_ID>, job_id=<JOB_ID>, or Unresolved thread [comment_id=<COMMENT_ID>] path:line>
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
- **Never Comment, Reply, Review, or Resolve Threads Autonomously**: Do NOT run
  `gh pr comment`, `gh api .../replies`, `gh pr review`, or
  `resolveReviewThread`/`unresolveReviewThread` without explicit single-use
  user consent for that exact action. (Single-use consent granted for this run,
  if any: <NONE | exact reply ID>.) Draft responses in `shepherding_report.md`.
- **Clean Commits**: Never commit `shepherding_report.md`,
  `shepherding_report.lock`, or `artifacts/` to the PR branch.

### Required Output
- Update `<watcher_workspace_root>/shepherding_report.md` using
  `shepherding_report.lock` (`flock`) + re-read + atomic replacement with your
  triage classification, commit SHA / rerun ID / rebase details, `comment_id`
  and drafted reply (if any), and outcome.
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
- **Branch**: `<head_branch>` -> `<base_branch>` (`<head_sha>`)
- **Overall Status**: `IN PROGRESS` | `CI GREEN (Awaiting Human)` | `SUPER GREEN` | `ESCALATED`
- **CI Checks**: `<passed>/<total> Required Pass` (`<failed> Fail`, `<pending> Pending`)
- **Review Comments**: `<unresolved> Unresolved` (`<drafted> Drafted / Awaiting Human`)
- **Flakes / Infrastructure Issues**: `<flake_count>`

---

## 2. Ledger of Actions Taken
| Timestamp / Iteration | Target PR & Check / Thread | Classification | Action Taken | Outcome / Verification |
| :--- | :--- | :--- | :--- | :--- |
| `YYYY-MM-DD HH:MM:SS` (Iter 1) | `PR #123 / <check_or_thread>` | `PR-Caused` \| `Flake` \| `Infra` \| `Base-Drift` \| `Review-Comment` \| `Merge-Conflict` | `<commit SHA / rerun run ID / rebase / draft reply>` | `<Verifying / Passed / Code Fixed (Reply Pending Consent) / Escalated>` |

---

## 3. CI Checks Status & Triage
| Platform / Check | Status | Failing Job / Tests | Classification | Action |
| :--- | :--- | :--- | :--- | :--- |
| `<platform>` | `FAILED` | `<failing job/test>` | `PR-Caused` | `<fix>` |

---

## 4. Review Comments
| Comment ID | Author | Location | Comment Snippet | Status | Draft Reply / Fix SHA |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `<comment_id>` | `@<author>` | `<path:line>` | `<snippet>` | `Code Fixed (Reply Pending Consent)` \| `Drafted (Never Posted)` | `<commit SHA + proposed reply>` |

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
