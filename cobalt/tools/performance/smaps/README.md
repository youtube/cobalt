# SMAPS Capture and Analysis Toolchain

This directory contains a suite of Python scripts for capturing, processing, and analyzing memory usage data (specifically, `/proc/<pid>/smaps` content) for a specified Android or Linux process. These tools are useful for long-term memory profiling to identify potential memory leaks by tracking memory growth over time and identifying the largest consumers.

## Features

*   **Cross-platform:** Supports capturing smaps from both Android (via ADB) and local Linux processes.
*   **Smart Process Selection:** On Linux, if multiple processes with the same name are found, the script identifies the main process (the one with the longest uptime) and captures its smaps data.
*   **Periodic Capture:** Captures smaps data at a configurable interval.
*   **Duration Control:** Runs for a specified total duration and then exits.
*   **Configurable:** Allows customization of the target process, capture interval, duration, and output directory.
*   **Testable:** The script is refactored into a class for easy testing and mocking of system calls.

## Smaps Capture Usage

To run the script, navigate to the `cobalt/tools/performance/smaps/` directory and execute `smaps_capture.py`.

```bash
python3 smaps_capture.py [OPTIONS]
```

### Command-line Arguments

*   `--platform` (type: `str`, choices: `android`, `linux`, default: `android`)
    The platform to capture from.
*   `-p`, `--process_name` (type: `str`, default: `com.google.android.youtube.tv`)
    The name of the process to capture smaps data from.
*   `-i`, `--interval_minutes` (type: `int`, default: `2`)
    The interval in minutes between each smaps capture.
*   `-d`, `--capture_duration_seconds` (type: `int`, default: `10800` (3 hours))
    The total duration in seconds for the entire capture session. The script will exit after this duration.
*   `-o`, `--output_dir` (type: `str`, default: `cobalt_smaps_logs`)
    The directory where the captured smaps log files will be saved.
*   `-s`, `--device_serial` (type: `str`, default: `localhost:45299`)
    (Android only) The serial number of the Android device or emulator to connect to. Set to `None` if only one device is connected.
*   `--adb_path` (type: `str`, default: `adb`)
    (Android only) The absolute path to the `adb` executable.

### Examples

1.  **Run with default settings (capturing from an Android device):**
    ```bash
    python3 smaps_capture.py
    ```

2.  **Capture a different Android process every 5 minutes for 1 hour:**
    ```bash
    python3 smaps_capture.py -p com.example.my_app -i 5 -d 3600
    ```

3.  **Capture a local Linux process for 10 minutes:**
    ```bash
    python3 smaps_capture.py --platform linux -p chrome -i 1 -d 600 -o chrome_smaps_logs
    ```

4.  **Specify a different output directory and Android device serial:**
    ```bash
    python3 smaps_capture.py -o /tmp/my_smaps_logs -s R58M1293QYV
    ```

## Unified Analysis Pipeline

To simplify the analysis process, the `run_analysis_pipeline.py` script combines the batch processing, analysis, and visualization steps into a single command.

### `run_analysis_pipeline.py`

This script takes a directory of raw smaps logs and generates the final visualization PNG, handling all intermediate steps automatically.

#### Usage

```bash
python3 run_analysis_pipeline.py <RAW_LOG_DIR> [OPTIONS]
```

#### Command-line Arguments

*   `<RAW_LOG_DIR>` (required positional argument)
    The path to the directory containing the raw smaps log files.
*   `--output_image` (type: `str`, default: `smaps_analysis.png`)
    The path where the final output PNG image will be saved.
*   `--platform` (type: `str`, choices: `android`, `linux`, default: `android`)
    Specify the platform for platform-specific aggregations.

#### Example

```bash
python3 run_analysis_pipeline.py cobalt_smaps_logs --output_image my_analysis.png --platform android
```

## Smaps Analysis and Batch Processing

This directory also contains scripts for analyzing the captured smaps data.

### `read_smaps.py`

This is a single-file analysis script that reads a smaps file and prints a summarized, human-readable output to the console.

#### Usage

```bash
python3 read_smaps.py <SMAPS_FILE> [OPTIONS]
```

#### Command-line Arguments

*   `<SMAPS_FILE>` (required positional argument)
    The path to the smaps file to be analyzed (e.g., `cobalt_smaps_logs/smaps_20250101_120000_12345.txt`).
*   `-k`, `--sortkey` (type: `str`, choices: `size`, `rss`, `pss`, `anonymous`, `name`, default: `pss`)
    The key to sort the output by.
*   `-s`, `--strip_paths` (action: `store_true`)
    Remove leading paths from binary names.
*   `-r`, `--remove_so_versions` (action: `store_true`)
    Remove dynamic library versions (e.g., `.so.1.2.3` becomes `.so`).
*   `-a`, `--aggregate_solibs` (action: `store_true`)
    Collapse shared libraries into single rows (e.g., `libc.so`, `libstdc++.so`).
*   `-d`, `--aggregate_android` (action: `store_true`)
    Consolidate various Android-specific allocations.
*   `-z`, `--aggregate_zeros` (action: `store_true`)
    Consolidate rows that show zero RSS and PSS.
*   `--no_anonhuge` (action: `store_true`)
    Omit the `AnonHugePages` column from the output.
*   `--no_shr_dirty` (action: `store_true`)
    Omit the `Shared_Dirty` column from the output.

#### Examples

1.  **Analyze a saved smaps file, sorted by PSS, stripping paths and aggregating shared libraries:**
    ```bash
    python3 read_smaps.py cobalt_smaps_logs/smaps_20250101_120000_12345.txt -k pss -s -a
    ```

2.  **Analyze a smaps file with Android-specific aggregation:**
    ```bash
    python3 read_smaps.py my_app_smaps.txt -d
    ```

### `read_smaps_batch.py`

This script provides a convenient way to process multiple smaps files at once. It reads all `.txt` files from a specified input directory, processes them, and saves the summarized output to a specified output directory, with each output file corresponding to its input.

#### Usage

```bash
python3 read_smaps_batch.py <RAW_LOGS_DIR> [OPTIONS]
```

#### Command-line Arguments

*   `<RAW_LOGS_DIR>` (required positional argument)
    The path to the directory containing the raw smaps log files.
*   `-o`, `--output_dir` (type: `str`, default: `processed_smaps`)
    The directory where the processed smaps log files will be saved.
*   **All arguments from `read_smaps.py`** are also available for fine-tuning the analysis of each individual smaps file.

#### Examples

1.  **Process all smaps files in a directory and save to `processed_logs`:**
    ```bash
    python3 read_smaps_batch.py cobalt_smaps_logs -o processed_logs
    ```

2.  **Process files with aggregation and sorting:**
    ```bash
    python3 read_smaps_batch.py my_logs_dir -a -s -k pss
    ```

### `analyze_smaps_logs.py`

After processing a batch of smaps files, you can use this script to inspect the final memory state of the run. It reads a directory of processed smaps files and prints a detailed, non-aggregated memory breakdown from the last log file in the time series.

The primary purpose of this script is to either provide a snapshot of the final memory layout or to generate a JSON file containing the full time-series data. This JSON output is essential for the `visualize_smaps_analysis.py` script, which handles aggregation and visualization.

The script can also output a structured JSON file containing the time-series data for further analysis or visualization.

#### Usage

```bash
python3 analyze_smaps_logs.py <PROCESSED_LOG_DIR> [OPTIONS]
```

#### Command-line Arguments

*   `<PROCESSED_LOG_DIR>` (required positional argument)
    The path to the directory containing the processed smaps log files.
*   `--json_output` (type: `str`)
    Optional: The path to a file where the JSON analysis output will be saved. This is required for visualization.

#### Examples

1.  **Print a detailed memory breakdown to the console:**
    ```bash
    python3 analyze_smaps_logs.py processed_logs
    ```

2.  **Generate a JSON output file for visualization:**
    ```bash
    python3 analyze_smaps_logs.py processed_logs --json_output analysis_output.json
    ```

### `visualize_smaps_analysis.py`

This script takes the JSON output from `analyze_smaps_logs.py` and generates a dashboard-style PNG image with three plots:
1.  Total PSS and RSS memory usage over time.
2.  A stacked area chart of the top 10 memory consumers.
3.  A line chart of the top 10 memory growers.

This provides a quick and intuitive way to visualize memory behavior and identify potential leaks.

#### Prerequisites

This script requires the `pandas` and `matplotlib` libraries. You can install them using pip:
```bash
pip install pandas matplotlib
```

#### Usage

```bash
python3 visualize_smaps_analysis.py <JSON_FILE> [OPTIONS]
```

#### Command-line Arguments

*   `<JSON_FILE>` (required positional argument)
    The path to the input JSON file generated by `analyze_smaps_logs.py`.
*   `--output_image` (type: `str`, default: `smaps_analysis.png`)
    The path where the output PNG image will be saved.

#### Example

```bash
python3 visualize_smaps_analysis.py analysis_output.json --output_image my_analysis.png
```

## Extending the Toolchain with New Fields

The toolchain is designed to be extensible, allowing you to add new fields from the raw smaps files to the analysis and visualization. The key is to follow the data pipeline from the processor (`read_smaps.py`) to the analyzer (`analyze_smaps_logs.py`) and visualizer (`visualize_smaps_analysis.py`).

Here is a step-by-step guide using the `Locked` field as an example.

### Step 1: Update the Processor (`read_smaps.py`)

This is the most critical step to get the new data into the processed files.

1.  **Add the new field to the `fields` tuple:**
    In `cobalt/tools/performance/smaps/read_smaps.py`, add your new field (in lowercase) to the `fields` string.

    ```python
    # --- BEFORE ---
    # fields = ('size rss pss ... swap swap_pss').split()

    # --- AFTER ---
    fields = ('size rss pss shr_clean shr_dirty priv_clean priv_dirty '
              'referenced anonymous anonhuge swap swap_pss locked').split()
    MemDetail = namedtuple('name', fields)
    ```

2.  **Update the `MemDetail` creation:**
    The `parse_smaps_entry` function automatically parses all fields into a dictionary. You just need to use the new field when creating the `MemDetail` object.

    ```python
    # --- BEFORE ---
    # d = MemDetail(..., data['swap'], data['swappss'])

    # --- AFTER ---
    d = MemDetail(
        data['size'], data['rss'], data['pss'], data['sharedclean'],
        data['shareddirty'], data['privateclean'], data['privatedirty'],
        data['referenced'], data['anonymous'], data['anonhugepages'],
        data['swap'], data['swappss'], data['locked'])
    ```

After these two changes, re-running `read_smaps_batch.py` will produce processed files that include a `locked` column.

### Step 2: (Optional) Use the Field in the Analyzer (`analyze_smaps_logs.py`)

The analyzer will now have access to the `locked` data. To display it, you can add it to the text report. For example, to show the total change in locked memory:

```python
# In analyze_logs in analyze_smaps_logs.py

# Add this block to the "Overall Total Memory Change" section
if 'locked' in total_history and len(total_history['locked']) > 1:
  total_locked_change = total_history['locked'][-1] - total_history['locked'][0]
  print(f'  Total Locked Change: {total_locked_change} kB')
```

### Step 3: (Optional) Use the Field in the Visualizer (`visualize_smaps_analysis.py`)

To add the new field to the graph, you need to:

1.  **Add the field to the JSON output in `analyze_smaps_logs.py`:**
    ```python
    # In the 'regions' list comprehension inside analyze_logs:
    'regions': [{
        'name': name,
        'pss': data.get('pss', 0),
        'rss': data.get('rss', 0),
        'locked': data.get('locked', 0)  # Add this line
    } for name, data in memory_data.items()]
    ```

2.  **Add a new plot in `visualize_smaps_analysis.py`:**
    For example, to add a line for "Total Locked" memory to the first chart:
    ```python
    # In create_visualization in visualize_smaps_analysis.py:
    total_locked = [d['total_memory'].get('locked', 0) for d in data]
    ax1.plot(timestamps, total_locked, label='Total Locked', color='green')
    ```

By following this pattern, you can incorporate any field from the raw smaps files into the entire toolchain.

## Testing

Unit tests are provided to ensure the functionality of the scripts. To run the tests, navigate to the project root directory and execute the following commands. Note that `__init__.py` handles Python path setup, so tests should always be run from the project root.

```bash
# For smaps_capture.py
python3 -m unittest cobalt/tools/performance/smaps/smaps_capture_test.py

# For read_smaps_batch.py
python3 -m unittest cobalt/tools/performance/smaps/read_smaps_test.py

# For analyze_smaps_logs.py
python3 -m unittest cobalt/tools/performance/smaps/analyze_smaps_logs_test.py

# For visualize_smaps_analysis.py
python3 -m unittest cobalt/tools/performance/smaps/visualize_smaps_analysis_test.py

# For the unified pipeline
python3 -m unittest cobalt/tools/performance/smaps/run_analysis_pipeline_test.py
```
