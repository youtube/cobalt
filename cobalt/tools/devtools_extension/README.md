# Cobalt Memory Breakdown — Chrome Extension

A multi-platform Chrome Extension that provides a **Chrome Side Panel** and **DevTools Panel** for real-time memory breakdown charts, session P50 telemetry medians, lifecycle guardrails, and baseline diffing across **Android TV, RDK boxes, and Linux Desktop**.

---

## Features

* **⚡ Real-Time Live Memory**: 1 Hz live meters for Physical RSS (`Memory.Browser.ResidentSet.Live`), Private Memory Footprint (`Memory.Browser.PrivateMemoryFootprint.Live`), and V8 Heap (`Memory.Experimental.Browser2.V8.Live`).
* **📊 Dual-Stream Telemetry Suite**: Displays the 13 canonical Cobalt UMA memory breakdown metrics (`*.P50`) aligned with `go/kimono-memory-metrics`.
* **⏱ Lifecycle Guardrails**: Real-time tracking of time-windowed peak private memory footprint across 0-2min (startup), 2-4min (playback), 4-8min (browsing), and 8-16min (soak).
* **📍 Interactive Baseline Diffing**: `[Take Baseline]` button calculates live delta increments ($\pm\Delta$ MB) across all subsystems to quickly isolate memory leaks.
* **⬇ Telemetry Exporter**: One-click JSON report export matching Kimono/PLX schemas for offline triage.
* **📱 Universal Platform Support**: Works seamlessly on Android TV (via ADB), RDK hardware (via LAN IP), and Linux desktop.

---

## How to Test the Extension

### Step 1: Load or Refresh the Extension in Chrome
1. Open Google Chrome and navigate to `chrome://extensions`.
2. Ensure **Developer mode** is enabled (top-right toggle).
3. If already loaded, click the 🔄 **Refresh** icon on the **Cobalt Memory Breakdown** card (or click **Load unpacked** and select `/usr/local/google/home/haozheng/cobalt/src/cobalt/tools/devtools_extension`).

---

### Step 2: Open the Side Panel
1. In the top-right toolbar of Google Chrome (next to the address bar), click the **Extensions** (puzzle piece) icon 🧩.
2. Click **Cobalt Memory Breakdown** (or pin it to your toolbar and click its icon).
3. The **Chrome Side Panel** will open immediately on the right side of your browser!

---

### Step 3: Connect to Cobalt

* **Option A: Local Linux Desktop (`loader_app`)**:
  - The side panel will **auto-connect immediately** to `ws://127.0.0.1:9222`.
  - Start Cobalt with:
    ```bash
    ./out/evergreen-x64_devel/loader_app \
        --remote-debugging-port=9222 \
        --remote-allow-origins=* \
        --url="https://www.youtube.com/tv"
    ```

* **Option B: Android TV / Fire TV (via ADB)**:
  - Set up ADB port forwarding:
    ```bash
    adb forward tcp:9222 localabstract:content_shell_devtools_remote
    ```
  - The side panel will automatically stream Android TV's live memory metrics!

* **Option C: Physical RDK / TV Set-Top Box (via LAN IP)**:
  - In the **Target CDP** input box at the top of the side panel, enter:
    ```text
    ws://<DEVICE_IP>:9222
    ```
  - Click **Connect**.
