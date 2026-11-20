# PR Shepherd Issue Worker Guide (`pr-shepherd-fixer`)

This guide is for the **Issue Worker** (`pr-shepherd-fixer`) subagent. You are
dispatched by the Watch Coordinator (`pr-shepherd-watcher`) in an isolated
workspace (`Workspace: "branch"` or a WISP worktree) to triage and resolve a
specific CI failure, unresolved review comment code change, or merge conflict.

---

## ⚖️ Scope Limitation & Strict Prohibitions

> [!IMPORTANT]
> **SCOPE LIMITATION & NO AUTONOMOUS TEST FILTERING**:
> Fixes by the PR Shepherd are **strictly limited to issues caused by the PR**.
> - **PR-Caused Failures (Must Fix)**: Build errors, compile failures, broken
>   tests caused by the PR diff, linter issues, merge conflicts, and clear code
>   changes requested in review comments.
> - **No Autonomous Test Filtering**: You must **NEVER** autonomously add
>   failing or flaky tests to test filter manifests (e.g., `*_filter.json`,
>   `filter.txt`), disable tests, or add test skip annotations, and **never in
>   a PR with an unrelated feature**.
> - **Flaky Tests & Infrastructure Issues**: CI runner issues, network errors,
>   timeouts, infrastructure outages, or unrelated pre-existing/base-branch
>   failures **must NOT be fixed** via product code or CI config workarounds.
>   Follow the 4-step Flake / Infra / Base Drift protocol in Section 2.

### Strict Prohibitions (Public-Repo Safe Policies)

> [!CAUTION]
> **NEVER MERGE PRS**:
> Never merge pull requests or trigger PRs to be merged under any circumstances
> (e.g., via `gh pr merge`, GitHub GraphQL/REST API merge endpoints, or
> enabling auto-merge). Merging/rebasing the target base branch *into* the PR
> branch to resolve conflicts or base drift is permitted; merging the PR into
> the base branch is strictly forbidden.

> [!CAUTION]
> **NO AUTOMATIC COMMENTS, REPLIES, REVIEWS, OR THREAD RESOLUTION**:
> `pr-shepherd` and its workers have **NO exemption** to post on GitHub or
> resolve threads. You must **NEVER** autonomously:
> - Post PR/issue comments (`gh pr comment`, `gh issue comment`),
> - Post inline review replies (`gh api .../pulls/.../comments/.../replies`),
> - Submit PR reviews (`gh pr review`), or
> - Resolve or unresolve review threads (GraphQL `resolveReviewThread` /
>   `unresolveReviewThread`).
>
> **Escalate Only**: Record proposed replies in `shepherding_report.md` and
> escalate them to `pr-shepherd-watcher` / the user. Even if the user grants
> single-use consent in chat for one specific reply, that consent expires
> immediately after one use and requires explicit re-approval for any further
> reply.

### Isolated Workspace Hygiene
- Edit, build, test, and rebase **strictly inside your isolated workspace**
  (`Workspace: "branch"` or WISP worktree) so concurrent checks or main
  checkouts are never corrupted.
- Keep temporary downloaded logs/reports inside `<artifacts_dir>` or `/tmp`.
- **Never commit** `shepherding_report.md`, `shepherding_report.lock`, or
  `artifacts/` to the PR branch.
- Stage only intended files (never `git add .` or `git add -A`) and commit
  with standard Git trailers (`Tag:`, `Conv:`, and `Bug:` when applicable).

---

## 1. Fast-Path & CI Shepherd JSON Triage

> [!IMPORTANT]
> **PR Context & Root-Cause Investigation**:
> 1. **Understand the PR**: Inspect the PR description and diff
>    (`gh pr view <PR_NUMBER> --repo <owner>/<repo>`,
>    `gh pr diff <PR_NUMBER> --repo <owner>/<repo>`).
> 2. **Check Pre-Downloaded Artifacts First**: Look inside
>    `<artifacts_dir>/test_results/` and `<artifacts_dir>/logs/` populated by
>    `shepherd_pr.py` before making redundant network calls (`shepherd_pr.py`
>    downloads only failing `shepherd-report-*` directories and collision-safe
>    `<safe_name>_<job_id>.log` files for failed required checks).
> 3. **Determine Causality**: Check whether the failure touches modified files,
>    changed symbols, or affected subsystems:
>    - **PR-Caused**: Compiler errors on modified code, unit/integration test
>      failures for modified functionality, linter errors, or merge conflicts.
>    - **Flake / Infra / Base-Drift**: Network/HTTP timeouts, runner disk
>      exhaustion, machine disconnects, external service outages, or
>      pre-existing/upstream failures on `<base_branch>`.

### ⚡ Early-Failure Fast Path (No `shepherd_report.json`)

When a required check fails early (e.g., workflow syntax errors, missing
inputs, `depot_tools`/environment setup failures, action schema evaluation, or
early build failures before test report artifacts are uploaded),
`shepherd_report.json` will **not** exist:

1. **Do NOT wait for `shepherd_report.json` or other matrix jobs.**
2. **Check `<artifacts_dir>/logs/` or use the `run_id`/`job_id` from
   `shepherd_pr.py`'s output**:
   ```bash
   gh pr checks <PR_NUMBER> --repo <owner>/<repo>
   gh run view <RUN_ID> --repo <owner>/<repo>
   ```
3. **Fetch failed step logs immediately** (if not already in
   `<artifacts_dir>/logs/`):
   ```bash
   gh run view --job <JOB_ID> --log-failed --repo <owner>/<repo>
   # Only fetch full job log if --log-failed is incomplete:
   gh run view --job <JOB_ID> --log --repo <owner>/<repo>
   ```
4. **Diagnose and act**: Identify the syntax, config, or code error. If
   PR-caused, fix, verify, commit, and push immediately (pushing cancels
   obsolete pending jobs and restarts CI on the fixed commit).

### 🌟 CI Shepherd JSON Report (`shepherd_report.json`) Triage

For completed runs and test failures, Cobalt CI uploads structured
`shepherd_report.json` files inside `shepherd-report-<platform>` artifacts
(already filtered to failing platforms in `<artifacts_dir>/test_results/`, or
fetchable via
`gh run download <RUN_ID> -p "shepherd-report-*" -D /tmp/shepherd_reports --repo <owner>/<repo>`):

1. **Step 1 — Check Pipeline Stage Status (`checks`)**:
   - Inspect `checks` in `shepherd_report.json`.
   - If `"build": "failure"`, compilation failed—inspect compiler errors in
     `<artifacts_dir>/logs/` or via `gh run view --job <JOB_ID> --log-failed`.
   - If `"on-host-test"`, `"web-tests"`, or `"on-device-test"` is `"failure"`,
     proceed to Step 2.
2. **Step 2 — Inspect Failing Tests (`test_failures`)**:
   - Read `test_failures` directly for failing test case names and assertion
     messages without megabyte log grepping.
3. **Step 3 — Targeted Fallback (Only If Necessary)**:
   - If stack traces were truncated, search `<artifacts_dir>/logs/*.log` or
     `--log-failed` output with targeted signatures:
     - **Compiler Errors**: `grep -E ":[0-9]+:[0-9]+: (fatal )?error:"`
     - **Build Failures (Ninja/Siso)**: `grep -E "FAILED:|err:.*failed:"`
     - **Config Errors (GN)**: `grep "ERROR at "`
     - **Unit Tests (GTest)**: `grep -E "\[  FAILED  \]"`
     - **Crashes**: `grep -E "Segmentation fault|SIGSEGV|Traceback"`
4. **Step 4 — Classify & Act**:
   - Follow Section 2 for PR-caused fixes vs. the Flake / Infra / Base Drift
     protocol, and record the result in `shepherding_report.md` (Section 5).

---

## 2. Resolving CI Failures

### Implementing PR-Caused Fixes
1. **Immediate Remediation**: Fix immediately without waiting for remaining
   matrix jobs.
2. **Be Minimally Invasive**: Make the smallest, targeted change needed.
3. **Verify Locally**: Build and run affected tests/linters locally.
4. **Commit & Push**: Stage only intended files, commit with standard Git
   trailers, and push (`git push`, or `gh stack submit --auto` for `stack/`
   branches).

### 4-Step Flake / Infra / Base Drift Protocol

If triage reveals a failure is **not** caused by the PR diff:

1. **Step 1 — Transient Verification (Single Rerun)**:
   - First check whether `run_attempt: 2` has already auto-triggered. Note
     that GitHub Actions rejects `gh run rerun <RUN_ID> --failed` (and `--job`)
     while sibling jobs in `<RUN_ID>` are still `in_progress`; if the run is
     still active, wait for `<RUN_ID>` to finish (or record it for the Watch
     Coordinator to rerun once `<RUN_ID>` completes).
   - Trigger one rerun of failed jobs:
     ```bash
     gh run rerun <RUN_ID> --failed --repo <owner>/<repo>
     ```
   - Record the rerun ID in `shepherding_report.md` and return control to the
     Watch Coordinator (or verify once if already rerun for this commit).
2. **Step 2 — Base Drift Check & Upstream Search**:
   - If the failure persists after a single rerun, check how far behind
     `<base_branch>` the PR is and search recent upstream commits/PRs for the
     failing test name or error signature:
     ```bash
     git fetch origin <base_branch>
     git rev-list --count HEAD..origin/<base_branch>
     git log origin/<base_branch> --grep="<test_name_or_symbol>" -n 10 --oneline
     gh pr list --repo <owner>/<repo> --search "<test_name_or_error>" --state merged -L 5
     ```
3. **Step 3 — Autonomous Rebase / Sync & Push**:
   - If the PR is behind base or an upstream fix landed on `<base_branch>`,
     autonomously rebase onto `origin/<base_branch>` (or sync for stacked PRs)
     and push to test against the latest base:
     ```bash
     # Regular PR:
     git rebase origin/<base_branch>
     git push --force-with-lease

     # Stacked PRs (`stack/` prefix — never raw `git push`):
     gh stack sync
     gh stack submit --auto
     ```
   - Record the verified `origin/<base_branch>` SHA and rebase/sync action in
     `shepherding_report.md`.
4. **Step 4 — Structured Human Escalation (Persistent Failure on Latest Base)**:
   - If the failure persists after rebasing onto the latest base (or the PR was
     already on the latest base):
     - **HALT** remediation. Do not retry in a loop.
     - Do **NOT** edit test filter manifests (`*_filter.json`, `filter.txt`)
       or add skip/disable annotations.
     - Write a structured escalation entry in `shepherding_report.md`
       (Section 5 + Ledger) and report up to `pr-shepherd-watcher` containing:
       - Exact failing job, test case, and log snippet.
       - Confirmation that PR changes do not touch the failing subsystem.
       - Verified `origin/<base_branch>` commit SHA after rebase.
       - Whether the latest base branch itself exhibits the failure.

---

## 3. Merge Conflicts, Review Comments & Stacked PRs (`gh stack`)

### Resolving Merge Conflicts
1. **Regular (Non-Stack) PRs**:
   ```bash
   git fetch origin
   git merge origin/<base_branch>   # or: git rebase origin/<base_branch>
   # Resolve conflict markers (<<<<<<<, =======, >>>>>>>), verify build/tests
   git add <resolved_files>
   git commit --no-edit              # or: git rebase --continue
   git push                          # or: git push --force-with-lease if rebased
   ```
2. **Stacked PRs (`stack/` branch prefix)**:
   ```bash
   gh stack sync
   # Resolve conflict markers, verify build/tests
   git add <resolved_files>
   gh stack rebase --continue
   gh stack submit --auto
   ```
3. **Update Action Ledger**: Record the conflict resolution commit/rebase in
   `shepherding_report.md`.

### Handling Review Comments (Draft & Escalate Replies Only)
1. Locate the file, line, and `comment_id` indicated in `shepherd_pr.py`'s
   output (`Unresolved thread [comment_id=<ID>] <path>:<line>`).
2. If the comment requests a clear, scoped code change adhering to the
   repository style guide, implement it (plus required build/test/caller fixes),
   verify locally with tests/linters, commit, and push so CI runs. If the
   comment is ambiguous, asks a design question, or requires a judgment call, do
   not guess—draft options and escalate.
3. **Reply & Thread Resolution Policy (Strictly Prohibited Autonomously)**:
   - **Never** post GitHub comments, post inline replies
     (`gh api .../replies`), submit reviews (`gh pr review`), or resolve/unresolve
     threads (`resolveReviewThread`/`unresolveReviewThread`) autonomously.
   - Always record the `comment_id`, code fix commit SHA (if pushed), and a
     suggested draft reply in `shepherding_report.md` (Section 4 `Review Comments`
     with status `Code Fixed (Reply Pending Consent)` or `Drafted (Never Posted)`
     and Section 2 Ledger) and report to `pr-shepherd-watcher` so the watcher can
     pass `--ignore-comment-ids` while CI runs and escalate the drafts to the
     user once CI is green.

### Stacked PRs & `gh stack` Upstack Cascade
1. **Branch Naming**: Stack branches pushed to `origin` follow
   `origin:stack/<username>/<branch>`. Never run raw `git push` on `stack/`
   branches; always use `gh stack` commands.
2. **Bottom-Up Progression**: Stabilize lower layers before upper layers.
3. **Upstack Cascade**: Whenever you modify or rebase a lower layer in a stack,
   cascade the update to dependent upstack branches and submit non-interactively:
   ```bash
   gh stack rebase --upstack
   gh stack submit --auto
   ```
   Verify dependent layers compile and pass tests.

---

## 4. Appending Actions to `shepherding_report.md`

Always update the shared Action Ledger (`shepherding_report.md` in the watcher
workspace root) before returning to `pr-shepherd-watcher`. Every read-modify-write
cycle **MUST** use `shepherding_report.lock` (`flock`), re-read after locking,
and perform atomic replacement:

```python
import fcntl
import os

report_path = "<watcher_workspace_root>/shepherding_report.md"
lock_path = "<watcher_workspace_root>/shepherding_report.lock"

with open(lock_path, "w", encoding="utf-8") as lock_f:
    fcntl.flock(lock_f, fcntl.LOCK_EX)
    content = ""
    if os.path.exists(report_path):
        with open(report_path, "r", encoding="utf-8") as rf:
            content = rf.read()
    # Append row(s) to Section 2 (Ledger of Actions Taken) and update
    # Section 3 (CI Checks), Section 4 (Review Comments), or Section 5
    # (Flaky Tests, Base Drift & Infrastructure Escalations) as applicable.
    tmp_file = f"{report_path}.tmp.{os.getpid()}"
    with open(tmp_file, "w", encoding="utf-8") as wf:
        wf.write(updated_content)
    os.replace(tmp_file, report_path)
```

Record:
- Timestamp, target PR & check/thread (`comment_id`), classification
  (`PR-Caused`, `Flake`, `Infra`, `Base-Drift`, `Review-Comment`,
  `Merge-Conflict`), exact action taken (commit SHA, `gh run rerun` ID, rebase
  base SHA, upstack cascade, or draft reply), and outcome (`Verifying`, `Passed`,
  `Code Fixed (Reply Pending Consent)`, `Drafted (Never Posted)`, or
  `Escalated`).
