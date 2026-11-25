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
  - Boots YouTube TV on Android at Home, with no deep link
    (`https://www.youtube.com/tv`). `dev.cobalt.coat` gets that URL as `--url=`
    in `commandLineArgs`; other packages, such as
    `com.google.android.youtube.tv`, get a `VIEW` intent with the URL as data
    (`-d`). The `--url-params` (default
    `force_startup_screen=none,hl=en,gl=US`) go in the `url_params` intent
    extra, and Cobalt appends them to the startup URL (the `--url=` argument,
    else the URL in the app manifest). In `--mode homefeed` the landing gate
    rejects a run whose page is not Home (`#/watch` in the URL) or lacks these
    params.
  - **0 DPAD scrolling** by default (`no_dpad_nav=True`, `pairs=6`,
    `exit_flush=True`), either over live network (`--mode homefeed`) or
    deterministic Web Page Replay (`--mode wpr`).
  - Every launch, in both arms, also gets `--remote-allow-origins=*` and
    `--enable-features=CobaltMetricsInterval:memory-metrics-interval/10` in
    `commandLineArgs`. That is a memory dump every 10 s (production default:
    the feature is disabled and its interval is 300 s); it is why `Memory.*`
    gauges have samples in short runs. Each run's `.log` records the effective
    `Launch command: ...`. Only non-release builds honor `commandLineArgs`.
  - Exercises real storage (`Local Storage`, `Cookies`) and cache (`HttpCache`,
    V8 `Code Cache`, `GrShaderCache`) traffic alongside startup, congestion, and
    memory guardrails.
  - At the end of the window, snapshots `S1` (in-session), triggers
    `h5vcc.system.exit()`, waits `3s`, and snapshots `S2` so `S2 - S1`
    captures the lifecycle exit flush.
  - When `--warmup-runs 1` is passed (for warm-cache / post-cleanup Launch #2
    verification), `android_ab_harness.py` automatically captures Warmup Launch
    #1 into `<run>.w1.txt` (`warmup_1` phase in `android_ab_report.py`) before
    sending `KEYCODE_HOME` + `3s` settle + `stop_package()`, so a single ABBA
    session reports both transitional Launch #1 and steady-state Launch #2.

## Arm Flags

- Separate the switches in `--arm-a-flags` / `--arm-b-flags` (spec keys
  `arm_a_flags` / `arm_b_flags`) with commas, never spaces, e.g.
  `--arm-b-flags=--enable-features=A:p/1,B,--disable-features=C`. An item
  without a leading `--` extends the previous feature list. A space before a
  second switch silently corrupts it (it becomes `--B --disable-features=C`).

## Pairs and Significance

- `android_ab_report.py` tags a row `[IMPROVED]`, `[REGRESSED]` or `[CHANGED]`
  only when the exact (binomial order-statistic) 95% CI of the median paired
  difference excludes 0. That CI exists only for n >= 6 pairs: with fewer
  pairs every changed row is `[NOISE]`, and the report shows the min-max range
  of the differences, labeled `range`, instead of a CI.
- For A/B runs, use an even number of pairs, at least 6 (the default): an even
  count gives each arm the first slot equally often (AB, BA, ...).

## Example Usage (Android Only)

```bash
vpython3 cobalt/tools/uma/experiment/android_ab_harness.py \
    --serial localhost:40501 \
    --results-dir out/ab/localstorage \
    --experiment ls_home65 \
    --mode homefeed \
    --pairs 6 \
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
