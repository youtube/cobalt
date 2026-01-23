# Code Coverage Tool

This directory contains scripts to facilitate code coverage scans within the Cobalt project.

## End-to-End Usage

This section describes the complete process for generating a code coverage report, from running the scan to viewing the final HTML output.

### Step 1: Run the Coverage Scan

The `run_coverage.py` script is the primary tool for executing coverage scans. It builds the specified targets with the necessary coverage instrumentation and runs them to collect data.

**Command:**

```bash
python3 cobalt/tools/code_coverage/run_coverage.py -p <PLATFORM> -o <OUTPUT_DIR>
```

*   `-p, --platform`: The platform to build and run the tests on (e.g., `android-x86`).
*   `-o, --output-dir`: The directory where the raw coverage data (`.lcov` files) will be stored.

**Example:**

This command will build and run all auto-discovered test targets for the `android-x86` platform and save the raw coverage data to `out/android-x86_coverage`.

```bash
python3 cobalt/tools/code_coverage/run_coverage.py -p android-x86 -o out/android-x86_coverage
```

### Step 2: Generate the HTML Report

After the scan is complete, the `generate_coverage_report.py` script is used to process the raw `.lcov` data and generate a user-friendly HTML report.

**Command:**

```bash
python3 cobalt/tools/code_coverage/generate_coverage_report.py -p <PLATFORM>
```

*   `-p, --platform`: The platform for which to generate the report (e.g., `android-x86`).

This script will automatically:
1.  Locate the raw coverage data from the `out/<PLATFORM>_coverage` directory.
2.  Clean and merge the data, excluding irrelevant files (like generated code in `out/`).
3.  Generate the final HTML report in `out/cobalt_coverage_report`.

**Example:**

```bash
python3 cobalt/tools/code_coverage/generate_coverage_report.py -p android-x86
```

**Viewing the Report:**

Once the script finishes, you can open the generated report in your browser:

```
file:///path/to/your/checkout/src/out/cobalt_coverage_report/index.html
```

## Test Filtering

The `run_coverage.py` script automatically discovers and applies test filters located in `cobalt/testing/filters/`. For each test target, the script looks for a corresponding `<target_name>_filter.json` file in the appropriate platform-specific directory.

If a filter file is found, the tests listed in the `failing_tests` section will be excluded from the coverage run using the `--gtest_filter` flag.

### Example Filter File

`cobalt/testing/filters/android-arm/net_unittests_filter.json`:

```json
{
  "comment": "Failing on missing libgio-2.0.so.0",
  "failing_tests": [
    "*"
  ]
}
```

In this example, all tests in the `net_unittests` target will be excluded from the coverage run.



### Usage

```bash
python3 cobalt/tools/code_coverage/generate_coverage_report.py --input-file <PATH_TO_LCOV_FILE>
```

*   `--input-file`: The path to the `.lcov` file you want to process.

This will generate a dedicated HTML report for that file in a directory named `out/<LCOV_FILE_NAME>_report`.

### Example

To generate a report for the `cobalt_unittests` coverage data:

```bash
python3 cobalt/tools/code_coverage/generate_coverage_report.py --input-file out/android-x86_coverage/cobalt_cobalt_unittests/linux/coverage.lcov
```

The report will be generated in `out/coverage_report`. You can view it by opening `out/coverage_report/index.html` in your browser.
