# Cobalt Storage Monitoring Tool (`storage_monitor.py`)

`storage_monitor.py` monitors and inspects Cobalt's persistent and cache storage directories across **Critical User Journeys (CUJs)** on Android TV (`dev.cobalt.coat`), capturing steady-state sizes, transient SQLite `-journal` / LevelDB compaction spikes, and per-file write churn.

---

## Prerequisites

- An Android TV device or emulator connected via `adb` (`adb devices`).
- Standard Python 3 (no external third-party packages required).
- Either a `userdebug`/`eng` Android build (with `su 0` / `adb root`) or a debuggable `dev.cobalt.coat` APK (supports `run-as dev.cobalt.coat`).

---

## Usage

```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --cuj <cold_start|home_scroll|video_playback|pause_flush|warm_relaunch|all> \
  [--interval 1.0] \
  [--output storage_report.md]
```

### Command-Line Arguments

| Argument | Default | Description |
| :--- | :--- | :--- |
| `--platform` | `android` | Target platform (currently supports `android`). |
| `--cuj` | *(Required)* | The Critical User Journey to execute and monitor (`cold_start`, `home_scroll`, `video_playback`, `pause_flush`, `warm_relaunch`, or `all`). |
| `--interval` | `1.0` | Polling frequency in seconds during the CUJ run. |
| `--output` | `storage_report_<cuj>_<timestamp>.md` | Output file path. Generates a formatted Markdown report by default, or a structured JSON report if the path ends with `.json`. |

---

## Supported Critical User Journeys (`--cuj`)

1. **`cold_start` (`60s`)**: Force-stops Cobalt, launches `https://www.youtube.com/tv`, and polls storage for 60 seconds. Captures legacy C25 migration, initial SQLite/LevelDB database creation (`Cookies`, `Local Storage`, `QuotaManager`, `BTM`), and cold shader cache compilation.
2. **`home_scroll` (`90s`)**: Force-stops Cobalt, launches the Home page, and sends directional key events (`KEYCODE_DPAD_RIGHT` / `KEYCODE_DPAD_DOWN`) every 2 seconds to scroll through shelves. Captures UI state writes to `Local Storage` and HTTP cache growth.
3. **`video_playback` (`180s`)**: Force-stops Cobalt, launches a video playback session (`#/watch?v=1La4QzGeaaQ`), plays for 60 seconds, opens the in-player shelf (`KEYCODE_DPAD_DOWN` twice), and scrolls (`KEYCODE_DPAD_RIGHT`) every 5 seconds for 120 seconds while video continues playing. Captures periodic `Local Storage` watch-progress commits, LevelDB `.log` $\rightarrow$ `.ldb` compaction spikes, and `VideoDecodeStats`.
4. **`pause_flush` (`40s`)**: Force-stops Cobalt, launches the Home page (`10s` `pre_pause`), and sends `KEYCODE_HOME` (`30s` `pause_flush`) to background Cobalt and trigger `OnCobaltPause()`, capturing synchronous `LocalStorage` and `CookieManager` flushes and transient `Cookies-journal` spikes.
5. **`warm_relaunch` (`60s`)**: Runs 3 consecutive force-stop and warm-relaunch cycles (20s each) without clearing app data. Captures LevelDB startup `.log` recovery into `.ldb` SSTables and `BrowserStabilityMetrics` (`.pma`) file rotation.
6. **`all` (`~430s`)**: Runs all 5 CUJs sequentially, cleanly exiting (`am force-stop`) and re-launching Cobalt before each CUJ.

---

## Examples

### 1. Run a Single CUJ (`video_playback`) and Generate a Markdown Report
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --cuj video_playback \
  --output /tmp/video_playback_storage.md
```

### 2. Run the Full 5-CUJ Suite and Export JSON for Automated Comparison
```bash
python3 cobalt/tools/storage/storage_monitor.py \
  --platform android \
  --cuj all \
  --output /tmp/full_storage_report.json
```

---

## Understanding the Output Report

The generated report contains two distinct sections:

1. **Section A: Storage Type Summary (`storage_type_summary`) — Full Footprint Accounting**
   - Scans both Credential-Encrypted (`/data/data/dev.cobalt.coat/`) and Device-Encrypted (`/data/user_de/0/dev.cobalt.coat/`) app storage directories and groups every file by its derived `(Location, Storage Type)`.
   - Reports **`Start KB`**, **`Peak KB (Time)`** (High-Water Mark), **`Final KB`**, and **`Write Ticks`**.

2. **Section B: Transient & High-Churn File Hotspots (`files_inventory`) — Filtered**
   - Strictly filtered to exclude static baseline files, retaining only files that were:
     - **`Transient (Deleted)`**: Existed during the run and were deleted/compacted/rotated before the end (e.g., compacted LevelDB `.log`/`.ldb` files, rotated `.pma` stability metrics, `.org.chromium.Chromium.*` atomic temp files).
     - **`Truncated`**: Shrank after peaking (e.g., `Cookies-journal`).
     - **`Active`**: Modified multiple times (`mtime_changes >= 2`) during the CUJ.
   - Ranks files by **`Writes (mtime_changes)`** and **`Peak KB`** to pinpoint the exact files responsible for flash wear and transient disk spikes.

---

## Running Unit Tests

```bash
python3 -m unittest cobalt/tools/storage/storage_monitor_test.py
```
