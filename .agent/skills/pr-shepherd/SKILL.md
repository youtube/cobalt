---
name: pr-shepherd
description: >-
  Shepherds PRs to green. The agent monitors status checks and review comments,
  uses the CI Shepherd JSON report to triage failures, and addresses review
  comments to achieve All Green.
---

# Pull Request Shepherd

The **PR Shepherd** skill autonomously monitors Pull Requests until all
review comments are addressed, all CI checks complete, and the PR reaches
**All Green**. It leverages the **CI Shepherd JSON report**
(`shepherd_report.json`) to rapidly pinpoint failing platforms, jobs, and
test cases without downloading massive logs.

---

## 🛑 MANDATORY COMPLETION GATE: ALL GREEN

> [!IMPORTANT]
> **STRICT COMPLETION INVARIANT**:
> The agent **CANNOT FINISH** its task or declare success until the PR has
> achieved **All Green** status.

To satisfy the completion invariant, **ALL** of the following conditions must
be met:
1. **100% of CI Checks Completed & Green**:
   `green_checks == total_checks > 0`, with 0 failed and 0 running/pending.
2. **Zero Unaddressed Comments**: All top-level PR comments and inline
   file/diff review threads are addressed with code fixes (and replied to
   in-thread when explicitly instructed by the user).
3. **No Pending Changes Requested**: All review decisions are `APPROVED` or have
   no outstanding `CHANGES_REQUESTED`.
4. **Verified Script Output**: The script output indicating success across all
   monitored PRs:
   ```text
   SUPER GREEN! Multipass authorized.
   ```

If any check is still running or any comment thread is unaddressed, the agent
**MUST CONTINUE MONITORING AND SHEPHERDING**. Do not stop or exit early.

### ⚠️ Scope of Fixes: PR-Caused Issues vs. Flakes / Infrastructure

> [!IMPORTANT]
> **FIX SCOPE LIMITATION**:
> Fixes by the PR Shepherd are **strictly limited to issues caused by the PR**.
> - **PR-Caused Issues (Fix)**: The shepherd fixes compile errors, test
>   failures, linter issues, merge conflicts, or regressions introduced by the
>   PR, and implements review feedback.
> - **Flaky Failures & Infrastructure Issues**:
>   The shepherd must **NOT** attempt to fix flaky tests, runner timeouts,
>   network failures, or CI infrastructure outages by altering code or CI
>   configurations. Instead, the shepherd must diagnose the issue, verify it is
>   not caused by the PR, and rerun jobs that are affected. Only if retries are
>   not effective should issues be reported to the user.

### 🚫 STRICT PROHIBITION: DO NOT MERGE PULL REQUESTS

> [!CAUTION]
> **NEVER MERGE PRS**:
> The PR Shepherd skill and all associated agents **MUST NOT** merge pull
> requests or trigger PRs to be merged under any circumstances (e.g., via
> `gh pr merge`, GitHub GraphQL/REST API merge endpoints, or auto-merge).
> - **Completion Boundary**: The shepherd's role is strictly limited to
>   monitoring, triaging, resolving comments, and fixing PR-caused failures
>   until the PR reaches **All Green** / **SUPER GREEN** status.
> - **No Autonomous Merging**: Even when a PR is fully approved and all checks
>   pass, the agent **MUST NOT** merge the PR.
> - **User Delegation Only**: PR merging is strictly reserved for the user
>   and requires explicit, unambiguous user instructions in chat.

### 🚫 STRICT PROHIBITION: PREVENT AUTOMATIC COMMENTING ON GITHUB

> [!CAUTION]
> **NO AUTOMATIC COMMENTS OR REPLIES**:
> The PR Shepherd skill and all associated agents **MUST NOT** post comments or
> replies on GitHub PRs autonomously without explicit user instruction.
> - **No Autonomous Commenting**: Never post PR comments, inline review replies,
>   or issue comments autonomously without explicit user instruction in chat.
> - **Single-Use Consent**: Any consent or instruction granted by the user to
>   post a comment or reply is strictly single-use and non-persistent. Consent
>   expires immediately after being used once. Mandatory re-approval is
>   required for subsequent comments.
> - **In-Thread Replies When Instructed**: When explicitly instructed by the
>   user to respond to review comments, always reply directly in-thread
>   (`gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/`
>   `<COMMENT_ID>/replies`).

---

## Running the Shepherd Script

The script `shepherd_pr.py` tracks PR status check rollups and review comment
threads. It streams real-time updates directly to the agent, tracking state,
fetching CI Shepherd JSON reports, and emitting changes over time.

```bash
# Specific PR in a specific repository
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo youtube/cobalt --pr 123 --watch

# Multiple PRs / Stacked PRs (all must be green for the all_green event)
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --pr 101,102,103 --watch --interval 45
```

---

## Orchestrator Agent Workflow

1. **Strict Orchestration**:
   - The main agent **MUST** delegate **ALL** PR-related work to subagents.
     Triage, log analysis, code changes, and status report edits are strictly
     forbidden for the main agent in its orchestrator capacity. It acts solely
     as a high-level orchestrator.
2. **Assess Status**:
   - Launch `python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py`
     with `--repo <owner>/<repo> --pr <PR_NUMBER>` to get the initial state and
     any platform-specific CI Shepherd failure reports.
3. **Delegate Work to Shepherd Subagents in an Isolated Workspace**:
   - **Invoke the Subagent**: Invoke a single subagent to use a workspace
     isolation skill, or investigate options if one is not available:
     - Set the `Role` of the subagent to `pr-shepherd-worker`.
     - Instruct the subagent to take ownership of the PR(s), monitor status,
       and handle all PR-caused issues required to make them green. This
       includes resolving merge conflicts, addressing review comments, and
       triaging and fixing or rerunning CI/check failures.
     - Instruct the subagent that fixes are strictly limited to PR-caused
       issues; flaky failures or infrastructure issues must not be attempted to
       be fixed by code changes, but rather diagnosed, rerun if transient, and
       reported.
     - Direct the subagent to read the shepherding guide
       [triage_agent.md][triage-agent-ref] before making changes, using the
       CI Shepherd JSON report as the primary failure diagnosis source.
   - **Monitor Subagent Progress**: Allow the subagent to work autonomously
     until it completes all fixes and updates. Do not perform any triage
     yourself.
4. **User Status Inquiries & Flakes/Infra Issues**:
   - If the user requests the current status or progress report, the main agent
     can read `shepherding_report.md` to report the latest status directly to
     the user.
   - If the worker identifies a flaky test or CI infrastructure issue (not
     caused by the PR), the orchestrator **MUST** report this failure, root
     cause diagnosis, and recommendations directly to the user.
5. **Verify All Green Status & Complete (DO NOT MERGE)**:
   - Confirm output displays:
     ```text
     SUPER GREEN! Multipass authorized.
     ```
   - Verify the results yourself to ensure PR has reached SUPER GREEN status.
   - Report the All Green status directly to the user.
   - **DO NOT MERGE**: Never run `gh pr merge`, trigger GitHub API merge
     endpoints, or enable auto-merge. PR merging is strictly reserved for the
     user.
   - Only then complete the task.

[triage-agent-ref]: resources/triage_agent.md
