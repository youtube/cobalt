# Cobalt Performance Test Tooling

This enables performance monitoring via ADB commands and dumpsys meminfo
and plots the results using matplotlib.

## Android Monitoring Script

### Setup

This script requires a few dependencies - pandas, matplotlib, numpy, and
pylint.

To install them, run the following command:

```
sudo apt install python3-pandas python3-numpy python3-matplotlib
```

### Usage

The monitoring script has various args that you can use to customize a few different parameters:

  * package  - Specify the Cobalt package name to test against
  * activity - Specify the Cobalt activity name to test against
  * url      - Specify a Youtube video URL to test against
  * output   - Specify an output format for the monitoring data. Can
               be one of ['csv', 'plot', 'both'].
  * interval - Specify the polling interval frequency for collecting data
  * outdir   - Specify the output directory to place both the output
               data and the resulting graphs.
  * flags    - Specify the command line & experiment flags to provide to Cobalt
  * skip-restart - Do not restart the active Cobalt process
  * help     - Display a list of command options and the script's usage

It defaults to a test YouTube video and sampling rate. So, you will
need to specify those if you want something specific.

Below is a simple usage command:

```
python3 $HOME/chromium/src/cobalt/tools/performance/android_monitor.py
```

**Tip:** To avoid the script from placing the resulting plots and csv files into
you local checkout, utilize the `--outdir` flag to specify somewhere outside of
your checkout.

### Testing

In order to execute Python unit tests, the command below will use the python unittest
module to run any tests named after a specific pattern:

```
python3 -m unittest discover -s </path/to/test/directory> -p '*TEST PATTERN*.py'
```


### Pylint

Like all other python scripts in open source, they should follow the
guidance set by https://google.github.io/styleguide/pyguide. A nice way
to ensure conformance to this is to run the pylinter provided by
`depot_tools/pylint_main.py`.

**Note:** Use of the pylint script is done via `python3`.

#### Usage

```
python3 $HOME/chromium/tools/depot_tools/pylint_main.py $HOME/chromium/src/cobalt/tools/performance/android_monitor.py
```

### Troubleshooting

#### Module pylint not found

Installations of python3 and vpython3 may not have pylint installed:

```
sudo apt install pylint
```

#### Original error was: No module named 'numpy.core._multiarray_umath'

Run `pip install numpy --upgrade` to resolve the error

## ADB Commands to load test sites

  * To load test sites

    ```
    adb shell am start --esa \
      commandLineArgs '--remote-allow-origins=*,--url="<NAME OF TEST SITE>"' \
      dev.cobalt.coat
    ```

  * To kill Cobalt

    ```
    adb shell am force-stop dev.cobalt.coat
    ```

## RDK Performance Benchmark Tooling

The in-tree RDK performance benchmarking tools provide automated end-to-end performance measurement and differential regression analysis for physical RDK devices.

The toolsuite consists of two complementary scripts:
1. `run_rdk_perf_benchmark.py`: Deploys/launches Cobalt on RDK, establishes a WebSocket connection to the Chrome DevTools Protocol (CDP) port (9222), executes deterministic CUJ workloads, and records high-resolution timeseries performance metrics.
2. `compare_perf_metrics.py`: Analyzes baseline vs. treatment CSV metrics across 8 differential performance gates (main thread CPU, peak load, UI compositor FPS, video frame drops, V8 script execution, layout reflow, scheduling jitter, and memory footprint).

### Design & Architecture Document

For comprehensive architectural specifications, mathematical foundations, data source provenance, and telemetry instrumentation details, see:
* [Cobalt Local RDK Performance Benchmark Design](https://docs.google.com/document/d/1roTA0ytMx46ctHY6Ql96RSF-TNp0FKkyyn0zOxzBRRs/edit?resourcekey=0-X-K-vMkbLoPdcmmQfxC0EQ&tab=t.0#heading=h.7ilxptasrppw)

### Limitations & Disclaimer

> **Disclaimer:** This local benchmarking framework is a best-effort approach for measuring performance impact on local desk devices during development and pre-commit verification. It represents a pragmatic shortcut while Crossbench integration for Cobalt is being planned and developed.
>
> **Source of Truth:** Production Finch experiments remain the ultimate source of truth for all user-facing performance and regression impact in the wild.

Key limitations to consider when interpreting results:
* **Network & Ad Stream Variance**: Live CDN streaming, adaptive bitrate transitions, and dynamic pre-roll ad delivery introduce environmental variance across test runs. While `run_rdk_perf_benchmark.py` includes multi-vector ad skip automation (DOM click, D-pad Enter, CDP mouse click) once the skip button is interactive, completely eliminating pre-roll ad variance is typically only possible in lab environments with ad-suppressed accounts or mock networks.
* **Warmup Transients**: Initial page bootstrap, V8 JIT compilation, and media decoder buffer fill generate transient spikes. Always use `--warmup-sec` (default: 15s) in `compare_perf_metrics.py` and run tests for at least 180s to measure stable, steady-state performance.
* **Hardware & Thermal Environment**: A single physical device cannot represent the full distribution of production devices, ambient thermal variations, or vendor-specific background daemons.

### Captured Performance Dimensions

* **Compositor Cadence & Smoothness**: Real-time VSync `requestAnimationFrame` frame counters, jank detection (>25ms intervals), and Chromium UMA compositor dropped frame percentages (`Graphics.Smoothness.PercentDroppedFrames3.AllSequences`).
* **W3C Long Tasks**: Stalls $\ge$ 50ms and severe freezes $\ge$ 150ms observed via in-engine `PerformanceObserver`.
* **Blink Scheduler Task Load**: Cumulative task duration, script execution time, and layout/style recalculation times via CDP `Performance.getMetrics`.
* **Hardware Video Playback QoS**: Total decoded frames, dropped video frames, and corrupted frames queried via `HTMLVideoElement.getVideoPlaybackQuality()`.
* **Memory Footprint**: Process private dirty memory footprint (`Memory.Browser.PrivateMemoryFootprint.Live` / `ResidentSet.Live`) and V8 heap sizes.

### Usage: `run_rdk_perf_benchmark.py`

#### Prerequisites

* Target RDK device reachable via SSH/deploy script (`starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts/deploy_rdk.py`).
* Python 3 with `websocket-client` (falls back automatically to in-tree `third_party/catapult/telemetry/third_party/websocket-client` if not installed).

#### Command Options

* `--output <path>`: (Required) Path where timeseries metrics CSV will be saved.
* `--workload <watch|watch_browse>`:
  * `watch` (Steady Video Playback CUJ, default): Launches reference video (`v=1La4QzGeaaQ`), automatically skips pre-roll ads once the skip button becomes interactive, and maintains continuous video playback to measure hardware decoder QoS (`droppedVideoFrames`), compositor VSync cadence, and main thread load.
  * `watch_browse` (Watch & Recommendations Browse CUJ): Plays video steadily for 15s, moves focus off the media player scrubber into the recommendation shelf (`3x ArrowDown`), and smoothly scrolls recommendations (`ArrowDown` every 2s) while video decodes in the background to evaluate GPU rasterization, compositor smoothness, and W3C long-task hitches under interaction.
* `--duration-sec <seconds>`: Duration of the benchmark run (default: 180s).
* `--deeplink <url_or_v>`: Video deeplink for deterministic CUJ (defaults to official YouTube TV certification stream: `v=1La4QzGeaaQ`).
* `--enable-features <features>`: Comma-separated Chrome/Cobalt features to enable.
* `--disable-features <features>`: Comma-separated Chrome/Cobalt features to disable.
* `--extra-params <params>`: Additional command-line flags forwarded to Cobalt.
* `--skip-deploy`: Skip rebuilding/flashing if binaries are already present on the RDK device.
* `--skip-launch`: Connect to an already running Cobalt instance on port 9222 without restarting it.

#### Examples

1. **Standard 180-second watch benchmark:**
   ```bash
   python3 cobalt/tools/performance/run_rdk_perf_benchmark.py \
     --skip-deploy \
     --duration-sec 180 \
     --output baseline_rdk_perf.csv
   ```

2. **Watch + browse workload with custom feature flags:**
   ```bash
   python3 cobalt/tools/performance/run_rdk_perf_benchmark.py \
     --skip-deploy \
     --workload watch_browse \
     --duration-sec 120 \
     --enable-features MyFeatureName \
     --extra-params --v=1 \
     --output treatment_rdk_perf.csv
   ```

3. **Benchmarking an already-running instance (desk profiling):**
   ```bash
   python3 cobalt/tools/performance/run_rdk_perf_benchmark.py \
     --skip-launch \
     --duration-sec 60 \
     --output live_metrics.csv
   ```

### Usage: `compare_perf_metrics.py`

Use `compare_perf_metrics.py` to perform automated differential regression analysis between baseline and treatment CSV files:

```bash
python3 cobalt/tools/performance/compare_perf_metrics.py \
  --baseline baseline_rdk_perf.csv \
  --treatment treatment_rdk_perf.csv \
  --output regression_report.md
```

The script evaluates regressions against configurable thresholds (`--max-cpu-increase-percent`, `--max-fps-drop-percent`, `--max-video-drop-increase-percent`, etc.), exits with code `0` on PASS or `1` on FAIL, and prints a formatted Markdown summary table.
