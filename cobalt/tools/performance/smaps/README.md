# SMAPS Capture Tool

This directory contains a Python script (`smaps_capture.py`) designed to periodically capture memory usage data (specifically, `/proc/<pid>/smaps` content) for a specified Android process. This tool is useful for long-term memory profiling and identifying potential memory leaks in applications running on an Android device.

## Features

*   **Periodic Capture:** Captures smaps data at a configurable interval.
*   **Duration Control:** Runs for a specified total duration and then exits.
*   **Configurable:** Allows customization of the target process, capture interval, duration, output directory, and ADB settings via command-line arguments.
*   **Testable:** The script is refactored into a class for easy testing and mocking of system calls.

## Usage

To run the script, navigate to the `cobalt/tools/performance/smaps/` directory and execute `smaps_capture.py`.

```bash
python3 smaps_capture.py [OPTIONS]
```

### Command-line Arguments

*   `-p`, `--process_name` (type: `str`, default: `com.google.android.youtube.tv`)
    The name of the Android process to capture smaps data from.
*   `-i`, `--interval_minutes` (type: `int`, default: `2`)
    The interval in minutes between each smaps capture.
*   `-d`, `--capture_duration_seconds` (type: `int`, default: `10800` (3 hours))
    The total duration in seconds for the entire capture session. The script will exit after this duration.
*   `-o`, `--output_dir` (type: `str`, default: `cobalt_smaps_logs`)
    The directory where the captured smaps log files will be saved.
*   `-s`, `--device_serial` (type: `str`, default: `localhost:45299`)
    The serial number of the Android device or emulator to connect to. Set to `None` if only one device is connected.
*   `--adb_path` (type: `str`, default: `adb`)
    The absolute path to the `adb` executable.

### Examples

1.  **Run with default settings:**
    ```bash
    python3 smaps_capture.py
    ```

2.  **Capture a different process every 5 minutes for 1 hour:**
    ```bash
    python3 smaps_capture.py -p com.example.my_app -i 5 -d 3600
    ```

3.  **Specify a different output directory and device serial:**
    ```bash
    python3 smaps_capture.py -o /tmp/my_smaps_logs -s R58M1293QYV
    ```

## Smaps Analysis and Batch Processing

This directory also contains scripts for analyzing the captured smaps data.

### `read_smaps.py`

This is a single-file analysis script that reads a smaps file and prints a summarized, human-readable output to the console.

### `read_smaps_batch.py`

This script provides a convenient way to process multiple smaps files at once. It reads one or more input files, processes them, and saves the summarized output to a specified directory, with each output file corresponding to its input.

#### Usage

```bash
python3 read_smaps_batch.py <SMAPS_FILES...> [OPTIONS]
```

#### Command-line Arguments

*   `<SMAPS_FILES...>` (required positional argument)
    One or more paths to the smaps files to be analyzed. Wildcards can be used (e.g., `my_logs/*.txt`).
*   `-o`, `--output_dir` (type: `str`, default: `processed_smaps`)
    The directory where the processed smaps log files will be saved.
*   `-k`, `--sortkey` (type: `str`, choices: `size`, `rss`, `pss`, `anonymous`, `name`, default: `pss`)
    The key to sort the output by.
*   `-s`, `--strip_paths` (action: `store_true`)
    Remove leading paths from binary names.
*   ... (and other analysis options from `read_smaps.py`)

#### Examples

1.  **Process all smaps files in a directory and save to `processed_logs`:**
    ```bash
    python3 read_smaps_batch.py cobalt_smaps_logs/*.txt -o processed_logs
    ```

2.  **Process specific files with aggregation and sorting:**
    ```bash
    python3 read_smaps_batch.py file1.txt file2.txt -a -s -k pss
    ```

### `analyze_smaps_logs.py`

After processing a batch of smaps files, you can use this script to analyze the entire run. It reads a directory of processed smaps files, tracks memory usage over time, and generates a summary report.

The report includes:
*   The top 5 largest memory consumers by PSS and RSS at the end of the run.
*   The top 5 memory regions that have grown the most in PSS and RSS over the duration of the run.
*   The overall change in total PSS and RSS.

#### Usage

```bash
python3 analyze_smaps_logs.py <PROCESSED_LOG_DIR>
```

#### Example

```bash
python3 analyze_smaps_logs.py processed_logs
```

## Testing

Unit tests are provided to ensure the functionality of the scripts. To run the tests, navigate to the project root directory and execute the following command:

```bash
# For smaps_capture.py
python3 -m unittest cobalt/tools/performance/smaps/smaps_capture_test.py

# For read_smaps_batch.py
python3 -m unittest cobalt/tools/performance/smaps/read_smaps_test.py

# For analyze_smaps_logs.py
python3 -m unittest cobalt/tools/performance/smaps/analyze_smaps_logs_test.py
```
