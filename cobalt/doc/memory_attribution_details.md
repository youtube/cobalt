# Cobalt Memory Attribution: Implementation Details

This document provides a detailed breakdown of the metrics and infrastructure added to achieve the >80% RSS attribution goal during 4K video playback.

## 1. New Memory Allocator Metrics
Custom `MemoryDumpProvider`s have been implemented to register with Chromium's `MemoryDumpManager`. These metrics appear in the "allocator dumps" section of the global memory dump.

### Media Stack (`cobalt/media`)
*   **`cobalt/media/video_buffers`**: Tracks the total size (in bytes) of all encoded video bitstream buffers currently held in memory.
*   **`cobalt/media/audio_buffers`**: Tracks the total size (in bytes) of all encoded audio samples in the pipeline.
*   **`cobalt/media/decoded_frames`**: Tracks the estimated memory used by decoded video frames. This is a critical metric for 4K playback, as raw 4K frames are very large.

### Renderer & Rasterizer (`cobalt/renderer`)
*   **`cobalt/renderer/rasterizer/resource_cache`**: Dynamically queries the **Skia `GrDirectContext`** to report the actual byte usage of the graphics resource cache (glyph caches, image caches, etc.).
*   **`cobalt/renderer/rasterizer/vertex_buffers`**: Tracks memory allocated for geometry data.
*   **`cobalt/renderer/rasterizer/index_buffers`**: Tracks memory for vertex indices.
*   **`cobalt/renderer/rasterizer/command_buffers`**: Tracks memory overhead used to store graphics commands before they are sent to the GPU.

---

## 2. Enhanced Starboard GPU Reporting
GPU memory reporting has been implemented for key platforms to capture "extra-process" memory that is not captured by standard process-local tracking.

*   **Linux (DMA-BUF Attribution)**: Implemented a parser for `/sys/kernel/debug/dma_buf/bufinfo`. It identifies DMA-BUFs (commonly used for hardware video decoding and zero-copy textures) associated with Cobalt's PID and attributes their size to the GPU bucket.
*   **Android (System Query)**: Enhanced the `StarboardBridge` Java-to-Native layer to query system-level memory metrics that capture ION/GPU memory usage not normally visible to the process's internal heap tracker.

---

## 3. Core Infrastructure & API
*   **`h5vcc.performance.requestGlobalMemoryDump()`**: A new asynchronous JavaScript API that returns a comprehensive JSON string.
    *   **Cross-Process Support**: Captures data from the Browser, Renderer, and GPU processes simultaneously.
    *   **OS Metrics**: Includes Resident Set Size (RSS), Proportional Set Size (PSS), and Private Footprint.
*   **Unified Reporting Path**: Modified `services/resource_coordinator` to allow direct calls to Starboard system memory APIs, ensuring OS-level metrics match Cobalt's platform-specific reality.

---

## 4. Attribution Analysis Tooling
Located in `cobalt/tools/performance/`:

*   **`analyze_memory.py`**: Parses the JSON dump and calculates the **Attribution Percentage** using:
    $$	ext{Attribution \%} = \frac{\sum(	ext{cobalt/* metrics} + 	ext{chromium metrics})}{	ext{Total System RSS}}$$
*   **`run_4k_memory_test.py`**: An automation script that simulates 4K playback, triggers the dump, and validates the 80% threshold.

---

## 5. Verification Coverage
*   **Unit Tests**: `MediaMemoryDumpProviderTest` and `RasterizerMemoryDumpProviderTest` in `cobalt_unittests` verify tracker logic and JSON serialization.
*   **Browser Tests**: `PerformanceBrowserTest` in `cobalt_browsertests` verifies the end-to-end flow from the Web API to the system-level memory snapshot.
