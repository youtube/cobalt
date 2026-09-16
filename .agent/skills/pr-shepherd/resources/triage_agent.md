# PR Shepherd CI Failure Triage Guide

This guide is for the PR Shepherd agent to triage, diagnose, and resolve CI check failures, and address review comments. It contains the core invariants, monitoring instructions, and detailed procedures for triage, fixing, and replying to review comments.

> [!IMPORTANT]
> **SCOPE LIMITATION: FIXES MUST BE CAUSED BY THE PR**:
> Fixes by the PR Shepherd are **strictly limited to issues caused by the PR**.
> - **PR-Caused Failures**: Build errors, compile failures, broken tests,
>   linter issues, merge conflicts, and review comments caused by the PR diff
>   **must be fixed**.
> - **Flaky Tests & Infrastructure Issues**: CI runner issues, network errors,
>   timeouts, infrastructure outages, or unrelated flaky tests **must NOT be
>   attempted to be fixed** by the shepherd. Do not modify code or CI configs
>   to attempt fixing them. Instead, report them clearly to the
>   orchestrator/user.

## 🛑 MANDATORY COMPLETION GATE: GREEN STATUS

> [!IMPORTANT]
> **STRICT COMPLETION INVARIANT**:
> You **CANNOT FINISH** your task or declare success until the PR has
> achieved **Green** status (or any blocking non-PR failures have been
> reported).

To satisfy the completion invariant, **ALL** of the following conditions must be met:
1. **100% of CI Checks Completed & Green**: `green_checks == total_checks > 0`, with 0 failed and 0 running/pending.
2. **Zero Unaddressed Comments**: All top-level PR comments and inline file/diff review threads are addressed, replied to in-thread, and resolved.
3. **No Pending Changes Requested**: All review decisions are `APPROVED` or have no outstanding `CHANGES_REQUESTED`.
4. **Verified Script Output**: The script outputs:
   ```text
   SUPER GREEN! Multipass authorized.
   ```

If any check is still running or any comment thread is unaddressed, you **MUST CONTINUE
MONITORING AND SHEPHERDING**. Do not stop or exit early.

### 🚫 STRICT PROHIBITION: DO NOT MERGE PULL REQUESTS

> [!CAUTION]
> **NEVER MERGE PRS**:
> You must **NEVER merge pull requests** or trigger PRs to be merged under any circumstances (e.g., via `gh pr merge`, GitHub GraphQL/REST API merge endpoints, or enabling auto-merge).
> - **Scope Boundary**: Your responsibility is strictly limited to monitoring, triaging failures, pushing fixes, and resolving review comments until the PR reaches **SUPER GREEN** status.
> - **No Autonomous Merging**: Even when all checks pass and the PR is approved (`SUPER GREEN`), **DO NOT merge the PR**.
> - **User Delegation Only**: PR merging is strictly reserved for the user and requires explicit, unambiguous user instructions in chat.

---

## Running the Shepherd Script

## Running the Shepherd Script

Run the `shepherd_pr.py` script as a background task to monitor the PR. It tracks PR status check rollups and review comment threads, outputting minimal text diffs and CI Shepherd report findings for each state change.

```bash
# Start watching the PR in the background with CI Shepherd reports enabled
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --watch --ci-report
```

When you begin monitoring, immediately initialize `shepherding_report.md` in your workspace. When failures or comments are detected by the script, address them immediately in your workspace and update the report.

---

## 🥞 Stacked Pull Requests (`gh stack`)

When shepherding stacked PRs (a chain of dependent PRs, e.g. `PR #1 (base: main) <- PR #2 <- PR #3`):
1. **Full-Stack Monitoring**: You may pass all PR numbers in the stack to the script (e.g. `--pr 101,102,103 --watch --ci-report`).
2. **Bottom-Up Progression**: Stacked PR layers depend sequentially from bottom (base) to top. Lower layers must reach Green first (merging is performed by the user).
3. **Upstack Cascade & Synchronization**: When you fix code or resolve comments on a lower layer, you must cascade changes upstack (`gh stack sync` or `gh stack submit --auto`) so all dependent PRs update their checks.
4. **Stack Reference**: For full CLI command syntax, refer to `gh-stack` CLI extension documentation or repository stack guidelines.

---

## 1. GitHub CLI (`gh`) Commands for Triage & Log Retrieval

For comprehensive `gh` CLI documentation and policies, refer to the [`github-interactions` skill](SKILLS_DIR/github-interactions/SKILL.md).

```bash
# 1. Inspect CI Shepherd reports directly via shepherd_pr.py (instant & structured):
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py --repo <owner>/<repo> --pr <PR_NUMBER> --ci-report

# 2. Download CI Shepherd JSON report artifacts (lightweight, ~500 bytes per platform):
gh run download <RUN_ID> -p 'shepherd-report-*' -D /tmp/shepherd_reports \
  --repo <owner>/<repo>

# 3. Fetch failed step logs only (fast & token-efficient, use if build failed):
gh run view --job <JOB_ID> --log-failed --repo <owner>/<repo>

# 4. Fetch full job log (only if step log is incomplete):
gh run view --job <JOB_ID> --log --repo <owner>/<repo>

# 5. View overall workflow run status and job list:
gh run view <RUN_ID> --repo <owner>/<repo>

# 6. Rerun failed jobs in a workflow run (for transient infra flakes):
gh run rerun <RUN_ID> --failed --repo <owner>/<repo>

# 7. Reply to a review comment in-thread:
gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/<COMMENT_ID>/replies \
  --field body="Done. <brief explanation>"
```

---

## 2. Efficient Log Triage & Root-Cause Extraction

> [!IMPORTANT]
> **PR Context & Investigating Root Cause**
> 1. **Understand the PR**: Always make yourself aware of the context of the
>    PR (e.g., use `gh pr view <PR_NUMBER>` to inspect the description and
>    changed files). Read the git log of the PR branch and the associated diffs.
> 2. **Determine if the PR Caused the Failure**: Investigate whether the
>    failure was introduced or triggered by the PR's specific changes. Check if
>    the failure touches modified files, changed symbols, or affected
>    subsystems.
> 3. **Distinguish PR Failures vs. Flakes/Infra Issues**:
>    - **PR-Caused**: Compiler errors on modified code, unit test failures for
>      modified functionality, integration tests in affected modules, linter
>      failures, or merge conflicts. These are in-scope for fixing.
>    - **Flaky Tests / Infra Issues**: Network/HTTP timeouts, runner disk
>      exhaustion, external service failures, machine disconnects, or
>      pre-existing flaky tests unrelated to PR changes. These are strictly
>      **out of scope** for code fixes.
> 4. **No Code Fixes for Flakes/Infra**: If the failure is determined to be a
>    flaky test or CI infrastructure issue, do NOT modify code or CI
>    configurations to fix it. Report the failure details and root cause to the
>    orchestrator/user.

### 🌟 Primary Triage Source: CI Shepherd JSON Report

The Cobalt CI pipeline generates structured **CI Shepherd JSON reports** (`shepherd_report.json`) uploaded as `shepherd-report-<platform>` artifacts for each platform matrix target (e.g. `shepherd-report-android-arm64`, `shepherd-report-linux-modular`, `shepherd-report-tvos`).

Always inspect the CI Shepherd JSON report **first** before pulling logs or downloading test results:

1. **Step 1: Check Pipeline Stage Status (`checks`)**:
   - Inspect the `checks` map in `shepherd_report.json` (or in `shepherd_pr.py --ci-report` output).
   - If `"build": "failure"`, the failure occurred during compilation. Skip test analysis and inspect compiler errors via `gh run view --job <JOB_ID> --log-failed`.
   - If `"on-host-test"`, `"web-tests"`, or `"on-device-test"` is `"failure"`, a test suite failed. Proceed to step 2.
2. **Step 2: Inspect Failing Tests (`test_failures`)**:
   - `shepherd_report.json` contains a structured map of `failing_tests` with the exact test case name (`Suite.TestName`) and the failure assertion message.
   - **Zero log grepping required**: You get the failing test name and failure message directly without downloading megabytes of raw console logs or test XMLs!
3. **Step 3: Targeted Fallback (Only If Necessary)**:
   - Only fetch job logs (`gh run view --job <JOB_ID> --log-failed`) if you need deep stack traces that were truncated, or to inspect compiler errors.
   - Filter targeted signatures:
     - **Compiler Errors**: `grep -E ":[0-9]+:[0-9]+: (fatal )?error:"`
     - **Build Failures (Ninja/Siso)**: `grep -E "FAILED:|err:.*failed:"`
     - **Config Errors (GN)**: `grep "ERROR at "`
     - **Unit Tests (GTest)**: `grep -E "\[  FAILED  \]"`
     - **Crashes / Exceptions**: `grep -E "Segmentation fault|SIGSEGV|Traceback \(most recent call last\):"`
4. **Step 4: Log Triage Findings**:
   - Update `shepherding_report.md` with the platform, failing sub-job, failing test case name(s), root cause, and classification (`PR-Caused` vs. `Flake/Infra`).

---

## 3. Resolving the Root Cause

### Permitted Fix Scope vs. Reporting
- **Permitted Fixes (PR-Caused Only)**:
  - Fix compiler, build, and linter errors introduced by the PR.
  - Fix test failures or regressions caused by the PR's changes.
  - Resolve merge conflicts with the base branch.
  - Implement requested changes from review comments.
- **Prohibited Fixes (Report to User)**:
  - Do NOT attempt to fix flaky tests unrelated to the PR.
  - Do NOT attempt to fix CI runner, network, tooling, or infrastructure issues.
  - Do NOT attempt to fix pre-existing bugs or test failures present on the
    base branch.

### Handling Flaky Tests & Infrastructure Failures
If triage reveals that a failure is due to a flaky test or infrastructure issue
(not caused by the PR):
1. **Do NOT apply code fixes**: Do not attempt workarounds or edits in the
   codebase.
2. **Transient verification**: You may rerun the failed job
   (e.g. `gh run rerun <RUN_ID> --failed --repo <owner>/<repo>`) once to check
   if it resolves a transient infra failure. Log the rerun action and update
   status to `Rerunning` in `shepherding_report.md`.
3. **Report to User/Orchestrator**: If the failure persists or is confirmed as a
   flake/infra issue, document the structured report in `shepherding_report.md`
   under the Flaky Tests & Infrastructure Failures section (including job name,
   run URL, error signature, snippet, and evidence) and notify the orchestrator.

### Implementing PR Fixes
Once you have identified a PR-caused failure or the requested change from a
review comment:
- **Implement the fix**: Make the necessary code changes.
- **Be Minimally Invasive**: When fixing issues with a PR, make the smallest,
  most targeted changes possible. Avoid unnecessary refactoring or scope creep.
- **Verify**: Build or run tests locally to ensure the issue is resolved.
- **Push**: Commit and push the fix (adhering to git rules).
- **Update Report**: Update `shepherding_report.md` with the applied fix, commit
  hash, and action timeline entry, setting the check status to
  `Fix Applied / Verifying`.
- **Continue Monitoring**: The `shepherd_pr.py` script will detect the new
  checks. Wait for them to pass. Only report back to the main agent when the PR
  is `SUPER GREEN`.

### Resolving Merge Conflicts

If the `shepherd_pr.py` script reports `WARNING: PR has merge conflicts!`, or if the PR's mergeability status is `CONFLICTING` / `DIRTY`:
*(Note: Merging/rebasing the target base branch into the PR branch to resolve conflicts is permitted, but merging the PR into the target base branch is strictly forbidden.)*
1. **Identify the base branch**: Determine the target branch of the PR (typically `main`).
2. **Fetch and Integrate Base Branch**:
   - **For Regular (Non-Stack) PRs**: Fetch from origin and merge (or rebase) the base branch into your PR branch:
     ```bash
     git fetch origin
     git merge origin/main
     ```
   - **For Stacked PRs**: Use `gh stack sync` to fetch and rebase.
3. **Resolve Conflict Markers**:
   - Locate conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) in the files.
   - Resolve conflicts carefully, keeping the PR's goal in mind.
   - Verify the codebase compiles and tests pass locally after resolving conflicts.
4. **Commit, Push, and Log the Resolution**:
   - **For Regular PRs**:
     ```bash
     git add <resolved_files>
     git commit --no-edit
     git push
     ```
   - **For Stacked PRs (branch prefix stack/)**:
     ```bash
     git add <resolved_files>
     gh stack rebase --continue
     gh stack submit --auto
     ```
   - **Update Report**: Log the conflict resolution in `shepherding_report.md`'s action log.

---

## 4. Addressing Review Comments

When addressing review comments:
1. Locate the file and line indicated in the report (`path:line`).
2. Implement the requested changes adhering to the repository's style guide.
3. Verify changes locally with tests/linters.
4. Reply in-thread using `gh api repos/<owner>/<repo>/pulls/<PR_NUMBER>/comments/<COMMENT_ID>/replies`.
5. **Update Report**: Mark the thread as resolved in `shepherding_report.md` and log the reply in the action timeline.

---

## 5. Isolated Workspace & Temporary Artifacts Protocol

To maintain workspace hygiene and prevent collisions:
1. **Isolated Execution**: Consider performing all file edits, script executions, and local builds inside an isolated temporary branch or worktree (e.g. via `wisp` skill) if necessary.
2. **Scratch Files**: Store any temporary downloaded log dumps or test result artifacts inside your temporary folder. Never leave untracked scratch files in shared root directories.
3. **Hermetic Commits / Pushes**: Once code changes or fixes are tested and verified, commit with proper Git trailers and update the PR branch (after obtaining the necessary user approvals). Never use broad staging commands like `git add .` or `git add -A` that could inadvertently stage temporary files or `shepherding_report.md`.
4. **Report File Accessibility**: Maintain `shepherding_report.md` at the workspace root so that the orchestrator agent and user can view real-time status at any time. Do not commit `shepherding_report.md` to the PR branch.

---

## 6. Stacked PRs Protocol (`gh stack`)

When modifying code in a stacked PR chain:
1. **Branch Naming & Remotes**: Stack branches pushed to `origin` must strictly follow the `origin:stack/<username>/<branch>` namespace per repository stack guidelines.
2. **Upstack Rebase & Sync**: If you modify a commit or layer below other dependent PRs in the stack:
   - Rebase dependent layers upward: `gh stack rebase --upstack`
   - Re-submit stack to GitHub: `gh stack submit`
3. **Verification Across Layers**: Ensure all dependent layers in the stack compile and pass local test suites before completing the task.

---

## 7. Shepherding Status Report (`shepherding_report.md`)

The shepherd subagents are responsible for creating and progressively updating `shepherding_report.md` in the workspace root throughout the run. The orchestrator agent relies on this file to report progress to the user.

### 🔒 Concurrency Control & Synchronized Report Access

Because multiple subagents and concurrent tasks write to the shared `shepherding_report.md` file, all report reading and writing **MUST** adhere to the following synchronization rules:

1. **Advisory File Locking (`shepherding_report.lock`)**:
   - All read-modify-write cycles and updates on `shepherding_report.md` **MUST** acquire an exclusive lock on `shepherding_report.lock` (e.g., using `flock -x` in shell or `fcntl.flock` in Python).
2. **Re-Read Before Write (No Stale Overwrites)**:
   - Always read the latest content of `shepherding_report.md` *after* acquiring the exclusive lock before computing or applying modifications. Never overwrite the report with stale state held in memory.
3. **Atomic File Replacement**:
   - Always write updated content to a temporary file (e.g., `shepherding_report.md.tmp.$$` or `shepherding_report.md.tmp.<pid>`) on the same filesystem, and use atomic move (`mv -f` / `os.replace`) to overwrite `shepherding_report.md`. This ensures readers never see partial or truncated file states.
4. **Synchronized Update Command Patterns**:
   - **Shell `flock` subshell pattern**:
     ```bash
     (
       flock -x 200
       # 1. Read latest shepherding_report.md
       # 2. Modify target sections or append action line to shepherding_report.md.tmp.$$
       # 3. Atomically overwrite:
       mv -f shepherding_report.md.tmp.$$ shepherding_report.md
     ) 200>shepherding_report.lock
     ```
   - **Python inline synchronization pattern**:
     ```python
     import fcntl, os

     with open("shepherding_report.lock", "w") as lock_f:
         fcntl.flock(lock_f, fcntl.LOCK_EX)
         # Read current shepherding_report.md
         with open("shepherding_report.md", "r") as rf:
             content = rf.read()
         # Apply section updates / modifications to content
         # ...
         # Atomically write
         tmp_file = f"shepherding_report.md.tmp.{os.getpid()}"
         with open(tmp_file, "w") as wf:
             wf.write(content)
         os.replace(tmp_file, "shepherding_report.md")
     ```

### Report Update Lifecycle & Triggers

You must update `shepherding_report.md` in-place whenever any of the following events occur:

1. **Initialization**: Create the initial report immediately upon taking ownership of the PR, capturing the initial checks rollup, open review comments, and status.
2. **Triage & Diagnosis**: When a check failure is investigated, update its status to `Investigating` and record the root cause and error signature.
3. **Fix Applied / Conflicts Resolved**: When code fixes are committed and pushed or merge conflicts are resolved, update the status to `Fix Applied / Verifying` and record the commit hash and changed files in the action log.
4. **Comments Resolved**: When an inline review comment is resolved and replied to in-thread, update its status to `Resolved` and record the reply details.
5. **Flakes & Infra Failures**: When a non-PR flake or infrastructure failure is identified, document the job details, error signature, log snippet, and evidence in the Flakes & Infrastructure section.
6. **Rerun Triggered**: When a transient failed job is rerun, update its status to `Rerunning` in the checks table and log the rerun action.
7. **SUPER GREEN Completion**: When all checks pass and the script outputs `SUPER GREEN! Multipass authorized.`, update the overall status to `SUPER GREEN` and finalize the report. Do not merge the PR (merging is strictly reserved for the user).

### Standard Report Template

Use the following Markdown structure when creating and updating `shepherding_report.md`:

````markdown
# PR Shepherding Report

## 📊 Overview
- **Monitored PR(s)**: [#<PR_NUMBER> (<PR_TITLE>)](<PR_URL>)
- **Repository**: `<owner>/<repo>`
- **Branch**: `<head_branch>` -> `<base_branch>`
- **Overall Status**: `IN PROGRESS` | `SUPER GREEN` | `BLOCKED (Non-PR / Infra Issue)`
- **CI Checks**: `<passed_count>/<total_count> Passing` (`<failed_count> Failed`, `<pending_count> Pending`)
- **Review Comments**: `<unresolved_count> Unresolved Threads` (`<resolved_count> Resolved`)
- **Flakes / Infrastructure Issues**: `<flake_count>`

---

## 🚦 CI Checks Status & Triage
| Platform / Check | Status | Failing Job / Tests (CI Shepherd Report) | Classification | Action / Resolution |
| :--- | :--- | :--- | :--- | :--- |
| `<platform / job>` | `FAILED` / `PASSED` / `PENDING` / `RERUNNING` | `<failing sub-job (e.g. build) or failing test cases (e.g. Suite.Test)>` | `PR-Caused` / `Flake/Infra` / `N/A` | `<commit hash / rerun / in progress>` |

---

## 💬 Review Comments
| Author | Location | Comment Snippet | Status | Resolution / In-Thread Reply |
| :--- | :--- | :--- | :--- | :--- |
| `@<author>` | `<path>:<line>` | `<snippet of comment>` | `Resolved` / `In Progress` | `<brief description of fix and in-thread reply>` |

---

## ⚠️ Flaky Tests & Infrastructure Failures
*(Document any non-PR failures here with concrete evidence for the orchestrator and user)*
- **Job Name**: `<job_name>` ([Run Details](<run_url>))
  - **Error Signature**: `<regex pattern or failure line>`
  - **Log Snippet**:
    ```text
    <log snippet showing runner / timeout / network failure>
    ```
  - **Evidence**: <Explanation of why the failure is unrelated to PR changes>
  - **Action Taken**: <Rerun triggered once / Escalated to orchestrator>

---

## 📝 Action Log & Timeline
- `[YYYY-MM-DD HH:MM:SS]` **Initialized**: Started shepherding PR #<PR_NUMBER>.
- `[YYYY-MM-DD HH:MM:SS]` **Triage**: Diagnosed failure in `<platform>` via CI Shepherd report (`<failing_job>` / `<failing_test>`).
- `[YYYY-MM-DD HH:MM:SS]` **Fix**: Pushed commit `<hash>` fixing `<issue>`.
- `[YYYY-MM-DD HH:MM:SS]` **Review**: Replied to comment by `@<author>` on `<path>:<line>`.
- `[YYYY-MM-DD HH:MM:SS]` **Rerun**: Triggered rerun for transient failure in `<job_name>`.
- `[YYYY-MM-DD HH:MM:SS]` **Completed**: PR reached `SUPER GREEN` status!
````
