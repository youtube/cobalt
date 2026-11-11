# Cobalt Storage Monitoring Tool (`storage_monitor.py`)

`storage_monitor.py` monitors and inspects Cobalt's persistent and cache storage directories across **Critical User Journeys (CUJs)** or passive manual sessions on Android TV (`dev.cobalt.coat` or custom packages like `com.google.android.youtube.tv`), capturing steady-state sizes, transient SQLite `-journal` / LevelDB compaction spikes, per-file write churn, and the app's block I/O (`read_bytes` / `write_bytes` from `/proc/<pid>/io`).

---

## How This Tool Complements UMA

UMA measures storage across the fleet, but only as a few numbers per launch: totals and process-wide rates. This tool runs on one device and shows where those numbers come from.

| Question | UMA (fleet) | `storage_monitor.py` (one device) |
| :--- | :--- | :--- |
| How big is each directory? | `Cobalt.Storage.UserData.StartupTotalSizeKB` and `Cobalt.Storage.DirCache.StartupTotalSizeKB`: one total each, 30 s after launch. | Section A: start, peak (with time) and final size of every storage type (`Cookies`, `Local Storage`, `DIPS`, `DIR_CACHE:Code Cache`, ...), including ones outside those two directories (`BrowserStabilityMetrics`, GPU shader caches under `/data/user_de`). |
| Which files are written, and how often? | Not available. `Cobalt.Storage.ProcessIo.WriteRateKBPerMin` is process-wide. | Section B: per-file write count, size change and lifespan. |
| Short-lived files and spikes? | Missed between samples. | 1 s polling catches `-journal` / `-wal` spikes, LevelDB compaction files and temp files (`Transient`, `Truncated`). |
| Block I/O over time? | `Cobalt.Storage.ProcessIo.StartupReadKB` / `StartupWriteKB` (first 30 s) and `WriteRateKBPerMin` (5-minute averages). | The same `/proc/<pid>/io` counters, totaled over the run with the peak KB/s and when it happened, across app restarts. |
| Open file descriptors & 32-bit `mmap` VMAs? | `Memory.Experimental.Browser2.Small.{LevelDatabase,Sqlite}` (heap only; no FD or VMA count). | `/proc/<pid>/fd` and `/proc/<pid>/maps` polling: total and storage-backed open FDs and `mmap` VMAs, plus `Peak FDs` and `Peak VMAs` per storage type in Section A. |
| Do Web Storage & MediaCapabilities JS APIs still work? | `SessionStorageContext.CommitResult`, `Cobalt.LocalStorage.*Error`, `Cookie.*Problem`. | `--verify-storage-apis`: active CDP `Runtime.evaluate` probe verifying `window.sessionStorage`, `window.localStorage`, `document.cookie`, and `navigator.mediaCapabilities.decodingInfo()`. |
| Checking a change before it ships? | Needs a release and fleet data. | Scripted CUJs on a `userdebug` device or a debuggable APK, including Kimono (`--package`). |

Device-wide counters such as `/proc/diskstats` and `/proc/pressure/io` are not collected: they need root, include every app on the device, and are better captured with a system trace such as Perfetto.

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
  [--verify-storage-apis] \
  [--cdp-port 9222] \
  [--output storage_report.md]
```

### Command-Line Arguments

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--platform` | `android` | Target platform (currently supports `android`). |
| `--package` | `dev.cobalt.coat` | Target Android package name to inspect (e.g., `dev.cobalt.coat` for Chrobalt or `com.google.android.youtube.tv` for Kimono). |
| `--cuj` | `manual` | The Critical User Journey to execute and monitor (`manual`, `cold_start`, `home_scroll`, `video_playback`, `pause_flush`, `warm_relaunch`, or `all`). Defaults to `manual` (polls passively until `Ctrl+C`). |
| `--interval` | `1.0` | Polling frequency in seconds during the CUJ run. |
| `--verify-storage-apis` | `False` | When set, enables `--remote-allow-origins=*` on launch and runs a CDP `Runtime.evaluate` check verifying `window.sessionStorage`, `window.localStorage`, `document.cookie`, and `navigator.mediaCapabilities.decodingInfo()`. |
| `--cdp-port` | `9222` | Local TCP port used for `adb forward` when `--verify-storage-apis` is enabled. |
| `--extra-args` | `""` | Comma-separated extra `commandLineArgs` passed to Cobalt on launch (e.g. `--enable-features=CobaltStorageCleanup`). |
| `--output` | `storage_report_<cuj>_<timestamp>.md` | Output file path. Generates a formatted Markdown report by default, or a structured JSON report if the path ends with `.json`. |

---

## Supported Critical User Journeys (`--cuj`)

1. **`manual` *(Default)***: Does not restart or send key events to the app. Passively polls storage and `/proc/<pid>/{io,fd,maps}` at `--interval` while you manually interact with the device or run a soak test, generating the report when you press `Ctrl+C`.
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

### 2. Run a Single Automated CUJ (`video_playback`) on Kimono (`com.google.android.youtube.tv`) with Web API Verification
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --package com.google.android.youtube.tv \
  --cuj video_playback \
  --verify-storage-apis \
  --output /tmp/kimono_video_playback.md
```

### 3. Run the Full 5-CUJ Suite and Export JSON for Automated Comparison
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --cuj all \
  --verify-storage-apis \
  --output /tmp/full_storage_report.json
```

---

## Understanding the Output Report

The report header shows:
- **Persistent Footprint** (start, peak with time, final)
- **Block I/O (`/proc/<pid>/io`)**: total `read_bytes` / `write_bytes` over the run, the peak KB/s of each with its time, and how many processes were seen
- **Open FDs & VMAs (`/proc/<pid>/fd`, `maps`)**: peak total and storage-backed open file descriptors and 32-bit `mmap` virtual memory areas (including unlinked `(deleted)` files still held open in the kernel)
- **Web API Parity (CDP)** (when `--verify-storage-apis` is set): `PASS`/`FAIL` status for `window.sessionStorage`, `window.localStorage`, `document.cookie`, and `navigator.mediaCapabilities.decodingInfo()`

It is followed by two sections:

1. **Section A: Storage Type Summary (`storage_type_summary`) — Full Footprint Accounting**
   - Scans both Credential-Encrypted (`/data/data/<package>/`) and Device-Encrypted (`/data/user_de/0/<package>/`) app storage directories and groups every file by its derived `(Location, Storage Type)`.
   - Reports **`Start KB`**, **`Peak KB (Time)`** (High-Water Mark), **`Final KB`**, **`Write Ticks`**, **`Peak FDs`**, and **`Peak VMAs`**.

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
