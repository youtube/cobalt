# Cobalt Android UMA Experiment Harness (`cobalt/tools/uma/experiment/`)

**Note:** All scripts in this directory are implemented for **Android only**.

Paired ABBA experiment runner (`android_ab_harness.py`), single-launch Android
CDP runner (`android_cdp_puller.py`), statistical report generator
(`android_ab_report.py`), and shared UMA snapshot/WPR helpers
(`android_uma_dump.py`) for verifying Cobalt storage, cache, startup, and memory
`BASE_FEATURE` flags on Android devices via UMA histograms over Chrome DevTools
Protocol (CDP).

## Verification Modes (`homefeed` & `wpr`)

- **Basic YouTube TV Home Cold-Start + Lifecycle Exit CUJ (`--mode homefeed` or
  `--mode wpr`)**:
  - Boots YouTube TV on Android (`https://www.youtube.com/tv`,
    `--url-params=force_startup_screen=none,hl=en,gl=US`) with **0 DPAD
    scrolling** by default (`no_dpad_nav=True`, `pairs=3`, `exit_flush=True`),
    either over live network (`--mode homefeed`) or deterministic Web Page
    Replay (`--mode wpr`).
  - Exercises real storage (`Local Storage`, `Cookies`) and cache (`HttpCache`,
    V8 `Code Cache`, `GrShaderCache`) traffic alongside startup, congestion, and
    memory guardrails.
  - At the end of the window, snapshots `S1` (in-session), triggers
    `h5vcc.system.exit()`, waits `3s`, and snapshots `S2` so `S2 - S1`
    captures the lifecycle exit flush.
  - When `--warmup-runs 1` is passed (for warm-cache / post-cleanup Launch #2
    verification), each warmup launch sends `KEYCODE_HOME` + `3s` settle
    before `stop_package()` so `SimpleCache` writes `the-real-index` and
    `GrShaderCache` flushes compiled shaders to disk.

## Example Usage (Android Only)

```bash
vpython3 cobalt/tools/uma/experiment/android_ab_harness.py \
    --serial localhost:40501 \
    --results-dir out/ab/localstorage \
    --experiment ls_home65 \
    --mode homefeed \
    --pairs 3 \
    --poll-interval-s 65 \
    --exit-flush \
    --arm-a-flags=--disable-features=CobaltLocalStorageOptimization \
    --arm-b-flags=--enable-features=CobaltLocalStorageOptimization

python3 cobalt/tools/uma/experiment/android_ab_report.py \
    --results-dir out/ab/localstorage \
    --experiment ls_home65 \
    --out out/ab/localstorage/ls_home65_report.md
```

## Running Unit Tests

```bash
python3 -m unittest discover -s cobalt/tools/uma/experiment -p "*_test.py" -v
```
