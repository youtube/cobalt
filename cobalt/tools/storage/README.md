# Cobalt Storage Monitoring Tool (`storage_monitor.py`)

`storage_monitor.py` monitors and inspects Cobalt's persistent and cache storage directories across **Critical User Journeys (CUJs)** or passive manual sessions on Android TV (`dev.cobalt.coat` or custom packages like `com.google.android.youtube.tv`), capturing steady-state sizes, transient SQLite `-journal` / LevelDB compaction spikes, per-file write churn, and the app's block I/O (`read_bytes` / `write_bytes` from `/proc/<pid>/io`).

It also records the **storage footprint of every bucket and folder over time** (`storage_footprint.py`), using the same bucket table as the `Cobalt.Storage.Footprint.*` UMA histograms, and turns it into an HTML report with findings, a chart and a folder tree (see [Storage Footprint Timeline](#storage-footprint-timeline)).

---

## How This Tool Complements UMA

UMA measures storage across the fleet, but only as a few numbers per launch: totals and process-wide rates. This tool runs on one device and shows where those numbers come from.

| Question | UMA (fleet) | `storage_monitor.py` (one device) |
| :--- | :--- | :--- |
| How big is each directory? | `Cobalt.Storage.Footprint.{Startup,PostCleanup,Uptime}.Bucket.<Bucket>.SizeKB`: allocated size per bucket 10 s after launch, after the 30 s storage cleanup, and once a day. | The same buckets every `--interval` seconds through the CUJ (footprint timeline), plus the size of every folder and the largest files. Section A: start, peak (with time) and final size of every storage type (`Cookies`, `Local Storage`, `DIPS`, `DIR_CACHE:Code Cache`, ...), including ones outside those two directories (`BrowserStabilityMetrics`, GPU shader caches under `/data/user_de`). |
| Which files are written, and how often? | Not available. `Cobalt.Storage.ProcessIo.WriteRateKBPerMin` is process-wide. | Section B: per-file write count, size change and lifespan. |
| Short-lived files and spikes? | Missed between samples. | 1 s polling catches `-journal` / `-wal` spikes, LevelDB compaction files and temp files (`Transient`, `Truncated`). |
| Block I/O over time? | `Cobalt.Storage.ProcessIo.StartupReadKB` / `StartupWriteKB` (first 10 s) and `WriteRateKBPerMin` (5-minute averages). | The same `/proc/<pid>/io` counters, totaled over the run with the peak KB/s and when it happened, across app restarts. |
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
  [--output storage_report.md] \
  [--timeline storage_report_footprint.jsonl]

# Compare the footprint of two runs (no device needed):
python3 cobalt/tools/storage/storage_monitor.py --compare a_footprint.jsonl b_footprint.jsonl
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
| `--timeline` | `<output>_footprint.jsonl` | Footprint timeline (one JSON record per sample). The HTML report is written next to it with the same name (`.html`). |
| `--compare A B` | | Compares the footprint timelines of two runs (e.g. a feature off and on) bucket by bucket and prints a Markdown table. Does not touch the device. |

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

The Markdown report puts the **Storage Footprint by Bucket** section (below) before Section A.

---

## Storage Footprint Timeline

Every poll also records where the app's disk space is. Each file and folder is attributed to one **bucket** (`Cookies`, `LocalStorage`, `DIPS`, `HttpCache`, `CodeCacheJs`, `CleanupTargets`, ...) with the table in [`cobalt_storage_buckets.inc`](../../browser/metrics/cobalt_storage_buckets.inc). The browser uses the same table for the `Cobalt.Storage.Footprint.*` histograms, so a bucket's lab number can be compared directly with its UMA histogram. Sizes are **allocated** disk space (`st_blocks * 512`, what `du` and Android's storage settings count); KB and MB are 1024-based.

Buckets are grouped by root:

| Root | Directory (Android) | What it holds |
| :--- | :--- | :--- |
| `UserData` | `app_content_shell/content_shell` | Persistent Chromium storage: cookies, Local Storage, DIPS, ... Budget: 1 MB. |
| `DirCache` | `cache` | HTTP, code and shader caches. Android may clear it. Budget: 24 MB (`kSbMaxSystemPathCacheDirectorySize`). |
| `AppData` | `/data/data/<package>` | Everything else the app owns: Java files, the WorkManager database, stability metrics, ... |
| `UserDe` | `/data/user_de/0/<package>` | The device-encrypted directory with the OS shader caches. Lab only; the browser does not measure it. |

A run writes:

| File | Contents |
| :--- | :--- |
| `<output>.md` | The report. Its **Storage Footprint by Bucket** section has the findings, the key-moment table, the largest folders and the largest files. (`.json` reports have the findings in `storage_footprint`.) |
| `<output>_footprint.html` | The full view: a self-contained page (no scripts) to open in a browser. |
| `<output>_footprint.jsonl` | The raw samples, to re-render or compare later. |

The HTML report, top to bottom:

1. **Summary cards**: total at the end, peak (with time), `UserData` and `DirCache` against their budgets (red when over), and process starts.
2. **Findings**, most severe first:
   - `HIGH`: a root over its budget at the end, with its largest buckets.
   - `MEDIUM`: space in `CleanupTargets` (what `kCobaltStorageCleanup` deletes); SQLite `-wal` / `-journal` files of 256 KB or more, with their ratio to the database.
   - `INFO`: buckets that grew (when the growth happened, or the rate per minute if it was steady); transient spikes (a peak at least 512 KB above both the start and the end, e.g. LevelDB compaction); space in the `Other*` catch-all buckets, with its largest paths; small-file overhead; large drops of buckets (256 KB and 20% or more, e.g. the cleanup 30 s after launch, or a cache eviction), and repeated drops (churn: data deleted and written again).
3. **Footprint at key moments**: one row per bucket, grouped by root, at the start, 10 s and 30 s after the first launch (when the browser records the `Startup` and `PostCleanup` histograms), the end of each CUJ (`--cuj all`) or of each phase of a single CUJ, and the end. Also the peak (with time), the change over the run, a trend line, the share of the total and the file count.
4. **Footprint over time**: a stacked area chart of the buckets (one color family per root) with CUJ or phase bands and launch/HOME markers, and the app's block I/O below it: writes and reads, each scaled to its own peak.
5. **Folders at the end**: a collapsible tree of every folder with its total size (including subfolders), change since the start, file count and bucket. A folder whose peak during the run was well above its end size shows the peak and when (e.g. LevelDB compaction), and folders deleted during the run are listed struck out with when they were deleted (e.g. by the storage cleanup). Other folders under 0.5% of their parent are folded.
6. **Largest files at the end**: allocated and logical size, change since the start, and bucket.

To re-render a timeline, or compare two runs (e.g. a feature off and on):

```bash
python3 cobalt/tools/storage/storage_footprint.py report run_footprint.jsonl [--html run.html] [--markdown run.md]
python3 cobalt/tools/storage/storage_footprint.py compare a_footprint.jsonl b_footprint.jsonl [--output diff.md]
```

### JSONL Format

One JSON object per line; `t` is seconds since the start of the run.

- `{"type": "header", ...}`: package, platform, CUJ, interval, extra args, start time, the root directories, the bucket names and the budgets.
- `{"type": "event", "t": ..., "event": ..., "detail": ...}`: `cuj`, `cuj_phase`, `launch`, `home`, `process_start` and `process_exit`.
- `{"type": "sample", "t": ..., ...}`: the app's `pid` and its `io` counters (`read_bytes`, `write_bytes`); `roots` and `buckets` with `allocated_bytes`, `logical_bytes`, `files` and `dirs` (and each bucket's `root`); `folders` as `path: [allocated, logical, files]` (paths relative to the app directory, `""` is the app directory, totals include subfolders); and `files` as `[path, allocated, logical]` for the 25 largest files plus every SQLite companion file of 32 KB or more and its database.

---

## Running Unit Tests

```bash
python3 -m unittest cobalt/tools/storage/storage_monitor_test.py cobalt/tools/storage/storage_footprint_test.py
```
