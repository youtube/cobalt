---
name: pr-shepherd
description: >-
  Shepherds PRs to green. Monitors status checks and review comments via a
  3-tier delegation loop, immediately triages and fixes failures without
  waiting for the full matrix, uses CI Shepherd reports and step logs, and
  maintains an action ledger to achieve All Green.
---

# Pull Request Shepherd

The **PR Shepherd** skill autonomously monitors Pull Requests until all
review comments are addressed, all CI checks complete, and the PR reaches
**All Green**. It uses a **3-tier delegation architecture** to protect main
agent context, leverages **CI Shepherd JSON reports** (`shepherd_report.json`)
and failed step logs to pinpoint failures rapidly, and records every step in a
synchronized **Action Ledger** (`shepherding_report.md`).

---

## 🛑 MANDATORY COMPLETION GATE: ALL GREEN

> [!IMPORTANT]
> **STRICT COMPLETION INVARIANT**:
> The agent **CANNOT FINISH** its task or declare success until the PR has
> achieved **All Green** status (or a verified non-PR blocker requires human
> escalation).

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

If any check is still running or any comment thread is unaddressed, the watch
loop **MUST CONTINUE MONITORING AND SHEPHERDING**. Do not stop or exit early.

---

## ⚡ IMMEDIATE ACTION ON FAILURE (ACTIVE INTERRUPTION)

> [!IMPORTANT]
> **DO NOT WAIT FOR MATRIX COMPLETION WHEN FAILURES ARE DETECTED**:
> The moment **ANY** check, job, or step fails (`FAILURE`, `CANCELLED`,
> `ERROR`, `TIMED_OUT`), or any unresolved review comment or merge conflict
> appears, the Watch Coordinator **MUST IMMEDIATELY INTERRUPT AND DISPATCH**:
> - **Active Interruption (Never Wait for Settling)**: If 1 job fails while
>   other matrix jobs are still running or pending, **DO NOT WAIT** for the
>   remaining jobs or a full status snapshot. Spawn an Issue Worker
>   (`pr-shepherd-fixer`) immediately.
> - **Early-Failure Fast Path**: If a job fails early (e.g., workflow syntax,
>   `depot_tools` setup, action schema evaluation, or early build steps),
>   `shepherd_report.json` will not exist yet. **DO NOT WAIT** for
>   `shepherd_report.json` or downstream artifacts! Immediately inspect
>   downloaded step logs or fetch them via
>   `gh run view --job <JOB_ID> --log-failed` and diagnose the root cause.
> - **Rapid Remediation & Cancellation of Obsolete Runs**: When a failure is
>   caused by PR changes, implement and push the corrective commit immediately.
>   Pushing the fix cancels obsolete pending jobs and starts a fresh CI run on
>   the corrected code.

---

## ⚠️ Scope of Fixes: PR-Caused Issues vs. Flakes / Base Drift

> [!IMPORTANT]
> **FIX SCOPE LIMITATION & TEST FILTERING PROHIBITION**:
> Fixes by the PR Shepherd are **strictly limited to issues caused by the PR**.
> - **PR-Caused Issues (Fix)**: Compile errors, test failures, linter issues,
>   merge conflicts, or regressions introduced by the PR, plus code changes
>   requested in review comments.
> - **No Autonomous Test Filtering**: The shepherd must **NEVER** autonomously
>   add failing or flaky tests to test filter manifests (e.g., `*_filter.json`,
>   `filter.txt`), disable tests, or add test skip annotations, and **never in
>   a PR with an unrelated feature**.
> - **Flake / Infra / Base Drift Protocol**:
>   Do **NOT** attempt to work around flaky tests, runner timeouts, network
>   failures, or CI infrastructure outages by editing unrelated code or CI
>   configs. Follow this 4-step protocol:
>   1. **Transient Verification**: Rerun affected jobs once
>      (`gh run rerun <RUN_ID> --failed --repo <owner>/<repo>`).
>   2. **Base Drift & Upstream Search**: If the failure persists, check how far
>      behind the base branch the PR is
>      (`git fetch origin <base> && git rev-list --count HEAD..origin/<base>`)
>      and search recent upstream commits (`git log origin/<base> --grep=...`)
>      and merged PRs (`gh pr list --state merged`) for fixes that already
>      landed on `<base>`.
>   3. **Autonomous Rebase & Sync**: If the PR is behind base or an upstream
>      fix landed on `<base>`, rebase on `origin/<base>`
>      (`git rebase origin/<base>` or `gh stack sync`) and push to verify
>      against the updated base.
>   4. **Mandatory Human Escalation**: If the failure persists on the latest
>      base branch (or the PR is already up to date with base), **HALT**
>      shepherding—do not retry in a loop or edit test filters—and escalate to
>      the user with a structured report detailing the failing test/job, log
>      evidence, PR independence, rebased base SHA, and base-branch status.

---

## 🚫 STRICT PROHIBITIONS (PUBLIC-REPO SAFE POLICIES)

### 1. DO NOT MERGE PULL REQUESTS

> [!CAUTION]
> **NEVER MERGE PRS**:
> The PR Shepherd skill and all associated agents **MUST NOT** merge pull
> requests or trigger PRs to be merged under any circumstances (e.g., via
> `gh pr merge`, GitHub GraphQL/REST API merge endpoints, or auto-merge).
> - **Completion Boundary**: The shepherd's role ends at **All Green** /
>   **SUPER GREEN** status (or human escalation).
> - **User Delegation Only**: PR merging is strictly reserved for the user and
>   requires explicit, unambiguous user instructions in chat.

### 2. PREVENT AUTOMATIC COMMENTING ON GITHUB

> [!CAUTION]
> **NO AUTOMATIC COMMENTS OR REPLIES**:
> The PR Shepherd skill and all associated agents **MUST NOT** post comments or
> replies on GitHub PRs or issues autonomously without explicit user
> instruction in chat.
> - **No Autonomous Commenting**: Never post PR comments, inline review
>   replies, or issue comments autonomously.
> - **Single-Use Consent**: Any consent or instruction granted by the user to
>   post a comment or reply is **strictly single-use and non-persistent**. It
>   expires immediately after being used once; mandatory re-approval is
>   required for any subsequent comment or reply.
> - **In-Thread Replies When Instructed**: Only when explicitly instructed by
>   the user for a specific reply, reply directly in-thread
>   (`gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/<COMMENT_ID>/replies`).
>   Otherwise, implement the requested code changes, log the pending reply in
>   `shepherding_report.md`, and request single-use user consent if a GitHub
>   reply is needed.

---

## Running the Shepherd Script

`shepherd_pr.py` tracks PR status check rollups, CI Shepherd JSON reports,
review comment threads, and mergeability. When failures occur, it automatically
downloads CI Shepherd `shepherd_report.json` artifacts (to `test_results/`) and
failed step logs (to `logs/`) under `--artifacts-dir`.

```bash
# Watch a single PR (downloads failure artifacts/logs to artifacts/ by default)
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo youtube/cobalt --pr 123 --watch

# Watch stacked or multiple PRs with a custom poll interval and artifacts path
# (multi-PR artifacts are stored under <artifacts_dir>/pr_<num>/)
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo youtube/cobalt --pr 101,102,103 --watch --interval 45 \
  --artifacts-dir /tmp/shepherd_artifacts

# One-shot check without downloading artifacts or logs
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo youtube/cobalt --pr 123 --no-download
```

### Supported CLI Flags & Exit Behavior
- `--repo <owner>/<name>`: Target GitHub repository (defaults to current repo).
- `--pr <num[,num...]>`: Single PR number or comma-separated PR list.
- `--watch`: Poll continuously at `--interval` seconds until green or failure.
- `--interval <sec>`: Poll interval in seconds (default: `30`).
- `--artifacts-dir` / `--download-dir <path>`: Destination directory for
  downloaded `test_results/` and `logs/` on failure (default: `artifacts`;
  nested under `pr_<num>/` when multiple PRs are monitored).
- `--no-download`: Disable automatic download of failure artifacts and logs.
- **Exit behavior under `--watch`**:
  - Exits `0` and prints `SUPER GREEN! Multipass authorized.` when all monitored
    PRs are green, mergeable, and have zero unresolved threads.
  - Exits `1` immediately and prints `ACTION REQUIRED: Triage failure immediately!`
    the moment any check or CI Shepherd report fails—restart `--watch` after
    the Issue Worker pushes a fix or triggers a rerun.
  - Emits `Open thread:`, `New thread:`, and `WARNING: ... merge conflicts`
    lines immediately on stdout when detected.

---

## 🥞 Stacked Pull Requests (`gh stack`)

When shepherding stacked PRs (a chain of dependent PRs, e.g.
`PR #1 (base: main) <- PR #2 <- PR #3`):
1. **Full-Stack Monitoring**: Pass all PR numbers in the stack to
   `shepherd_pr.py` (e.g., `--pr 101,102,103 --watch`).
2. **Bottom-Up Progression**: Stacked PR layers depend sequentially from
   bottom (base) to top. Fix and verify lower layers first (merging remains
   strictly user-only).
3. **Upstack Cascade & Synchronization**: When an Issue Worker modifies or
   rebases a lower layer, cascade changes upstack using
   `gh stack rebase --upstack` / `gh stack sync` and update PRs with
   `gh stack submit` (or `gh stack submit --auto`).
4. **Branch Namespace**: Stack branches pushed to `origin` follow the
   `origin:stack/<username>/<branch>` convention. Refer to the `gh-stack` skill
   or repository stack guidelines for full CLI syntax.

---

## 🏗️ 3-Tier Delegation Architecture

> [!IMPORTANT]
> **STRICT TIER ISOLATION & CONTEXT CONSERVATION**:
> PR shepherding uses three strictly separated roles so long-running watch
> loops and verbose CI logs never exhaust the Main Agent's context window.

### Tier 1: Main Agent (Caller / High-Level Orchestrator)
- **MUST NOT** run `shepherd_pr.py --watch` directly in its own context.
- **MUST NOT** read [resources/triage_agent.md][triage-agent-ref] directly.
- **MUST NOT** inspect raw CI logs, download artifacts, perform failure
  triage, or edit PR code itself.
- **Responsibilities**:
  1. Spawn a **Watch Coordinator** subagent (`Role: "pr-shepherd-watcher"`)
     passing the target repo, PR number(s), base branch, and any explicit
     single-use user instruction for GitHub comment replies (default: replies
     prohibited).
  2. Wait for the Watch Coordinator to report either
     `SUPER GREEN! Multipass authorized.` or an escalation (non-PR base/infra
     failure, context-limit handoff if needed, or request for single-use reply
     consent).
  3. Only read `shepherding_report.md` if summarizing current progress for the
     user or presenting an escalation report.
  4. Never merge the PR upon completion.

### Tier 2: Watch Coordinator Subagent (`Role: "pr-shepherd-watcher"`)
- **Responsibilities**:
  1. Initialize `shepherding_report.md` at the workspace root (using
     `shepherding_report.lock` + atomic replacement) with the structured
     **Ledger of Actions Taken**.
  2. Run `shepherd_pr.py --watch` (with failure artifacts downloaded to
     `artifacts/` or `--artifacts-dir`).
  3. **Active Interruption**: The moment **ANY** failure (`ACTION REQUIRED` /
     failed check), unresolved review comment (`Open thread:` / `New thread:`),
     or merge conflict (`WARNING: ... merge conflicts`) is detected—without
     waiting for the rest of the matrix—spawn an **Issue Worker** subagent
     (`Role: "pr-shepherd-fixer"`) in an isolated workspace
     (`Workspace: "branch"` or a WISP worktree).
  4. Provide the Issue Worker with the PR number(s), failing check/job/thread
     details, artifact directory path, and explicit comment-reply policy, and
     instruct it to read [resources/triage_agent.md][triage-agent-ref] first.
  5. Record dispatch/outcome updates in `shepherding_report.md`, then resume
     `shepherd_pr.py --watch` until `SUPER GREEN! Multipass authorized.` (exit
     `0`) or an escalation trigger occurs.
  6. **Clean Context Handoff**: If approaching context limits during long CI
     runs, ensure `shepherding_report.md` (and its Action Ledger) reflects the
     exact current state and hand off cleanly to a fresh watcher or report the
     handoff state to the Main Agent.

### Tier 3: Issue Worker Subagent (`Role: "pr-shepherd-fixer"`)
- **Responsibilities**:
  1. Read [resources/triage_agent.md][triage-agent-ref] **first** before
     inspecting logs or modifying code.
  2. Work strictly in an isolated workspace (`Workspace: "branch"` or WISP
     worktree) to prevent collisions.
  3. Triage failures using downloaded `shepherd_report.json` and step logs
     (or `gh run view --job <JOB_ID> --log-failed` on the early-failure fast
     path).
  4. Fix PR-caused failures, resolve merge conflicts, implement review comment
     code changes (respecting the single-use consent prohibition on GitHub
     replies), or execute the Flake / Infra / Base Drift protocol.
  5. Update the **Ledger of Actions Taken** in `shepherding_report.md`
     (protected by `shepherding_report.lock` + atomic replacement) with
     timestamp/iteration, target PR & check/thread, classification, action
     taken, and outcome/verification.
  6. Report the structured result back to the Watch Coordinator (`pr-shepherd-watcher`).

---

## 📋 Action Ledger (`shepherding_report.md` + `shepherding_report.lock`)

Both the Watch Coordinator and Issue Workers must maintain a structured
**Ledger of Actions Taken** in `shepherding_report.md` at the workspace root
(never committed to the PR branch). Every read-modify-write cycle **MUST**
acquire an exclusive lock on `shepherding_report.lock` (`flock -x` or
`fcntl.flock`), re-read the file after locking, and overwrite via atomic
replacement (`mv -f` / `os.replace`).

Each ledger entry must capture:
- **Timestamp / Iteration**
- **Target PR & Check / Thread**
- **Classification** (`PR-Caused` / `Flake` / `Infra` / `Base-Drift` /
  `Review-Comment` / `Merge-Conflict`)
- **Action Taken** (commit SHA, rerun run ID, rebase, reply)
- **Outcome / Verification**

[triage-agent-ref]: resources/triage_agent.md
