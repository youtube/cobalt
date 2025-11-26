# Cobalt Builds Gemini Extension

## Installation

To install this extension for the Gemini CLI, run the following
command from the root of the Chromium repository:

```
gemini extensions install --path  agents/extensions/cobalt_builds
```

Or, run the following command from your home folder:
```
vpython3 chromium/src/agents/extensions/install.py add cobalt_builds
```

To confirm the MCP is installed and working, `/mcp list` from gemini
CLI should show a line `cobalt_builds (from cobalt-builds) - Ready (7
tools)`

This extension provides an MCP server with tools for configuring,
building, and running Cobalt binaries, intended for use with
gemini-cli.

## Command-Line Utility (`do`)

This extension includes a command-line utility `do` for easy
interaction with the build server.  It can be used to configure,
build, and run Cobalt binaries.  All commands are invoked via the
`do` script.

### Quick Start Examples

**Configure a `devel` build for the `non-hermetic` platform:**
```bash
./agents/extensions/cobalt_builds/do configure_build non-hermetic devel
```

**Build the `nplb` target:**
```bash
./agents/extensions/cobalt_builds/do build non-hermetic devel nplb
```

**Run the `nplb` binary:**
```bash
./agents/extensions/cobalt_builds/do run non-hermetic devel nplb
```

**Build with a dry-run to see the command:**
```bash
./agents/extensions/cobalt_builds/do build non-hermetic devel nplb --dry-run
```

### MCP Command Overview

*   **`get_supported_platforms`**: Lists all available build platforms.
*   **`configure_build`**: Generates the build configuration for a
    given platform and variant.
*   **`build`**: Compiles a specified target for a given platform and variant.
*   **`run`**: Executes a binary for a given platform and variant.
*   **`stop`**: Terminates a running `build` or `run` task.
*   **`check_status`**: Checks the status of a `build` or `run` task.
*   **`clear_status`**: Clears the status of a completed task.
*   **`analyze`**: Analyzes a build or run log for errors and warnings.

## Testing

This extension includes a test suite for the log analysis
functionality.  The tests are located in the
`agents/extensions/cobalt_builds/tests` directory.

To run the tests, execute the following command from the root of the
Chromium repository:

```bash
./agents/extensions/cobalt_builds/tests/test_analysis.sh
```

## Code Structure

The log analysis functionality is structured as a pipeline, with each
stage of the analysis encapsulated in its own class.

### Core Components

*   **`analysis.py`**: This is the main entry point for the log
    analysis.  It contains the `BaseAnalyzer` class and its subclasses,
    `TestLogAnalyzer` and `BuildLogAnalyzer`.

*   **`TestLogAnalyzer`**: This class orchestrates the analysis of
    test logs.  It uses the following components in sequence:
    1.  **`GtestParser`**: Parses gtest markers and determines test boundaries.
    2.  **`IssueDetector`**: Identifies individual issues from log lines.
    3.  **`IssueConsolidator`**: Clusters and consolidates the detected issues.
    4.  **`ReportGenerator`**: Formats the final analysis report.
*   **`BuildLogAnalyzer`**: This class is responsible for analyzing build logs.

### Supporting Modules

*   **`lib/`**: This directory contains the pipeline components.
*   **`models.py`**: Defines the data structures used in the
    analysis, such as `LogEvent` and `Issue`.
*   **`patterns.py`**: Centralizes all regular expressions.
*   **`config.py`**: Contains shared configuration constants.
*   **`server.py`**: Contains the `TaskRunner` class for managing
    build and run processes.
