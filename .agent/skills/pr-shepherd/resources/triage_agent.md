# PR Shepherd CI Failure Triage Guide

This guide is for the PR Shepherd agent to triage, diagnose, and resolve CI
check failures, and address review comments. It contains the core invariants,
monitoring instructions, and detailed procedures for triage, fixing, and
replying to review comments.

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

To satisfy the completion invariant, **ALL** of the following conditions must
be met:
1. **100% of CI Checks Completed & Green**:
   `green_checks == total_checks > 0`, with 0 failed and 0 running/pending.
2. **Zero Unaddressed Comments**: All top-level PR comments and inline
   file/diff review threads are addressed, replied to in-thread, and resolved.
3. **No Pending Changes Requested**: All review decisions are `APPROVED` or have
   no outstanding `CHANGES_REQUESTED`.
4. **Verified Script Output**: The script outputs:
   ```text
   SUPER GREEN! Multipass authorized.
   ```

If any check is still running or any comment thread is unaddressed, you **MUST
CONTINUE MONITORING AND SHEPHERDING**. Do not stop or exit early.

### 🚫 STRICT PROHIBITION: DO NOT MERGE PULL REQUESTS

> [!CAUTION]
> **NEVER MERGE PRS**:
> You must **NEVER merge pull requests** or trigger PRs to be merged under any
> circumstances (e.g., via `gh pr merge`, GitHub GraphQL/REST API merge
> endpoints, or enabling auto-merge).
> - **Scope Boundary**: Your responsibility is strictly limited to monitoring,
>   triaging failures, pushing fixes, and resolving review comments until the
>   PR reaches **SUPER GREEN** status.
> - **No Autonomous Merging**: Even when all checks pass and the PR is approved
>   (`SUPER GREEN`), **DO NOT merge the PR**.
> - **User Delegation Only**: PR merging is strictly reserved for the user and
>   requires explicit, unambiguous user instructions in chat.

---

## Running the Shepherd Script

Run the `shepherd_pr.py` script as a background task to monitor the PR. It
tracks PR status check rollups and review comment threads, outputting minimal
text diffs and CI Shepherd report findings for each state change.

```bash
# Start watching the PR in the background
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER> --watch
```

When you begin monitoring, immediately initialize `shepherding_report.md` in
your workspace. When failures or comments are detected by the script, address
them immediately in your workspace and update the report.

---

## 🥞 Stacked Pull Requests (`gh stack`)

When shepherding stacked PRs (a chain of dependent PRs, e.g.
`PR #1 (base: main) <- PR #2 <- PR #3`):
1. **Full-Stack Monitoring**: You may pass all PR numbers in the stack to the
   script (e.g. `--pr 101,102,103 --watch`).
2. **Bottom-Up Progression**: Stacked PR layers depend sequentially from
   bottom (base) to top. Lower layers must reach Green first (merging is
   performed by the user).
3. **Upstack Cascade & Synchronization**: When you fix code or resolve
   comments on a lower layer, you must cascade changes upstack
   (`gh stack sync` or `gh stack submit --auto`) so all dependent PRs update
   their checks.
4. **Stack Reference**: For full CLI command syntax, refer to `gh-stack` CLI
   extension documentation or repository stack guidelines.

---

## 1. GitHub CLI (`gh`) Commands for Triage & Log Retrieval

For comprehensive `gh` CLI documentation and policies, refer to the
[`github-interactions` skill](SKILLS_DIR/github-interactions/SKILL.md).

```bash
# 1. Inspect PR status & CI Shepherd report findings directly:
python3 SKILLS_DIR/pr-shepherd/scripts/shepherd_pr.py \
  --repo <owner>/<repo> --pr <PR_NUMBER>

# 2. Download CI Shepherd JSON report artifacts (~500 bytes per target):
gh run download <RUN_ID> -p 'shepherd-report-*' -D /tmp/shepherd_reports \
  --repo <owner>/<repo>

# 3. Fetch failed step logs only (fast & token-efficient for builds):
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
>    PR (e.g., use `gh pr view <PR_NUMBER>` to inspect description & diffs).
> 2. **Determine if PR Caused Failure**: Investigate whether failure was
>    introduced or triggered by the PR's specific changes. Check if failure
>    touches modified files, changed symbols, or affected subsystems.
> 3. **Distinguish PR Failures vs. Flakes/Infra Issues**:
>    - **PR-Caused**: Compiler errors on modified code, unit test failures for
>      modified functionality, integration tests in affected modules, linter
>      failures, or merge conflicts. These are in-scope for fixing.
>    - **Flaky Tests / Infra Issues**: Network/HTTP timeouts, runner disk
>      exhaustion, external service failures, machine disconnects, or
>      pre-existing flaky tests unrelated to PR changes. These are strictly
>      **out of scope** for code fixes.
> 4. **No Code Fixes for Flakes/Infra**: If failure is determined to be a
>    flaky test or CI infrastructure issue, do NOT modify code or CI
>    configurations to fix it. Report failure details to orchestrator/user.

### 🌟 Primary Triage Source: CI Shepherd JSON Report

The Cobalt CI pipeline generates structured **CI Shepherd JSON reports**
(`shepherd_report.json`) uploaded as `shepherd-report-<platform>` artifacts
for each platform matrix target (e.g. `shepherd-report-android-arm64`,
`shepherd-report-linux-modular`, `shepherd-report-tvos`).

Always inspect the CI Shepherd JSON report **first** before pulling logs:

1. **Step 1: Check Pipeline Stage Status (`checks`)**:
   - Inspect `checks` map in `shepherd_report.json` (or script stdout).
   - If `"build": "failure"`, compilation failed. Skip test analysis and
     inspect compiler errors via `gh run view --job <JOB_ID> --log-failed`.
   - If `"on-host-test"`, `"web-tests"`, or `"on-device-test"` is `"failure"`,
     a test suite failed. Proceed to step 2.
2. **Step 2: Inspect Failing Tests (`test_failures`)**:
   - `shepherd_report.json` contains a structured map of `failing_tests` with
     exact test case name (`Suite.TestName`) and failure assertion message.
   - **Zero log grepping required**: You get failing test name and failure
     message directly without downloading megabytes of raw console logs!
3. **Step 3: Targeted Fallback (Only If Necessary)**:
   - Only fetch job logs (`gh run view --job <JOB_ID> --log-failed`) if you
     need deep stack traces that were truncated, or compiler errors.
   - Filter targeted signatures:
     - **Compiler Errors**: `grep -E ":[0-9]+:[0-9]+: (fatal )?error:"`
     - **Build Failures (Ninja/Siso)**: `grep -E "FAILED:|err:.*failed:"`
     - **Config Errors (GN)**: `grep "ERROR at "`
     - **Unit Tests (GTest)**: `grep -E "\[  FAILED  \]"`
     - **Crashes**: `grep -E "Segmentation fault|SIGSEGV|Traceback"`
4. **Step 4: Log Triage Findings**:
   - Update `shepherding_report.md` with platform, failing sub-job, failing
     test case name(s), root cause, and classification (`PR-Caused` vs
     `Flake/Infra`).

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
  - Do NOT attempt to fix CI runner, network, tooling, or infrastructure
    issues.
  - Do NOT attempt to fix pre-existing bugs or test failures present on the
    base branch.

### Handling Flaky Tests & Infrastructure Failures
If triage reveals that a failure is due to a flaky test or infrastructure issue
(not caused by the PR):
1. **Do NOT apply code fixes**: Do not attempt workarounds or edits in code.
2. **Transient verification**: You may rerun the failed job
   (`gh run rerun <RUN_ID> --failed --repo <owner>/<repo>`) once to check if it
   resolves a transient infra failure. Log rerun action and update status to
   `Rerunning` in `shepherding_report.md`.
3. **Report to User/Orchestrator**: If failure persists or is confirmed as a
   flake/infra issue, document structured report in `shepherding_report.md`
   under Flaky Tests & Infrastructure Failures section and notify orchestrator.

### Implementing PR Fixes
Once you have identified a PR-caused failure or requested change from review:
- **Implement fix**: Make necessary code changes.
- **Be Minimally Invasive**: Make smallest, most targeted changes possible.
- **Verify**: Build or run tests locally to ensure issue is resolved.
- **Push**: Commit and push the fix (adhering to git rules).
- **Update Report**: Update `shepherding_report.md` with applied fix, commit
  hash, and action timeline entry, setting check status to
  `Fix Applied / Verifying`.
- **Continue Monitoring**: `shepherd_pr.py` will detect new checks. Wait for
  them to pass. Only report back to main agent when PR is `SUPER GREEN`.

### Resolving Merge Conflicts

If `shepherd_pr.py` reports `WARNING: PR has merge conflicts!`, or if PR's
mergeability status is `CONFLICTING` / `DIRTY`:
*(Note: Merging/rebasing target base branch into PR branch to resolve conflicts
is permitted, but merging PR into target base branch is strictly forbidden.)*
1. **Identify base branch**: Determine target branch of PR (typically `main`).
2. **Fetch and Integrate Base Branch**:
   - **For Regular (Non-Stack) PRs**: Fetch from origin & merge (or rebase):
     ```bash
     git fetch origin
     git merge origin/main
     ```
   - **For Stacked PRs**: Use `gh stack sync` to fetch and rebase.
3. **Resolve Conflict Markers**:
   - Locate conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) in files.
   - Resolve conflicts carefully, keeping PR's goal in mind.
   - Verify codebase compiles and tests pass locally after resolving.
4. **Commit, Push, and Log Resolution**:
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
   - **Update Report**: Log conflict resolution in `shepherding_report.md`.

---

## 4. Addressing Review Comments

When addressing review comments:
1. Locate file and line indicated in report (`path:line`).
2. Implement requested changes adhering to repository's style guide.
3. Verify changes locally with tests/linters.
4. Reply in-thread via
   `gh api repos/<owner>/<repo>/pulls/<PR>/comments/<ID>/replies`.
5. **Update Report**: Mark thread resolved in `shepherding_report.md` and
   log reply.

---

## 5. Isolated Workspace & Temporary Artifacts Protocol

To maintain workspace hygiene and prevent collisions:
1. **Isolated Execution**: Perform file edits and builds inside isolated
   temporary branch or worktree (e.g. via `wisp` skill) if necessary.
2. **Scratch Files**: Store temporary downloaded logs inside temporary folder.
   Never leave untracked scratch files in shared root directories.
3. **Hermetic Commits / Pushes**: Commit with proper Git trailers and update PR
   branch after obtaining necessary user approvals. Never use broad staging
   commands like `git add .` or `git add -A`.
4. **Report File Accessibility**: Maintain `shepherding_report.md` at workspace
   root so orchestrator agent and user can view real-time status at any time.
   Do not commit `shepherding_report.md` to PR branch.

---

## 6. Stacked PRs Protocol (`gh stack`)

When modifying code in a stacked PR chain:
1. **Branch Naming & Remotes**: Stack branches pushed to `origin` must strictly
   follow `origin:stack/<username>/<branch>` namespace.
2. **Upstack Rebase & Sync**: If you modify a commit or layer below dependent
   PRs:
   - Rebase dependent layers upward: `gh stack rebase --upstack`
   - Re-submit stack to GitHub: `gh stack submit`
3. **Verification Across Layers**: Ensure all dependent layers in stack compile
   and pass local test suites before completing task.

---

## 7. Shepherding Status Report (`shepherding_report.md`)

The shepherd subagents are responsible for creating and progressively updating
`shepherding_report.md` in workspace root throughout the run.

### 🔒 Concurrency Control & Synchronized Report Access

Because multiple subagents and concurrent tasks write to shared
`shepherding_report.md` file, all report reading and writing **MUST** adhere
to synchronization rules:

1. **Advisory File Locking (`shepherding_report.lock`)**:
   - All read-modify-write cycles on `shepherding_report.md` **MUST** acquire
     an exclusive lock on `shepherding_report.lock` (e.g., using `flock -x`
     in shell or `fcntl.flock` in Python).
2. **Re-Read Before Write (No Stale Overwrites)**:
   - Always read latest content of `shepherding_report.md` *after* acquiring
     exclusive lock before computing modifications. Never overwrite report
     with stale state held in memory.
3. **Atomic File Replacement**:
   - Always write updated content to temporary file (e.g.,
     `shepherding_report.md.tmp.$$` or `shepherding_report.md.tmp.<pid>`) on
     same filesystem, and use atomic move (`mv -f` / `os.replace`) to
     overwrite `shepherding_report.md`.
4. **Synchronized Update Command Patterns**:
   - **Shell `flock` subshell pattern**:
     ```bash
     (
       flock -x 200
       # 1. Read latest shepherding_report.md
       # 2. Modify target sections or append action line to tmp file
       # 3. Atomically overwrite:
       mv -f shepherding_report.md.tmp.$$ shepherding_report.md
     ) 200>shepherding_report.lock
     ```
   - **Python inline synchronization pattern**:
     ```python
     import fcntl, os

     with open("shepherding_report.lock", "w") as lock_f:
         fcntl.flock(lock_f, fcntl.LOCK_EX)
         with open("shepherding_report.md", "r") as rf:
             content = rf.read()
         # Apply section updates / modifications
         tmp_file = f"shepherding_report.md.tmp.{os.getpid()}"
         with open(tmp_file, "w") as wf:
             wf.write(content)
         os.replace(tmp_file, "shepherding_report.md")
     ```

### Report Update Lifecycle & Triggers

You must update `shepherding_report.md` in-place whenever any event occurs:

1. **Initialization**: Create initial report immediately upon taking ownership.
2. **Triage & Diagnosis**: When check failure is investigated, update status
   to `Investigating` and record root cause.
3. **Fix Applied / Conflicts Resolved**: When code fixes are committed or merge
   conflicts resolved, update status to `Fix Applied / Verifying`.
4. **Comments Resolved**: When inline review comment is resolved and replied
   to, update status to `Resolved`.
5. **Flakes & Infra Failures**: Document job details and evidence in Flakes.
6. **Rerun Triggered**: Update status to `Rerunning` in checks table.
7. **SUPER GREEN Completion**: When all checks pass and script outputs
   `SUPER GREEN!`, update overall status to `SUPER GREEN` and finalize.

### Standard Report Template

Use the following Markdown structure when creating and updating
`shepherding_report.md`:

````markdown
# PR Shepherding Report

## 📊 Overview
- **Monitored PR(s)**: [#<PR_NUMBER> (<PR_TITLE>)](<PR_URL>)
- **Repository**: `<owner>/<repo>`
- **Branch**: `<head_branch>` -> `<base_branch>`
- **Overall Status**: `IN PROGRESS` | `SUPER GREEN` | `BLOCKED (Infra)`
- **CI Checks**: `<passed>/<total> Pass` (`<failed> Fail`, `<pending> Pending`)
- **Review Comments**: `<unresolved> Unresolved` (`<resolved> Resolved`)
- **Flakes / Infrastructure Issues**: `<flake_count>`

---

## 2. CI Checks Status & Triage
| Platform / Check | Status | Failing Job / Tests | Classification | Action |
| :--- | :--- | :--- | :--- | :--- |
| `<platform>` | `FAILED` | `<failing job/test>` | `PR-Caused` | `<fix>` |

---

## 3. Review Comments
| Author | Location | Comment Snippet | Status | Resolution |
| :--- | :--- | :--- | :--- | :--- |
| `@<author>` | `<path>` | `<snippet>` | `Resolved` | `<fix>` |

---

## 4. Flaky Tests & Infrastructure Failures
*(Document any non-PR failures here with concrete evidence)*
- **Job Name**: `<job_name>` ([Run Details](<run_url>))
  - **Error Signature**: `<regex pattern or failure line>`
  - **Log Snippet**:
    ```text
    <log snippet showing runner / timeout / network failure>
    ```
  - **Evidence**: <Explanation of why failure is unrelated to PR changes>
  - **Action Taken**: <Rerun triggered once / Escalated to orchestrator>

---

## 5. Action Log & Timeline
- `[YYYY-MM-DD HH:MM:SS]` **Initialized**: Started shepherding PR #<PR_NUMBER>.
- `[YYYY-MM-DD HH:MM:SS]` **Triage**: Diagnosed failure in `<platform>`.
- `[YYYY-MM-DD HH:MM:SS]` **Fix**: Pushed commit `<hash>` fixing `<issue>`.
- `[YYYY-MM-DD HH:MM:SS]` **Review**: Replied to comment by `@<author>`.
- `[YYYY-MM-DD HH:MM:SS]` **Rerun**: Triggered rerun in `<job_name>`.
- `[YYYY-MM-DD HH:MM:SS]` **Completed**: PR reached `SUPER GREEN` status!
````
