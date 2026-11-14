# SbPlayer Create/Destroy Endurance Harness

`SbPlayerEnduranceTest.DISABLED_CreateDestroy` (`player_endurance_test.cc`) is a
platform-agnostic stress test of the public Starboard `SbPlayer` API. Everything
below the `SbPlayer` API is treated as a black box.

`run_android_endurance.py` and `run_android_endurance_matrix.py` are
Android-specific host scripts for running the test on a target device over `adb`
and collecting self-contained diagnostic bundles for triage.
`summarize_endurance.py` summarizes collected bundle directories into JSON and
Markdown reports.

## What the Test Does

Before the first iteration, the test loads the requested `.dmp` audio and video
sample streams into memory so file I/O does not occur inside timed iterations.
Only one `SbPlayer` exists at a time: each player is destroyed before the next
is created.

On each iteration `i` (`0 .. iterations - 1`):

1. If `i > 0` and `--endurance_gap_ms > 0`, sleeps for the requested gap.
2. Calls `SbPlayerCreate` (rotating across comma-separated `--endurance_video`
   clips via `i % count`) and waits for `kSbPlayerStateInitialized`.
3. Feeds audio and video samples (`SbPlayerWriteSamples` /
   `SbPlayerWriteEndOfStream`) until the player reports
   `kSbPlayerStatePresenting`.
4. If `--endurance_seeks > 0`, calls `SbPlayerSeek` that many times across
   spread-out clip positions, feeding samples back to `kSbPlayerStatePresenting`
   after each seek.
5. Calls `SbPlayerDestroy`.

Failed iterations are recorded to CSV and the loop continues so intermittent
failures can be counted over long runs. Waits on `SbPlayer` callbacks are
bounded. Because blocking `SbPlayer` calls cannot be interrupted, a watchdog
thread monitors `create`, `presenting`, `seek`, and `destroy` against
`--endurance_hang_timeout_ms` (default `30000` ms): if a step exceeds the
timeout, an interim `*_hung` row is flushed immediately, and if the call later
returns, the final row for that iteration is written and the test stops.

## Step 1: Prerequisites and Device Setup

1. Configure a Cobalt checkout for your target Android ABI (for example,
   `out/android-arm_qa` or `out/android-arm64_qa`):

   ```bash
   cobalt/build/gn.py -p android-arm -c qa
   ```

2. Verify `python3` and `adb` are available on the host, and confirm the target
   Android TV / device is online and authorized:

   ```bash
   adb -s <SERIAL> get-state
   # Expected output: device
   ```

3. Prepare the device for unattended runs:
   - Keep HDMI output / display awake (disable sleep timers, screensavers, and
     auto-standby) so surface destruction does not interrupt `punch_out` runs.
   - Give the device exclusively to the test during execution. Pass
     `--stop-package <pkg>` if another media app may hold foreground or hardware
     decoders beforehand.

## Step 2: Build `player_benchmarks` and Test Clips

Build `player_benchmarks` from the repository root:

```bash
autoninja -C out/android-arm_qa player_benchmarks
```

`player_benchmarks` depends on
`//starboard/shared/starboard/player:player_test_data`. During the build,
`starboard/tools/download_from_gcs.py` automatically downloads the `.dmp` clips
listed in `starboard/shared/starboard/player/testdata/sha1_files.gni` into the
build output test data directory. The generated
`out/android-arm_qa/bin/run_player_benchmarks` wrapper pushes those `.dmp` files
to the device automatically.

For local Linux smoke testing without an Android device:

```bash
autoninja -C out/linux-x64x11_devel player_benchmarks
./out/linux-x64x11_devel/player_benchmarks \
  --gtest_also_run_disabled_tests \
  --gtest_filter=SbPlayerEnduranceTest.DISABLED_CreateDestroy \
  --endurance_iterations=10 \
  --endurance_csv=/tmp/player_endurance_smoke.csv
```

## Step 3: Reproduce a Single Endurance Cell

Use `run_android_endurance.py` to run one endurance cell on Android and collect
a diagnostic bundle:

```bash
python3 starboard/benchmark/player/run_android_endurance.py \
  -d <SERIAL> \
  --runner out/android-arm_qa/bin/run_player_benchmarks \
  --bundle-dir /tmp/endurance_single \
  --iterations 1000 \
  --video vertical_1080p_24_fps_399_av1.dmp \
  --audio beneath_the_canopy_aac_stereo.dmp
```

Key options:

- `--gap-ms <int>`: sleep duration in ms between `SbPlayerDestroy` and the next
  `SbPlayerCreate` (default: `0`).
- `--seeks <int>`: seeks per iteration before teardown (default: `0`).
- `--output-mode {punch_out,decode_to_texture}`: `SbPlayer` output mode
  (default: `punch_out`).
- `--color-mode {default,hdr10,bt601,abr_bt709_bt601}`: overrides container and
  sample `SbMediaColorMetadata` so color-space and HDR transitions can be
  exercised deterministically using standard SDR `.dmp` clips:
  - `default`: keeps the `.dmp` clip's native color metadata (`BT.709` SDR).
  - `hdr10`: stamps `BT.2020` / `SMPTE ST 2084 (PQ)` (`max_cll=1000`,
    `max_fall=200`) on `SbPlayerCreationParam` and video samples.
  - `bt601`: stamps `SMPTE 170M` (`BT.601` limited-range SDR) on
    `SbPlayerCreationParam` and video samples.
  - `abr_bt709_bt601`: starts each player at `BT.709` and alternates video
    sample metadata between `BT.601` and `BT.709` across successive seeks
    (`--seeks > 0`), reproducing `1080p BT.709 <-> 480p BT.601` ABR switches.
- `--experimental-features <Key=int,...>`: comma-separated Starboard
  experimental feature overrides applied to every created player (for example,
  `Media.EnableFlushDuringSeek=1`). Run each feature configuration in its own
  process invocation.
- `--snapshot-every <N>` / `--anomaly-ms <ms>`: periodic `dumpsys` capture
  every `N` iterations, and capture on failed iterations or when
  `create_to_presenting_ms`, `seek_max_ms`, or `destroy_ms` reaches the
  threshold.
- `--stop-package <pkg>` / `--reboot-before`: optional pre-run cleanup.

## Step 4: Reproduce the Full Baseline Endurance Matrix

Use `run_android_endurance_matrix.py` with
`starboard/benchmark/player/endurance_matrix_example.json` to run the full
multi-codec sweep unattended (single-clip `1000`-iteration churn across AV1,
VP9, and AVC at `1080p`/`720p`/`480p`, 3-clip cross-codec rotation, `5`-seek
churn, a `10,000`-iteration soak, and conditional `50`/`200`/`1000` ms gap
follow-up cells).

1. Preview per-cell commands without touching disk or `adb`:

   ```bash
   python3 starboard/benchmark/player/run_android_endurance_matrix.py \
     --matrix starboard/benchmark/player/endurance_matrix_example.json \
     -d <SERIAL> \
     --out-dir /tmp/endurance_matrix \
     --runner out/android-arm_qa/bin/run_player_benchmarks \
     --dry-run
   ```

2. Execute the matrix (or pass `--only '<glob>'`, such as `--only 'churn_*'`, to
   run a subset):

   ```bash
   python3 starboard/benchmark/player/run_android_endurance_matrix.py \
     --matrix starboard/benchmark/player/endurance_matrix_example.json \
     -d <SERIAL> \
     --out-dir /tmp/endurance_matrix \
     --runner out/android-arm_qa/bin/run_player_benchmarks
   ```

3. Generate JSON and Markdown summary reports:

   ```bash
   python3 starboard/benchmark/player/summarize_endurance.py \
     --matrix starboard/benchmark/player/endurance_matrix_example.json \
     --bundles-dir /tmp/endurance_matrix \
     --out-json /tmp/endurance_matrix/summary.json \
     --out-md /tmp/endurance_matrix/summary.md
   ```

### Resuming Interrupted Matrix Runs

Re-running the exact same `run_android_endurance_matrix.py` command on an
existing `--out-dir` resumes automatically:

- Any cell (or `run_1..run_N` sub-bundle when `repeat` is set) whose
  `manifest.json` and `results.csv` show a complete run (`setup_error` is
  `null`, `runner_timed_out` is `false`, and all requested rows are present or
  the test stopped early on a `*_hung` row) is skipped.
- Any incomplete bundle directory is moved aside to
  `<out-dir>/_incomplete/<name>.<timestamp>` and re-run; prior bundles are never
  deleted.
- Conditional cells (`run_if_any_failed`, `run_if_any_stalled`) run only when a
  referenced earlier cell recorded a failure or a stall (`>= --stall-ms`,
  default `1500` ms, on `create_to_presenting_ms`, `seek_max_ms`, or
  `destroy_ms`); otherwise they are recorded as `not_triggered`.
- Progress is recorded in `<out-dir>/matrix_state.json` and
  `<out-dir>/matrix.log`.

## Step 5: Reproduce Color-Metadata / Flushed-Seek A/B Comparisons

To evaluate a Starboard experimental feature flag (replace `<FEATURE_FLAG>=1`
below with the flag under test) across HDR10 startup, `BT.601` SDR startup, and
flushed-seek `BT.709 <-> BT.601` ABR transitions:

1. Create a paired matrix JSON file (for example, `/tmp/ab_matrix.json`) using
   `pair` and `arm` (`"off"` vs `"on"`):

   ```json
   [
     {
       "name": "pair1_vp9_hdr10_off",
       "pair": "pair1_vp9_hdr10",
       "arm": "off",
       "iterations": 200,
       "video": "vertical_1080p_24_fps_248_vp9.dmp",
       "audio": "beneath_the_canopy_aac_stereo.dmp",
       "color_mode": "hdr10"
     },
     {
       "name": "pair1_vp9_hdr10_on",
       "pair": "pair1_vp9_hdr10",
       "arm": "on",
       "iterations": 200,
       "video": "vertical_1080p_24_fps_248_vp9.dmp",
       "audio": "beneath_the_canopy_aac_stereo.dmp",
       "color_mode": "hdr10",
       "experimental_features": "<FEATURE_FLAG>=1"
     },
     {
       "name": "pair2_avc_bt601_off",
       "pair": "pair2_avc_bt601",
       "arm": "off",
       "iterations": 200,
       "video": "beneath_the_canopy_135_avc.dmp",
       "audio": "beneath_the_canopy_aac_stereo.dmp",
       "color_mode": "bt601"
     },
     {
       "name": "pair2_avc_bt601_on",
       "pair": "pair2_avc_bt601",
       "arm": "on",
       "iterations": 200,
       "video": "beneath_the_canopy_135_avc.dmp",
       "audio": "beneath_the_canopy_aac_stereo.dmp",
       "color_mode": "bt601",
       "experimental_features": "<FEATURE_FLAG>=1"
     },
     {
       "name": "pair3_av1_abr_seek5_off",
       "pair": "pair3_av1_abr_seek5",
       "arm": "off",
       "iterations": 150,
       "seeks": 5,
       "video": "vertical_1080p_24_fps_399_av1.dmp",
       "audio": "beneath_the_canopy_aac_stereo.dmp",
       "color_mode": "abr_bt709_bt601",
       "experimental_features": "Media.EnableFlushDuringSeek=1"
     },
     {
       "name": "pair3_av1_abr_seek5_on",
       "pair": "pair3_av1_abr_seek5",
       "arm": "on",
       "iterations": 150,
       "seeks": 5,
       "video": "vertical_1080p_24_fps_399_av1.dmp",
       "audio": "beneath_the_canopy_aac_stereo.dmp",
       "color_mode": "abr_bt709_bt601",
       "experimental_features": "Media.EnableFlushDuringSeek=1,<FEATURE_FLAG>=1"
     }
   ]
   ```

   *(To compare more than two arms—for example `Default`, `ArmA`, `ArmB`, and
   `ArmAB`—omit `"pair"` and `"arm"` and list one cell per workload/flag
   combination.)*

2. Run the matrix and generate the `ON - OFF` comparison report:

   ```bash
   python3 starboard/benchmark/player/run_android_endurance_matrix.py \
     --matrix /tmp/ab_matrix.json \
     -d <SERIAL> \
     --out-dir /tmp/endurance_ab \
     --runner out/android-arm_qa/bin/run_player_benchmarks

   python3 starboard/benchmark/player/summarize_endurance.py \
     --matrix /tmp/ab_matrix.json \
     --bundles-dir /tmp/endurance_ab \
     --out-json /tmp/endurance_ab/summary.json \
     --out-md /tmp/endurance_ab/summary.md
   ```

## What a Bundle Contains and How to Read the Output

Each cell bundle directory (`<out-dir>/<cell_name>/`) contains:

- `results.csv`: per-iteration `SbPlayer` API timings, `status`, `error_code`,
  and `error_message`, plus `#` header lines recording the runtime configuration
  (`video`, `audio`, `output_mode`, `color_mode`, `experimental_features`, etc.).
- `manifest.json`: run arguments, Android device/SoC properties, `boot_id`
  continuity check, wall duration, exit codes, CSV status counts, and snapshot
  inventories.
- `runner.log`: combined stdout and stderr of `run_player_benchmarks`.
- `logcat.txt`: continuous `adb logcat -v threadtime` across the run (useful for
  counting hardware `MediaCodec` creations, `MediaCodec::reclaim` calls, and
  vendor HAL transitions).
- `display.txt`, `meminfo_start.txt`, `meminfo_end.txt`, and `dumpsys/`:
  system display/memory dumps plus periodic (`iter_NNNNN_meminfo_pkg.txt`,
  `iter_NNNNN_codec.txt`, `iter_NNNNN_resource_manager.txt`) and anomaly
  (`anomaly_iter_NNNNN_<reason>_*.txt`) snapshots.

In `summary.md` and `summary.json`:

- **Status per iteration:** `ok` when the iteration reaches
  `kSbPlayerStatePresenting`, completes any requested seeks, and returns from
  `SbPlayerDestroy` without error; otherwise a step-specific failure or watchdog
  `*_hung` status (`create_failed`, `initialize_failed`, `presenting_failed`,
  `seek_failed`, `error`, `create_hung`, `presenting_hung`, `seek_hung`,
  `destroy_hung`, etc.). The last row written for an iteration is authoritative.
- **Timings at the `SbPlayer` API boundary (ms):** `gap_ms` (previous
  `SbPlayerDestroy` return to `SbPlayerCreate` start), `create_ms`
  (`SbPlayerCreate` synchronous call duration), `create_to_initialized_ms` and
  `create_to_presenting_ms` (`SbPlayerCreate` start to asynchronous
  `kSbPlayerStateInitialized` and `kSbPlayerStatePresenting` callbacks),
  `seek_median_ms` and `seek_max_ms` (`SbPlayerSeek` call to asynchronous
  `kSbPlayerStatePresenting` callback when `seeks > 0`), and `destroy_ms`
  (`SbPlayerDestroy` synchronous call duration). Iteration `0` includes one-time
  process warm-up and is reported separately (`cold_row0`); steady-state
  distributions (`median`, `p95`, `p99`, `max`) use `status == "ok"` rows
  `1..N-1`.
- **Findings thresholds:**
  - **Failure:** any non-`ok` iteration (`failed_count > 0`).
  - **Stall:** any `ok` iteration with `create_to_presenting_ms`,
    `seek_max_ms`, or `destroy_ms` `>= --stall-ms` (default `1500` ms).
  - **Drift:** steady `create_to_presenting_ms` last-100 `ok` median minus
    first-100 `ok` median exceeding both `--drift-ms` (default `50` ms) and
    `--drift-pct` (default `25%`) (`N/A` when fewer than 200 steady `ok` rows).
  - **Memory growth (`pss_growth`):** periodic `:test_process` package PSS delta
    `> --pss-growth-mb` (default `20` MB).
- **Paired `ON - OFF` comparisons:** When pairs of cells share `"pair"` with
  `"arm": "off"` and `"arm": "on"`, Section 3 reports `ON - OFF` steady deltas
  for `create_to_presenting_ms`, `seek_median_ms` (when `seeks > 0`), and
  `destroy_ms`.

## Operational Notes

- **Result path for direct gtest runs:** When invoking `run_player_benchmarks`
  directly on Android without `run_android_endurance.py`, pass an absolute
  writable path such as `--endurance_csv=/sdcard/Download/endurance.csv`
  (`run_android_endurance.py` handles Android 11+ scoped storage paths and CSV
  pulling automatically).
- **Test APK replacement:** Running `player_benchmarks` on Android installs its
  `org.chromium.native_test` APK, replacing any APK with that package name
  already installed on the device.
