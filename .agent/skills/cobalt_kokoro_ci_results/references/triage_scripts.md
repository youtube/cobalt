# The Kokoro Triage Scripts

> [!IMPORTANT] These scripts are specifically created for and should only be
> used by this skill. They are not intended to be used as a general library.

## Table of Contents

-   [Scripts Directory Tree](#scripts-directory-tree)
-   [Tools](#tools)
-   [Implementation](#implementation)
    -   [Concurrency Model](#concurrency-model)
    -   [Direct Queries](#direct-queries)
-   [Logging and Output](#logging-and-output)

## Scripts Directory Tree

The layout of the Kokoro triage scripts:

```
scripts/
├── kokoro_download.py
└── kokoro_download_test.py
```

## Tools

Here is a list of tools used by the scripts:

*   **`stubby`**: Used to query build statuses (`kokoro.status.v1.KokoroStatus.GetLatestStatus`) and fetch ResultStore actions (`ExportInvocation`).
*   **`fileutil`**: Used to locate and copy files from CNS (`fileutil ls` and `fileutil cp`).
*   **`cs` (Code Search)**: Used to search the codebase for `.gcl` config files.
*   **Standard Python Libraries**: `subprocess`, `json`, `re`, `concurrent.futures`, `tempfile`, `glob`.

> [!IMPORTANT] Do NOT use MCP servers directly in the scripts. Scripts should
> rely on standard CLI tools or APIs. MCP servers are intended for use by the
> agent.

## Implementation

The script merges discovery, status checking, log downloading, and child job expansion into a single execution flow that outputs a JSON file conforming to the unified schema.

### Concurrency Model

`kokoro_download.py` utilizes a `ThreadPoolExecutor` to handle the I/O-bound operations (network requests via `stubby` and `fileutil` copies) in parallel:
-   During discovery, all discovered jobs are submitted to the thread pool to check their status concurrently.
-   For failing jobs, log downloads and child job expansions are executed concurrently, reducing total runtime.

### Direct Queries

For targeted troubleshooting, `kokoro_download.py` supports two direct query flags:
*   `--job <job_name>`: Bypasses GCL discovery and triages only the specified job.
*   `--build <build_id>`: Bypasses both discovery and status checks, directly downloading logs for the given Sponge invocation ID.

## Logging and Output

The final JSON results are written to the file specified by `--output`.

Logging information, status updates, and errors are written to `stderr` to avoid polluting the output. Pass the `--verbose` flag to enable detailed debugging output.
