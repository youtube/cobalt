# UMA Tools for Cobalt

This directory contains Python scripts for interacting with and analyzing Cobalt's UMA histograms via the Chrome DevTools Protocol (CDP).

---

**Note:** These tools have been tested on Android and Linux, but the  Linux builds have only been tested on Linux monolithic builds, not RDK builds. (TODO: b/470381238)

## 1. `pull_uma_histogram_set_via_cdp.py`

A script to connect to a running Cobalt instance, poll for UMA histograms at a regular interval, and save the raw data.

### Features

*   **Connect to a running Cobalt instance:** The script can connect to a Cobalt instance with remote debugging enabled.
*   **Automatic Cobalt management:** The script can automatically launch and stop the Cobalt application on a connected Android device.
*   **Custom histogram sets:** The script can query for a default set of histograms, or a custom set provided via a text file.
*   **Flexible configuration:** The script can be configured with command-line arguments to control its behavior.

### Prerequisites

The script is designed to be run with `vpython3`, which manages the Python environment and dependencies. Ensure that `vpython3` is available in your shell.

### Usage

```bash
vpython3 pull_uma_histogram_set_via_cdp.py [OPTIONS]
```

#### Command-line Arguments

*   `--histogram-file` (type: `str`)
    Path to a text file containing a list of histograms to query, one per line.
*   `--no-manage-cobalt` (action: `store_true`)
    If set, the script will not attempt to start or stop the Cobalt process.
    This is the default and only supported behavior on Linux.
*   `--package-name` (type: `str`, default: `dev.cobalt.coat`)
    (Android only) The package name of the Cobalt application.
*   `--platform` (choices: `android`, `linux`, default: `android`)
    The platform the script is targeting.
*   `--poll-interval-s` (type: `float`, default: `30.0`)
    The polling frequency in seconds.
*   `--port` (type: `int`, default: `9222`)
    The remote debugging port that Cobalt is listening on.
*   `--output-file` (type: `str`)
    Path to a file to log the histogram data to. The data will be saved in a
    CSV-like format with the timestamp, histogram name, and JSON data.
*   `--url` (type: `str`)
    (Android only) The target URL for Cobalt to navigate to on launch.
*   `-q`, `--quiet` (action: `store_true`)
    If set, suppresses all non-essential print output. Useful for cleaner logs
    or for running in automated scripts.

### Verifying Cobalt Memory Breakdown Metrics via CDP

Cobalt integrates real-time live memory metrics, session median (P50) breakdown histograms, and time-windowed peak guardrails directly into the Chrome DevTools Protocol (`Performance.getMetrics`).

When `pull_uma_histogram_set_via_cdp.py` runs, it issues `Performance.enable` and `Performance.getMetrics` on every polling cycle, printing the full dual-stream memory metrics without requiring any external metric list files.

#### Step 1: Launch Cobalt with Remote Debugging

* **On Linux (Evergreen x64 devel)**:
  ```bash
  ./out/evergreen-x64_devel/loader_app \
      --remote-debugging-port=9222 \
      --remote-allow-origins=* \
      --url="https://www.youtube.com/tv"
  ```
  *(Tip: To accelerate background UMA memory dumps for local testing, add `--enable-features=CobaltMetricsInterval:memory-metrics-interval/10`)*.

* **On Android / RDK Device (via ADB)**:
  ```bash
  adb shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity \
      --esa commandLineArgs '--remote-debugging-port=9222,--remote-allow-origins=*'
  ```

#### Step 2: Run the Verification Script

* **Targeting Linux**:
  ```bash
  vpython3 cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py \
      --platform=linux \
      --port=9222 \
      --poll-interval-s=2
  ```

* **Targeting Android**:
  ```bash
  vpython3 cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py \
      --platform=android \
      --package-name=dev.cobalt.coat \
      --poll-interval-s=5
  ```

#### Step 3: Expected Output

On each polling iteration, the script will output the Performance metrics block containing both **Live instantaneous** memory values and **Session P50 / Peak** guardrails:

```text
Connected to WebSocket: ws://127.0.0.1:9222/devtools/page/...
Enabled Performance domain (ID: 1)

Performance Metrics for https://www.youtube.com/tv:
  JSHeapUsedSize: 14581348
  JSHeapTotalSize: 16998400

  === Live Real-Time Subsystem Metrics ===
  Memory.Browser.ResidentSet.Live: 471416832            (449.58 MB)
  Memory.Browser.PrivateMemoryFootprint.Live: 359141376 (342.50 MB)
  Memory.Experimental.Browser2.V8.Live: 14649192       ( 13.97 MB)
  Memory.Experimental.Browser2.Stacks.Live: 1073152     (  1.02 MB)

  === Session Median (P50) & Guardrail Metrics ===
  Memory.GPU.PeakMemoryUsage2.PageLoad.P50: 17825792    ( 17.00 MB)
  Memory.Experimental.Renderer.HighestPrivateMemoryFootprint.0to2min: 157286400 (150.00 MB)
```

> **Platform Note**:
> * `*.Live` metrics read `/proc/self/statm` directly via `base::ProcessMetrics` and populate immediately across all Linux and Android platforms without elevated privileges.
> * UMA background dumps (`CobaltMemoryMetricsEmitter`) rely on `/proc/self/pagemap`, which is accessible on production Android/RDK hardware. On Linux workstations, `CAP_SYS_ADMIN` restrictions prevent background `/proc/self/pagemap` parsing; deterministic testing of P50 histograms on desktop can be verified via `blink_unittests --gtest_filter="CobaltMemoryMetricsHelperTest.*"`.

### Troubleshooting

#### Handshake status 403 Forbidden

If you see an error similar to this:
`An error occurred: Handshake status 403 Forbidden ... Rejected an incoming WebSocket connection`

This means the running Cobalt instance is rejecting the connection from the script. To fix this, you must launch Cobalt with the correct flags to enable the remote debugging port and allow connections from remote origins. The method for this differs by platform.

**For Linux:**

Launch Cobalt directly from your shell, adding the `--remote-debugging-port` and `--remote-allow-origins` flags.

```bash
# Example for Linux
path/to/cobalt --remote-debugging-port=9222 --remote-allow-origins=*
```

**For Android:**

On Android, these flags are passed as extras to the `am start` command. If you are launching Cobalt manually via `adb`, you would include them in the `--esa commandLineArgs` argument. Ensure you include both flags.

```bash
# Example for launching Cobalt manually on Android
adb shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity \
  --esa commandLineArgs '--remote-debugging-port=9222,--remote-allow-origins=*'
```

Note: If the script is managing the Cobalt process for you (i.e., you are *not* using `--no-manage-cobalt`), it automatically adds `--remote-allow-origins=*`. However, it assumes remote debugging is already enabled in your Android build on port 9222. If it is not, you may need to launch Cobalt manually using the command above.

#### ImportError or AttributeError with `websocket`

The script is intended to be run with `vpython3`, which ensures a consistent environment with the correct dependencies, like `websocket-client`. If you encounter import errors related to the `websocket` module, ensure you are using `vpython3`. Running with the system `python3` may lead to conflicts if you have other websocket-related libraries installed.

---

## 2. `interpret_uma_histogram.py`

A script to read the data file generated by the puller script, calculate key statistical percentiles for each histogram, and optionally visualize the results.

### Features

*   **Percentile Analysis:** Calculates the 25th, 50th, 75th, 95th, and 99th percentiles to summarize the distribution of histogram data.
*   **Data Filtering:** Skips any histogram that contains no data (i.e., where the total `count` of values is zero) to ensure that the analysis and visualizations are meaningful.
*   **Clear Console Output:** Prints a human-readable summary for each histogram at each timestamp.
*   **Data Visualization:** Can generate and save line graphs plotting the percentiles over time for each histogram.

### Prerequisites

For visualization, the `matplotlib` library is required. You can install it via pip:
```bash
pip install matplotlib
```

### Usage

```bash
python3 interpret_uma_histogram.py [INPUT_FILE] [OPTIONS]
```

#### Arguments

*   `input_file` (positional)
    The path to the UMA histogram data file generated by the puller script (e.g., `test-uma-out.txt`).
*   `--visualize` (action: `store_true`)
    If set, the script will generate and save PNG plots of the percentile data over time. A separate plot is created for each histogram.

### Example

```bash
# Print the percentile analysis to the console
python3 interpret_uma_histogram.py test-uma-out.txt

# Print the analysis and also generate plots
python3 interpret_uma_histogram.py test-uma-out.txt --visualize
```

---

## How Percentiles Are Calculated

Since the raw data from Cobalt does not contain every individual data point, but rather groups them into **buckets**, the percentiles are calculated by identifying which bucket a given percentile falls into.

For example, the **75th percentile** is the value that is greater than or equal to 75% of all the data points in the histogram.

The script performs the following steps:

1.  **Find the "Rank" of the Percentile:** It determines which data point corresponds to the percentile by multiplying the total count of values in the histogram by the percentile.
    *   **Formula:** `rank = total_count * (percentile / 100.0)`
    *   **Example:** For a histogram with 791 values, the rank of the 75th percentile is `791 * 0.75 = 593.25`. This means we are looking for the ~593rd data point in an ordered list.

2.  **Walk Through Buckets Cumulatively:** The script iterates through the histogram's buckets in order, adding up the `count` of values in each bucket until the cumulative count meets or exceeds the calculated rank.

3.  **Select the Value from the Bucket:** Once the target bucket is found, the script uses that bucket's `high` value as the percentile value. This provides a strong approximation of the true percentile.
