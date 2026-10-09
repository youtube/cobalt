# Cobalt Storage Monitoring Tool (`storage_monitor.py`)

`storage_monitor.py` monitors and inspects Cobalt's persistent and cache storage directories across **Critical User Journeys (CUJs)** or passive manual sessions on Android TV (`dev.cobalt.coat` or custom packages like `com.google.android.youtube.tv`), capturing steady-state sizes, transient SQLite `-journal` / LevelDB compaction spikes, per-file write churn, and process-level `/proc/<pid>/io` read/write counters.

---

## Prerequisites

- An Android TV device or emulator connected via `adb` (`adb devices`).
- Standard Python 3 (no external third-party packages required).
- Either a `userdebug`/`eng` Android build (with `su 0` / `adb root`) or a debuggable APK (supports `run-as <package>`).

---

## Usage

```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  [--package dev.cobalt.coat] \
  [--cuj <manual|cold_start|home_scroll|video_playback|pause_flush|warm_relaunch|all>] \
  [--interval 1.0] \
  [--output storage_report.md]
```

### Command-Line Arguments

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--platform` | `android` | Target platform (currently supports `android`). |
| `--package` | `dev.cobalt.coat` | Target Android package name to inspect (e.g., `dev.cobalt.coat` for Chrobalt or `com.google.android.youtube.tv` for Kimono). |
| `--cuj` | `manual` | The Critical User Journey to execute and monitor (`manual`, `cold_start`, `home_scroll`, `video_playback`, `pause_flush`, `warm_relaunch`, or `all`). Defaults to `manual` (polls passively until `Ctrl+C`). |
| `--interval` | `1.0` | Polling frequency in seconds during the CUJ run. |
| `--output` | `storage_report_<cuj>_<timestamp>.md` | Output file path. Generates a formatted Markdown report by default, or a structured JSON report if the path ends with `.json`. |

---

## Supported Critical User Journeys (`--cuj`)

1. **`manual` *(Default)***: Does not restart or send key events to the app. Passively polls storage and `/proc/<pid>/io` at `--interval` while you manually interact with the device or run a soak test, generating the report when you press `Ctrl+C`.
2. **`cold_start` (`60s`)**: Force-stops the app, launches `https://www.youtube.com/tv`, and polls storage for 60 seconds. Captures legacy C25 migration, initial SQLite/LevelDB database creation (`Cookies`, `Local Storage`, `QuotaManager`), and cold shader cache compilation.
3. **`home_scroll` (`90s`)**: Force-stops the app, launches the Home page, and sends directional key events (`KEYCODE_DPAD_RIGHT` / `KEYCODE_DPAD_DOWN`) every 2 seconds to scroll through shelves. Captures UI state writes to `Local Storage` and HTTP cache growth.
4. **`video_playback` (`180s`)**: Force-stops the app, launches a video playback session (`#/watch?v=1La4QzGeaaQ`), plays for 60 seconds, opens the in-player shelf (`KEYCODE_DPAD_DOWN` twice), and scrolls (`KEYCODE_DPAD_RIGHT`) every 5 seconds for 120 seconds while video continues playing. Captures periodic `Local Storage` watch-progress commits, LevelDB `.log` $\rightarrow$ `.ldb` compaction spikes, and `VideoDecodeStats`.
5. **`pause_flush` (`40s`)**: Force-stops the app, launches the Home page (`10s` `pre_pause`), and sends `KEYCODE_HOME` (`30s` `pause_flush`) to background the app and trigger `OnCobaltPause()`, capturing synchronous `LocalStorage` and `CookieManager` flushes and transient `Cookies-journal` spikes.
6. **`warm_relaunch` (`60s`)**: Runs 3 consecutive force-stop and warm-relaunch cycles (20s each) without clearing app data. Captures LevelDB startup `.log` recovery into `.ldb` SSTables and `BrowserStabilityMetrics` (`.pma`) file rotation.
7. **`all` (`~430s`)**: Runs all 5 automated CUJs (`cold_start`, `home_scroll`, `video_playback`, `pause_flush`, `warm_relaunch`) sequentially, cleanly exiting (`am force-stop`) and re-launching the app before each CUJ.

---

## Examples

### 1. Passive Manual / Soak Monitoring (Until `Ctrl+C`)
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --output /tmp/manual_soak_storage.md
```

### 2. Run a Single Automated CUJ (`video_playback`) on Kimono (`com.google.android.youtube.tv`)
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --package com.google.android.youtube.tv \
  --cuj video_playback \
  --output /tmp/kimono_video_playback.md
```

### 3. Run the Full 5-CUJ Suite and Export JSON for Automated Comparison
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --cuj all \
  --output /tmp/full_storage_report.json
```

---

## Understanding the Output Report

The generated report includes header metadata (including cumulative **Process I/O (`/proc/<pid>/io`)** counters for `rchar`, `wchar`, `read_bytes`, `write_bytes`, `syscr`, `syscw`, Peak Block Read/Write `KB/s`, and Write Amplification Factor `write_bytes/wchar`, plus system-wide **Block Device IOPS (`/proc/diskstats`)** and **I/O Pressure Stall (`/proc/pressure/io`)** when readable) and two distinct sections:

1. **Section A: Storage Type Summary (`storage_type_summary`) — Full Footprint Accounting**
   - Scans both Credential-Encrypted (`/data/data/<package>/`) and Device-Encrypted (`/data/user_de/0/<package>/`) app storage directories and groups every file by its derived `(Location, Storage Type)`.
   - Reports **`Start KB`**, **`Peak KB (Time)`** (High-Water Mark), **`Final KB`**, and **`Write Ticks`**.

2. **Section B: Transient & High-Churn File Hotspots (`files_inventory`) — Filtered**
   - Strictly filtered to exclude static baseline files, retaining only files that were:
     - **`Transient (Deleted)`**: Existed during the run and were deleted/compacted/rotated before the end (e.g., compacted LevelDB `.log`/`.ldb` files, rotated `.pma` stability metrics, `.org.chromium.Chromium.*` atomic temp files).
     - **`Truncated`**: Shrank after peaking (e.g., `Cookies-journal`).
     - **`Active`**: Modified multiple times (`mtime_changes >= 2`) during the CUJ.
   - Reports **`Start KB`**, **`Peak KB`**, **`Final KB`**, **`Delta KB`** (`+X.XX` / `-X.XX`), **`Writes`**, and **`Status`**, ranking files by write frequency and peak size to pinpoint the exact files responsible for flash wear and transient disk spikes.

---

## Running Unit Tests

```bash
python3 -m unittest cobalt/tools/storage/storage_monitor_test.py
```
