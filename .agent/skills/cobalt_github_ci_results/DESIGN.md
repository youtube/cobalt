# GitHub Actions CI Results Retriever Design

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
├── github_download.py
└── github_download_test.py
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

The script classifies jobs into one of three categories:

*   Meta Jobs: Jobs that spawn other jobs or perform some CI chore (Docker
    builds, on-device test jobs).
*   Build Jobs: Jobs that perform builds.
*   Test Jobs: Jobs that run tests.

### Parallellization

The script is highly parallelized. Use futures and executor pools to
submit slow tasks (like network calls via `gh`) for optimal throughput.

### High Level Steps

Here are the general logical steps the script takes to retrieve results:

1.  **Job Discovery**
    *   **Default Mode**: Parse a source of truth (like
        `cobalt/BUILD_STATUS.md`) to extract workflow names (representing nightly and postsubmit jobs) and
        fetch their latest runs.
    *   **By Run ID**: If a run ID is provided, fetch jobs for that specific
        run.
2.  **Job Status Retrieval & Step-Level Check**
    *   Query GitHub API to get the latest run status.
3.  **Log Fetching & Test Result Extraction**
    *   For failing test jobs, attempt to download XML test results artifacts
        (matching `<test_target>*.xml`) via `gh run download`. If found, parse
        JUnit XML failures directly and write them as a synthetic log trace to
        the cache directory, avoiding raw logs download.
    *   For remaining failed jobs, fetch full log content using `gh api` and
        save it to the cache directory.
4.  **JSON Output Generation**
    *   Write a structured JSON report to the path specified by `--output` conforming to the unified schema.

### Concurrency Model

To optimize performance, the script employs a concurrency model using
`ThreadPoolExecutor` to process jobs in parallel when fetching logs and
extracting results.

All discovered jobs are submitted to a single thread pool for processing. This avoids sequential bottlenecks and speeds up execution.

## Logging and Output

The final JSON results are written to the file specified by `--output`.

Logging information, warnings, and errors are directed to `stderr`. Logging verbosity is configurable via `--verbose` flag.

## Updating the Script

1.  **Test Before Edits**: Always cover changes to the script with unit tests in
    `{skill_dir}/scripts/github_download_test.py`. Always add or edit test cases first. Then run tests and the
    script after making edits to ensure correctness.
