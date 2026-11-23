# Cobalt Storage Monitoring Tool (`storage_monitor.py`)

`storage_monitor.py` monitors and inspects Cobalt's persistent and cache storage directories across **Critical User Journeys (CUJs)** or passive manual sessions on Android TV (`dev.cobalt.coat` or custom packages like `com.google.android.youtube.tv`), capturing steady-state sizes, transient SQLite `-journal` / LevelDB compaction spikes, per-file write churn, and the app's block I/O (`read_bytes` / `write_bytes` from `/proc/<pid>/io`).

It also records the **storage footprint of every bucket and folder over time** (`storage_footprint.py`) and turns it into an HTML report with findings, a chart and a folder tree (see [Storage Footprint Timeline](#storage-footprint-timeline)).

---

## How This Tool Complements UMA

UMA measures storage across the fleet, but only as a few numbers per launch: totals and process-wide rates. This tool runs on one device and shows where those numbers come from.

| Question | UMA (fleet) | `storage_monitor.py` (one device) |
| :--- | :--- | :--- |
| How big is each directory? | `Cobalt.Storage.{UserData,DirCache}.StartupTotalSizeKB` (folder totals 10 s after launch) plus upstream per-cache init sizes (`SimpleCache.{Http,Code}.CacheSizeOnInit`, `LocalStorage.MojoSizeInKB`). | Every storage bucket and folder every `--interval` seconds through the CUJ (footprint timeline), plus the largest files. Section A: start, peak (with time) and final size of every storage type (`Cookies`, `Local Storage`, `DIPS`, `DIR_CACHE:Code Cache`, ...), including ones outside those two directories (`BrowserStabilityMetrics`, GPU shader caches under `/data/user_de`). |
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
  [--serial <serial>] \
  [--cuj <manual|cold_start|home_scroll|video_playback|pause_flush|warm_relaunch|all>] \
  [--interval 1.0] \
  [--verify-storage-apis] \
  [--cdp-port 9222] \
  [--extra-args <args>] \
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
| `--serial` | `$ANDROID_SERIAL` | Device serial passed as `adb -s` to every `adb` call (needed when several devices are connected). |
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
2. **`cold_start` (`60s`)**: Force-stops the app, launches `https://www.youtube.com/tv`, and polls storage for 60 seconds. App data is not cleared: on a fresh install, after `pm clear` or right after an upgrade this captures the C25 migration and initial database creation; otherwise it is a cold process start on the existing profile.
3. **`home_scroll` (`90s`)**: Force-stops the app, launches the Home page, and sends directional key events (`KEYCODE_DPAD_RIGHT` / `KEYCODE_DPAD_DOWN`) every 2 seconds to scroll through shelves. Captures UI state writes to `Local Storage` and HTTP cache growth.
4. **`video_playback` (`180s`)**: Force-stops the app, launches a video playback session (`#/watch?v=1La4QzGeaaQ`), plays for 60 seconds, opens the in-player shelf (`KEYCODE_DPAD_DOWN` twice), and scrolls (`KEYCODE_DPAD_RIGHT`) every 5 seconds for 120 seconds while video continues playing. Captures periodic `Local Storage` watch-progress commits, LevelDB `.log` $\rightarrow$ `.ldb` compaction spikes, and `VideoDecodeStats`.
5. **`pause_flush` (`40s`)**: Force-stops the app, launches the Home page (`10s` `pre_pause`), and sends `KEYCODE_HOME` (`30s` `pause_flush`) to background the app and trigger `OnCobaltPause()`, capturing synchronous `LocalStorage` and `CookieManager` flushes and transient `Cookies-journal` spikes.
6. **`warm_relaunch` (`60s`)**: Runs 3 consecutive force-stop + relaunch cycles (cold process starts on the same profile, 20s each). Captures LevelDB startup `.log` recovery into `.ldb` SSTables and `BrowserStabilityMetrics` (`.pma`) file rotation.
7. **`all` (`~450s`, about 7.5 min: 430 s of polling plus about 1.4 s per relaunch)**: Runs all 5 automated CUJs (`cold_start`, `home_scroll`, `video_playback`, `pause_flush`, `warm_relaunch`) sequentially, cleanly exiting (`am force-stop`) and re-launching the app before each CUJ.

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

Every poll also records where the app's disk space is. Each file and folder is attributed to one **bucket** (`Cookies`, `LocalStorage`, `DIPS`, `HttpCache`, `CodeCacheJs`, `CleanupTargets`, ...) using the built-in path rules in `storage_footprint.py` (`DEFAULT_BUCKET_TABLE_TEXT`). Sizes are **allocated** disk space (`st_blocks * 512`, what `du` and Android's storage settings count); KB and MB are 1024-based. UMA (`base::ComputeDirectorySize`) and the budgets count logical file sizes (`st_size`, files only), which on f2fs are about 0.5x allocated for `UserData` and about 0.9x for `DirCache`; the JSONL has both (`logical_bytes`).

Buckets are grouped by root:

| Root | Directory (Android) | What it holds |
| :--- | :--- | :--- |
| `UserData` | `app_content_shell/content_shell` | Persistent Chromium storage: cookies, Local Storage, DIPS, ... Budget: 1 MiB of file data (logical), as `Cobalt.Storage.UserData.StartupTotalSizeKB` counts it. |
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
3. **Footprint at key moments**: one row per bucket, grouped by root, at the start, 10 s after the first launch (when the browser records `Cobalt.Storage.{UserData,DirCache}.StartupTotalSizeKB` and `Cobalt.Storage.ProcessIo.Startup{Read,Write}KB`), 30 s after it (when `kCobaltStorageCleanup` runs, if enabled), the end of each CUJ (`--cuj all`) or of each phase of a single CUJ, and the end. Also the peak (with time), the change over the run, a trend line, the share of the total and the file count.
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

## Perfetto Storage Tracing (`profile_storage_perfetto.py`)

`profile_storage_perfetto.py` records an on-device Perfetto trace during a CUJ and shows why storage I/O is slow: which Chromium storage engine (SQLite, LevelDB, `base::File`, `ScopedBlockingCall`) ran when and for how long, and which concurrent fsyncs and block requests the UI, renderer, GPU and compositor threads were waiting on (kernel `io_wait`).

The tool launches the app itself because Chromium only connects to the system tracing service with `--enable-features=EnablePerfettoSystemTracing`, which is disabled by default on Android. The CUJs do not poll the device, so the trace only shows the app's own I/O.

```bash
python3 cobalt/tools/storage/profile_storage_perfetto.py \
  [--serial SERIAL] [--package com.google.android.youtube.tv] \
  [--cuj cold_start|lifecycle|manual] [--duration-s 65] [--cuj-cmd "<host command>"] \
  [--golden-tar /data/local/tmp/golden.tar] [--drop-caches] [--extra-args "--enable-features=A;B"] \
  [--include-memory-infra] [--out out/perfetto/cold_start.pftrace] [--report-dir DIR]
python3 cobalt/tools/storage/profile_storage_perfetto.py --analyze-only TRACE [--report-dir DIR]
python3 cobalt/tools/storage/profile_storage_perfetto.py --print-config
```

| `--cuj` | What it does |
| :--- | :--- |
| `cold_start` (default) | `am start -S` cold launch, then traces for `--duration-s` (default `65` s). |
| `lifecycle` (`~125 s`) | Cold launch and 30 s; 8 × (`KEYCODE_DPAD_DOWN`, `KEYCODE_DPAD_RIGHT`) 2 s apart; `#/watch?v=1La4QzGeaaQ` intent and 45 s; `KEYCODE_HOME` and 15 s. |
| `manual` | Cold launch, then traces until `Ctrl+C`, `--duration-s`, or until the `--cuj-cmd` host command exits. |

- `--golden-tar` restores the app data from a tar on the device with `run-as`, deletes `BrowserStabilityMetrics/*.pma`, and runs `sync` before tracing.
- `--drop-caches` needs root; otherwise it warns and traces with a warm page cache.
- `--extra-args` is comma-separated like in `storage_monitor.py`. Cobalt merges repeated `--enable-features`, so pass several features as `--enable-features=A;B`.
- Analysis needs `trace_processor_shell` (default `~/.local/share/perfetto/prebuilts/trace_processor_shell`; `curl -LO https://get.perfetto.dev/trace_processor && python3 trace_processor --version` downloads it there).
- If the CUJ fails or is interrupted, the tool still stops Perfetto and saves the trace and `<stem>.phases.json`. As a safety net, the trace stops by itself 120 s after its expected length (`manual`: after 1 hour). The exit code is 1 if the CUJ, a query, or `--cuj-cmd` fails, or if the trace is unhealthy.

Outputs, where `<stem>` is the trace file name without its extension:
- `<stem>.q1.csv` … `<stem>.q8.csv`: the queries in `perfetto_storage_queries.sql`: Q1 I/O by subsystem, Q2 slowest operations, Q3/Q4 storage histogram samples, Q5 first/last I/O per engine, Q6 overlapping slow operations of different engines, Q7 kernel `io_wait` per app thread, and Q8 the `io_wait` of `CrBrowserMain`, `Chrome_InProcRendererThread`, `Chrome_InProcGpuThread` and `VizCompositorThread`/`Compositor` that overlaps worker storage slices, `ext4`/`f2fs` fsyncs, or block requests.
- `<stem>.phases.json`: the CUJ actions and phases in `/proc/uptime` seconds (`CLOCK_BOOTTIME`, the trace clock: trace timestamp in ns = seconds × 1e9).
- `<stem>.summary.json` and `<stem>.md`: trace sha256, size and duration, versions, trace health, phases, and row counts and top rows of each query. A healthy trace is complete (it has `tracing_disabled_ns` and the end-of-trace stats) and has no failed flushes, overwritten or discarded buffer chunks, ftrace overruns, out-of-order events, or other `data_loss` stats. Ftrace events that the kernel does not support (for example `android_fs` and `f2fs` on some retail devices) are listed as ftrace setup errors.

---

## Running Unit Tests

```bash
python3 -m unittest cobalt/tools/storage/storage_monitor_test.py cobalt/tools/storage/storage_footprint_test.py
python3 -m unittest cobalt/tools/storage/profile_storage_perfetto_test.py
```
