# The Triage Scripts

> [!IMPORTANT] These scripts are specifically created for and should only be
> used by this skill. They are not intended to be used as a general library.

## Table of Contents

-   [Scripts Directory Tree](#scripts-directory-tree)
-   [Tools](#tools)
-   [Implementation](#implementation)
    -   [High Level Steps](#high-level-steps)
    -   [Concurrency Model](#concurrency-model)
-   [Logging and Output](#logging-and-output)
-   [Updating the Script](#updating-the-script)

## Scripts Directory Tree

The layout follows this general outline:

```
scripts/
├── gardener_utils.py
├── gardener_utils_test.py
├── github_discover.py
├── github_discover_test.py
├── github_download.py
├── github_download_test.py
├── result_analyzer.py
├── result_analyzer_test.py
```

## Tools

Here is a list of tools used by the scripts:

*   **`gh` (GitHub CLI)**: Used for interacting with GitHub directly via CLI
    commands (e.g., `gh run list`, `gh pr view`, `gh api`, and `gh run
    download`) to fetch jobs, status checks, logs, and XML results artifacts.
*   **Standard Python Libraries**: `subprocess`, `json`, `re`,
    `concurrent.futures`, `xml.etree.ElementTree`, `glob`, `tempfile`.

> [!IMPORTANT] Do NOT use MCP servers directly in the scripts. Scripts should
> rely on standard CLI tools or APIs. MCP servers are intended for use by the
> agent.

## Implementation

The script should classify jobs into one of three categories:

*   Meta Jobs Jobs that spawn other jobs or perform some CI chore (Docker
    builds, on-device test jobs).
*   Build Jobs Jobs that perform builds.
*   Test Jobs Jobs that run tests.
*   Result Processing Jobs Jobs that process test results and present logs from
    e.g. internal on-device test jobs.

Analyze the job definitions in the `.github/` directory for reference.

### Parallellization

The script should be highly parallelized. Use futures and executor pools to
submit slow tasks (like network calls via `gh`) for optimal throughput.

### High Level Steps

Here are the general logical steps the script takes to triage failures:

1.  **Job Discovery**
    *   **Default Mode (Build Gardener Duty)**: Parse a source of truth (like
        https://raw.githubusercontent.com/youtube/cobalt/refs/heads/main/cobalt/BUILD_STATUS.md)
        to extract workflow names (representing nightly and postsubmit jobs) and
        fetch their latest runs. This is the primary mode of operation.
    *   **By Run ID**: If a run ID is provided, fetch jobs for that specific
        run.
    *   **By PR**: If a PR number is provided, fetch status checks for that PR
        using `gh pr view`.
2.  **Job Status Retrieval & Step-Level Check**
    *   Query GitHub API or run CLI commands to get the latest run status or
        check status.
3.  **Log Fetching & Test Result Extraction**
    *   For failing test jobs, attempt to download XML test results artifacts
        (matching `<test_target>*.xml`) via `gh run download`. If found, parse
        JUnit XML failures directly and write them as a synthetic log trace to
        the local log file, avoiding raw logs download.
    *   For remaining failed jobs, fetch full log content using `gh api` and
        save it directly to the local log file.
4.  **Failure Cause Analysis**
    *   Use `result_analyzer.py` to scan logs for failure patterns (such as
        compilation errors, GN configs syntax errors, Siso/Ninja build actions
        failures, runtime exceptions/crashes, and transient infra flakes).
    *   **Rule**: If an internal job (e.g., Kokoro check on GitHub) is detected
        as failed, report it with a link but do not attempt to triage it.
5.  **Report Generation**
    *   Output a structured markdown report to `stdout`. See Report Format in
        SKILL.md for details.

### Concurrency Model

To optimize performance, the script employs a concurrency model using
`ThreadPoolExecutor` to process jobs in parallel when fetching logs and
analyzing failures.

All discovered jobs are submitted to a single thread pool for result processing
(fetching logs and analyzing failures). This avoids sequential bottlenecks and
speeds up execution.

## Logging and Output

The final triage report MUST always be printed to `stdout` as markdown.

Logging information, warnings, and errors should be directed to `stderr` or a
log file, and should not pollute the `stdout` output. Logging verbosity should
be configurable (e.g., via a `--verbose` flag) to aid in debugging without
affecting the machine-readable report output.

## Updating the Script

1.  **Test Before Edits**: Always cover changes to the script with unit tests in
    `{skill_dir}/scripts/<script_name>_test.py` (located in the same directory
    as the script). Always add or edit test cases first. Then run tests and the
    script after making edits to ensure correctness.
