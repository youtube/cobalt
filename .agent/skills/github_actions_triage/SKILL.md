---
name: github-actions-triage
description: >-
  Triages GitHub CI failures for nightly, postsubmit, and PR builds. Use when
  requested to triage GitHub CI failures, analyze job logs, act as a build
  gardener, or perform github actions triage for a GitHub repository. Don't use
  for internal Google CI systems like Kokoro or for projects not on GitHub.
---

# GitHub Actions Triage Skill

## TL;DR Add these to the TODO list

*   **Discover & Download Logs**
*   **Analyze Results**
*   **Verify Report**
*   **Summarize Report**
*   **Report Review**

## Skip Implementation Plan

When performing GitHub Actions triage, **proceed directly to execution** as this
is an investigatory and reporting task. No implementation plan or further user
approval is necessary unless explicitly requested by the user.

## Prerequisites

-   `gh` CLI installed and authenticated.

## Detailed Steps

-   [ ] **Discover & Download Logs**

    ```bash
    python3 {skill_dir}/scripts/github_discover.py --output github_jobs.json
    ```
    ```bash
    python3 {skill_dir}/scripts/github_download.py github_jobs.json --output github_results.json
    ```

-   [ ] **Analyze Results**

    ```bash
    python3 {skill_dir}/scripts/result_analyzer.py github_results.json
    ```

-   [ ] **Verify Report** For each failed job, you must verify the failure
    details:

    *   **Confirm Failure Cause**: Verify that the matching error signature
        lines reported in the script report are indeed the actual cause of the
        failure. Confirm that the detected error (rather than some warning or
        irrelevant log noise) is what prevented the run from completing
        successfully.
    *   **No Errors Detected Handling**: If a failed job has "No matching error
        signature found" or similar undetermined status:
        1.  Open the cached log file locally or via the Job URL.
        2.  Inspect the log (especially near the end) to find the lines that
            detail why the job failed.
        3.  Note the root cause and the specific error line in your final
            report.
        4.  Suggest updating the `RULES` in `result_analyzer.py` in the "Next
            Steps" so that similar failures can be automatically detected in the
            future.
    *   **Bug Finder**: An expert in finding bugs related to Cobalt GitHub
        Actions Triage issues. Instruct it to start searching the
        [Gardener Hotlist](https://b.corp.google.com/hotlists/7245259) and the
        Cobalt component and its children (componentid:114154+) to see if the
        verified error cause matches any known bugs.

-   [ ] **Summarize Report** Summarize the results using the instructions in
    "Report Format" below.

    Display the Triage Report to the user and save it to
    `<cwd>/gardener_report_<date>.md`.

-   [ ] **Report Review** Create a self agent and instruct it to review both the
    script reports and the summary follow the guidelines in
    [Report Formats](#report-formats).

### Next Steps

-   [ ] **Next Steps**: Halt and ask the user to approve next steps.
    *   **Self-Evolution**: If you encounter a cause that the script does not
        catch or identify correctly, or if logs are missing for any job, you
        MUST suggest updating the script or the error classification rules to
        handle the failure in the future.
    *   **Triage Hotlist Bugs**
        *   List of bugs to create for newly encountered issues.
        *   List of bugs that match the bug but not added on the Gardener
            Hotlist.
    *   **Investigate Fixes**: Create a list of investigative tasks to find and
        fix the issues in the report.
    *   **Restart failing jobs**: Jobs that are failing due to flaky tests/infra
        or other transient issues should be restarted. Also applies to jobs for
        which no logs or results are available. Provide a list and prompt the
        user to approve.

## Build Recency

To ensure that the triage focuses on current and relevant issues, we apply a
recency filter to the triaged builds:

-   **Nightly Builds** (event is `schedule` or `workflow_dispatch`): Only runs
    completed within the **past 24 hours** are considered for detailed log
    triage.
-   **Postsubmit Builds** (event is `push` or other non-nightly events): Only
    runs completed within the **past 7 days (1 week)** are considered for
    detailed log triage.

### Outdated Builds

Runs older than these thresholds are classified as **outdated**. For outdated
builds:

-   The script does not download or parse their logs.
-   They are reported under the "Outdated Failed Runs" section of the branch
    report.
-   The triage agent should suggest retriggering these builds to get fresh
    results.

## Error Matching Signatures

Instead of deducing the error type/category, the triage script uses defined
regular expression signatures to locate where errors occurred in the downloaded
log files. The script reports all matching locations per job.

### Common Signatures

The matching rules look for the following patterns:

1.  **Infrastructure Signatures**:
    -   VM setup, network disconnects, API limit errors: "Runner lost
        communication", "connection reset by peer", "API rate limit exceeded",
        "No space left on device", "HTTP 429", "CIPD server error", etc.
2.  **GN Config Signatures**:
    -   `ERROR at //.*\.gn`
3.  **Ninja/Siso Signatures**:
    -   `FAILED: (?:obj)?/`, `err: remote-exec.*failed`
4.  **Compilation Signatures**:
    -   C++/Java/Kotlin/Kotlin compiler error patterns (e.g. `file.cc:12:
        error:`, `file.java:23: error:`, `undefined reference to`)
5.  **Test Failure Signatures**:
    -   `[ FAILED ]`, `FAIL:`, `JUnit Failure:`, `Result: FAIL`
6.  **Crash Signatures**:
    -   `Segmentation fault`, `SIGSEGV`, `CRASHED`, `Received signal`, python
        tracebacks.

## Report Formats

### Script Report

The script produces a raw report listing the failing jobs in markdown format
(not JSON), grouped per branch, and providing a branch health report.

For each branch, it lists the health status (Healthy, Unhealthy, or Outdated
Failures). Under each Unhealthy branch, it lists the detailed failed jobs with
their Cached Log path, Job URL, Invocation Time, and **Error Location(s)**
pointing directly to the lines in the log where signatures matched.

#### Example

```markdown
# GitHub Build Status Triage Report

## Job Stats
*   **Total Jobs Fetched**: 5
*   **Failed Jobs**: 3

## Branch Health Report

### Branch: main (Unhealthy)
*   **Failed Jobs**: 2

### Branch: 27.lts (Outdated Failures)
*   No recent failures, but 1 outdated failed run(s) exist (retrigger suggested).

## Detailed Branch Failures

### Branch: main

#### Job: linux_compilation
*   **Invocation Time**: TODO
*   **Cached Log**: `/tmp/github_gardener_oxv/compilation_error.log`
*   **URL**: https://github.com/youtube/cobalt/actions/runs/123/job/1
*   **Error Location(s)**:
    *   Line 3: `test.cc:12: error: expected ';' before '}' token`
        *   Log Slice Command: `tail -n +1 "/tmp/github_gardener_oxv/compilation_error.log" | head -n 11`
    *   Line 4: `FAILED: obj/test.o`
        *   Log Slice Command: `tail -n +1 "/tmp/github_gardener_oxv/compilation_error.log" | head -n 11`

#### Job: linux_test
*   **Invocation Time**: TODO
*   **Cached Log**: `/tmp/github_gardener_oxv/test_failure.log`
*   **URL**: https://github.com/youtube/cobalt/actions/runs/123/job/2
*   **Error Location(s)**:
    *   Line 2: `[  FAILED  ] MyTest.TestOne (5 ms)`
        *   Log Slice Command: `tail -n +1 "/tmp/github_gardener_oxv/test_failure.log" | head -n 11`

### Branch: 27.lts

#### Outdated Failed Runs (Retrigger Suggested)
*   **android_27** (Run ID: 444, schedule) - Failed 2 day(s) ago.
    *   URL: https://github.com/youtube/cobalt/actions/runs/444
```

### Triage Report

The agent must summarize the raw script reports into a human-readable format.
The agent interprets the matching error locations to deduce the root cause, and
groups issues per branch:

For each branch, provide a health status and summarize the failures:

-   Main failure cause (e.g. Compilation error, Test failure, etc.) deduced from
    the matching log lines.
-   Number of failures and list of affected jobs.
-   Failure log lines.
-   Links to the CI runs.
-   Suggest actions (e.g. retriggering for outdated runs, filing bugs for new
    errors, etc.).

#### Example

*   **main** (Unhealthy) - 2 failed jobs

    *   **compilation_error**: 1 failure (linux_compilation)
        *   Failed to find standard headers.
        *   `fatal error: stdio.h not found`
        *   Bug: NEW
        *   Link:
            [linux_compilation](https://github.com/youtube/cobalt/actions/runs/123/job/1)
    *   **test_failure**: 1 failure (linux_test)
        *   `MyTest.TestOne` failed.
        *   `[ FAILED ] MyTest.TestOne`
        *   Bug: b/1234
        *   Link:
            [linux_test](https://github.com/youtube/cobalt/actions/runs/123/job/2)

*   **27.lts** (Outdated Failures) - 0 recent failed jobs

    *   No recent failures, but 1 outdated failed run exists:
        *   **android_27** (Run ID: 444, schedule) - Failed 2 day(s) ago.
            [Link](https://github.com/youtube/cobalt/actions/runs/444).
    *   **Action**: Suggest retriggering the nightly job to get fresh builds.
