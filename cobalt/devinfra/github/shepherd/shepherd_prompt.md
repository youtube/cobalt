Analyze the CI test failures for the current PR.
All test failure artifacts, reports, and check statuses have already been fetched and preloaded into the `artifacts/` directory.
Inspect `artifacts/summary.json` and any logs in `artifacts/logs/` or `artifacts/test_results/`.
Inspect the PR code changes if needed to determine whether failures are caused by PR modifications.
Do not run git fetch or git clone. Do not perform extraneous network searches or repetitive commands.
If detailed step logs could not be retrieved or the check is an external build status, formulate your determination directly from the available check statuses and failure descriptions.

## Critical Triage and Deduction Rules

### 1. Verdict Determination
- "pass": All CI checks succeeded, no failed checks, and `is_green` is true.
- "failure": Any deterministic failure introduced by PR code, including:
  - Compilation or build errors (e.g. clang, gcc, ninja, rustc).
  - Deterministic test assertion failures (e.g. gtest, junit, math or unit test failures).
  - Lint, formatting, or missing trailer checks (e.g. missing Bug ID).
  - Merge conflicts or dirty state (`mergeable: CONFLICTING` or `merge_state_status: DIRTY`).
- "flaky": Failures caused by transient issues, infrastructure errors, or intermittent flakes, including:
  - Network/socket timeouts, runner disconnects, or VM infrastructure failures.
  - Tests that failed on attempt 1 but passed on subsequent retry attempts.
  - External service/worker timeouts (e.g. Kokoro worker VM communication timeout).

### 2. Recommendation Rules
- When verdict is "pass":
  - `action`: "none"
  - `restart_failed_jobs`: false
  - `labels`: []
- When verdict is "failure":
  - `action`: "none"
  - `restart_failed_jobs`: false
  - `labels`: []
- When verdict is "flaky":
  - If the failure is in an external Kokoro check (e.g. starts with `ci/kokoro:` or platform is `tvos` / kokoro):
    - Kokoro builds cannot be restarted via GitHub Actions run rerun.
    - `action`: "apply_labels"
    - `labels`: ["kokoro:run"]
    - `restart_failed_jobs`: false
  - If the failure is in a standard GitHub Actions workflow (e.g. linux, android, evergreen):
    - `action`: "restart_failed_jobs"
    - `restart_failed_jobs`: true
    - `labels`: []

## Output Format
You MUST write your final determination JSON object directly to the file `artifacts/triage_analysis.json` using the `write_file` tool.
The JSON object must contain the following fields:
- "verdict": "failure" | "flaky" | "pass"
- "confidence": a percentage string (e.g. "95%") indicating your confidence in the verdict
- "summary": a short, concise description of the CI results in plain text (under 100 characters, no line breaks, suitable for a GitHub commit status description, e.g. "All checks passed", "Compilation error in window.cc", "Flaky network timeout in web_platform_tests")
- "findings_summary": markdown text explaining the root cause and triage recommendations to append to the PR comment
- "recommendation": an object with:
  - "action": "restart_failed_jobs" | "apply_labels" | "none"
  - "labels": array of label strings (e.g. ["kokoro:run"] or [])
  - "restart_failed_jobs": boolean (ALWAYS explicitly set to true or false, never null or omitted)
