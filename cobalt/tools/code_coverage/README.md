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

## Generating a Report from a Single LCOV File

If you want to view the coverage report for a single test target without merging it with others, you can use the `--input-file` argument.

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


### Generating GN Arguments for Coverage Builds

Before running the code coverage script, you need to ensure that your build directory is configured with the correct GN arguments for coverage. The `cobalt/build/gn.py` script has been updated to support this.

#### Usage

To generate the necessary GN arguments for a coverage build, run the `gn.py` script with the `--coverage` flag:

```bash
python3 cobalt/build/gn.py -p <PLATFORM> --coverage
```

*   `-p, --platform <PLATFORM>`: The platform to build for (e.g., `android-x86`).

This will create a build directory named `<PLATFORM>_devel-coverage` (e.g., `out/android-x86_devel-coverage`) and generate an `args.gn` file with the required coverage flags.

### Usage of the Code Coverage Tool

Once you have generated the GN arguments, you can run the code coverage tool. The `code_coverage_tool.py` script accepts the same arguments as the upstream `tools/code_coverage/coverage.py` script. It automatically injects the `--coverage-tools-dir` argument if it's not already provided, pointing to the default LLVM build directory.

To use the tool, navigate to the `src/` directory (the root of your checkout) and execute the script:

```bash
python3 cobalt/tools/code_coverage/code_coverage_tool.py [OPTIONS] <TARGETS>
```
