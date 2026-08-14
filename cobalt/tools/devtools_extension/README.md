# Cobalt Memory Breakdown DevTools Extension (Side Panel)

A lightweight Manifest V3 Chrome Extension providing real-time memory inspection, session P50 telemetry breakdowns, and time-windowed peak memory guardrail tracking for Cobalt/Chrobalt on Android TV, physical RDK devices, and Linux Desktop.

---

## Installation & Setup in Google Chrome

1. Open Google Chrome on your workstation and navigate to:
   ```text
   chrome://extensions
   ```
2. Toggle on **Developer mode** in the upper-right corner.
3. Click **Load unpacked**.
4. Select the directory:
   ```text
   <cobalt-src>/cobalt/tools/devtools_extension/
   ```
5. The **Cobalt Memory Breakdown** extension icon will appear in your Chrome toolbar.

---

## Connecting to Cobalt Targets

### 1. Android TV / Chromecast / Living Room Devices

1. **Forward the DevTools port over ADB**:
   ```bash
   adb forward tcp:9222 localabstract:content_shell_devtools_remote
   ```
2. **Launch Cobalt with Remote Debugging Enabled**:
   ```bash
   adb shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity \
     --esa commandLineArgs '--remote-debugging-port=9222,--remote-allow-origins=*,--enable-features=CobaltMetricsInterval:memory-metrics-interval/3,--url=https://www.youtube.com/tv'
   ```
   *(Note: `--enable-features=CobaltMetricsInterval:memory-metrics-interval/3` accelerates UMA sampling so P50 metrics populate every 3 seconds).*
3. **Open the Side Panel**:
   Click the Cobalt icon in your Chrome toolbar. The extension will automatically open the side panel and connect to `ws://127.0.0.1:9222`.

---

### 2. Linux Desktop / Evergreen-x64 (`loader_app`)

1. **Launch `loader_app` with Remote Debugging**:
   ```bash
   ./out/evergreen-x64_devel/loader_app \
     --remote-debugging-port=9222 \
     --remote-allow-origins=* \
     --enable-features=CobaltMetricsInterval:memory-metrics-interval/3 \
     --url="https://www.youtube.com/tv"
   ```
2. **Open the Side Panel**:
   Click the Cobalt extension icon in your Chrome toolbar.

---

### 3. RDK / Remote Set-Top Box

1. Launch Cobalt on the RDK device with `--remote-debugging-port=9222 --remote-allow-origins=*`.
2. In the Side Panel header, enter the device's IP address (e.g. `192.168.1.105:9222`) and click **Connect**.

---

## Metric Breakdown Reference

The primary **Session Median Telemetry (P50 Breakdown)** table renders the 12 continuous native and managed allocators matching production Kimono / PLX field telemetry (`go/kimono-memory-metrics`):

### Process Totals (OS / Kernel)
* **Physical Resident Set (RSS)** (`Memory.Browser.ResidentSet`): Total physical RAM pages in RAM mapped by the OS kernel.
* **Private Memory Footprint** (`Memory.Browser.PrivateMemoryFootprint`): Unshared memory dedicated strictly to Cobalt (Anonymous RSS + Swap), directly contributing to device OOM threshold.

### Subsystem & Allocator Breakdown
* **PartitionAlloc C++ Heap** (`Memory.Experimental.Browser2.PartitionAlloc`): General C++ allocations across browser/engine modules and JavaScript `ArrayBuffer` backing stores.
* **System Malloc Heap** (`Memory.Experimental.Browser2.Malloc`): System C runtime `malloc` (allocations outside PartitionAlloc-Everywhere, e.g. third-party dynamic libraries).
* **V8 JavaScript Engine Heap** (`Memory.Experimental.Browser2.V8`): V8 JavaScript engine managed heap (JS objects, compiled bytecode, closures, IC caches).
* **Blink C++ GC (cppgc)** (`Memory.Experimental.Browser2.BlinkGC`): Oilpan C++ Garbage Collector holding DOM Nodes, Elements, and CSSOM.
* **Skia 2D Graphics & Images** (`Memory.Experimental.Browser2.Skia`): 2D raster cache, glyph atlas, path tessellations, and decoded image buffers.
* **Binary Executable Pages** (`Memory.Browser.LibChrobaltRss`): Resident executable code, `.rodata`, and `.data` pages of `libchrobalt.so`.
* **Other Dynamic Shared Libraries** (`Memory.Experimental.Browser2.CodeOther`): Resident memory mapped by other dynamic system libraries (`libc.so`, system GPU drivers).
* **Font Caches & Glyph Buffers** (`Memory.Experimental.Browser2.Fonts`): Font tables, FreeType glyph slot caches, and HarfBuzz text shaping buffers.
* **Thread Stacks** (`Memory.Experimental.Browser2.Stacks`): Active stack frames for Cobalt threads (Browser, Compositor, IO, Media workers).
* **Android ART Java Heap** (`Memory.Experimental.Browser2.JavaHeap`): Android Java runtime heap for Coat activity lifecycle and JNI media bridges.
