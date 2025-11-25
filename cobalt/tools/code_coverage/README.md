# Code Coverage Tools

This directory contains scripts for generating and analyzing code coverage reports
for Cobalt.

## `establish_coverage_baseline.py`

This script is the primary orchestrator for generating code coverage data. It
uses Clang's source-based code coverage to instrument binaries, run tests, and
produce LCOV files.

By default, this script automatically applies test filters based on the target
platform. These filters are defined in JSON files located under
`cobalt/testing/filters/<platform>/`. For example, `android-arm` builds will
use filters from `cobalt/testing/filters/android-arm/`. A special case exists
where `android-x86` builds will also map to the `android-arm` filters.

### Usage

```bash
python3 cobalt/tools/code_coverage/establish_coverage_baseline.py -p <platform> [options]
```

### Key Options

-   `--include-skipped-tests`: Include tests that are normally skipped by the
    platform-specific filters.

See the script's `-h` flag for a full list of options.

## `check_coverage.py`

This script provides a way to parse LCOV files and enforce coverage quality
gates, making it ideal for CI/CD integration. It can calculate both absolute
(total) coverage and differential (new code) coverage.

### Features

-   **Absolute Coverage**: Calculates the total line coverage from an LCOV file.
-   **Differential Coverage**: Uses `diff-cover` to calculate the line coverage
    of only the code changed in the current branch compared to a base branch
    (e.g., `origin/main`).
-   **Threshold Enforcement**: Fails with a non-zero exit code if differential
    coverage is below a specified threshold.
-   **Markdown Reports**: Generates a human-readable `coverage_report.md` file
    summarizing the findings.

### Usage

```bash
python3 cobalt/tools/check_coverage.py --lcov-file <path_to_lcov> --threshold <percentage>
```

## `run_pr_coverage.py`

This Python script orchestrates a typical CI workflow for checking code
coverage on a pull request. It replaces the previous shell script with a more
robust and portable Python implementation.

### Workflow

1.  Installs the `diff-cover` Python dependency.
2.  Runs the Chromium `coverage.py` tool to build a specified test target
    (or `crypto_unittests` by default) and generate an LCOV report.
3.  Runs `check_coverage.py` to:
    -   Calculate absolute and differential coverage.
    -   Enforce an 80% coverage threshold on new code.
    -   Generate `coverage_summary.md`.
4.  Prints the location of the final report.

### Usage

Default target (`cobalt_unittests`):
```bash
python3 cobalt/tools/code_coverage/run_pr_coverage.py
```

Specifying a custom target:
```bash
python3 cobalt/tools/code_coverage/run_pr_coverage.py url_unittests
```