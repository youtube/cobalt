# Cobalt Memory Metrics

Cobalt integrates with Chromium's Memory Infra and UMA (User Metrics Analysis) to provide detailed insights into memory usage across various subsystems. This documentation describes the available memory histograms and how they are collected.

## Memory Histograms

The following histograms are recorded periodically (every 5 minutes by default) and reported via the telemetry system.

### Subsystem Breakdown

These metrics provide a breakdown of memory usage within the Cobalt process:

*   **`Cobalt.Memory.JavaScript`**: Memory used by the V8 JavaScript heap (in MB).
*   **`Cobalt.Memory.DOM`**: Memory consumed by DOM nodes and related structures (in MB).
*   **`Cobalt.Memory.Layout`**: Memory for layout trees and rendering data (in MB).
*   **`Cobalt.Memory.Graphics`**: Memory used by Skia, GPU resources, textures, etc. (in MB).
*   **`Cobalt.Memory.Media`**: Buffers and resources used by the media pipeline (in MB).
*   **`Cobalt.Memory.Native`**: Other general native heap allocations (in MB).

### Growth and Object Counts

*   **`Cobalt.Memory.PrivateMemoryFootprint.GrowthRate.{LeakType}`**: The rate at which the private memory footprint is growing (in KB/min).
    *   Variants:
        *   `Slow`: High resolution for growth up to 10 MB/min.
        *   `Fast`: Captures growth up to 1 GB/min.
*   **`Cobalt.Memory.ObjectCounts.{Component}`**: The number of live objects for specific components in Blink.
    *   Variants: `Document`, `JSEventListener`, `LayoutObject`, `Node`.

### Resident Memory (Platform-level)

These metrics capture the Resident Set Size (RSS) as reported by the operating system:

*   **`Memory.Total.Resident`**: Total resident memory used by the process and its children.
*   **`Memory.Browser.Resident`**: Resident memory used by the browser process.
*   **`Memory.Renderer.Resident`**: Resident memory used by the renderer process/thread.
*   **`Memory.Gpu.Resident`**: Resident memory used by the GPU process/thread.

## Collection Mechanism

Memory metrics are collected by the `CobaltMetricsServiceClient`. It utilizes `memory_instrumentation::MemoryInstrumentation` to capture global memory dumps and extract aggregated data.

The collection interval can be controlled via the `--memory-metrics-interval` command-line switch (value in seconds).

## Local Verification

You can use the following tools in `cobalt/tools/uma/` to verify memory metrics locally:

1.  **`pull_uma_histogram_set_via_cdp.py`**: Connects to a running Cobalt instance via CDP and polls for histogram values.
2.  **`interpret_uma_histogram.py`**: Analyzes the data collected by the puller script and calculates statistics (percentiles).

Example usage:
```bash
vpython3 cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py --platform linux --poll-interval-s 10
```
