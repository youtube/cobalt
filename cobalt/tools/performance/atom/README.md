# ATOM_PERF measurement tools (PoC)

Scripts for measuring Cobalt speed on an Android TV device over adb, using the
`ATOM_PERF` log lines added by the `atom-perf-logging` branch. All timestamps
are CLOCK_MONOTONIC microseconds (same clock as `base::TimeTicks` and Java
`System.nanoTime()`).

## Usage

```sh
# Install a qa build that contains the ATOM_PERF logging, then:
./run_series.sh <results_dir> 5        # 5 passes x {empty, warm} HTTP cache
./run_once.sh <run_dir> [warm|empty]   # one pass
./parse_atom_perf.py <run_dir> [...]   # timeline (1 dir) or summary (N dirs)
```

Each pass: cold launch -> normalize to HOME -> 9 D-pad presses -> OK (play) ->
BACK -> HOME -> warm launch. The harness writes `ATOM_PERF phase name=<phase>`
markers to logcat, and the parser groups events by phase.

- **Normalize:** the app occasionally starts on the account picker. SELECT,
  SELECT, PLAY, BACK ends on HOME from either state (see `run_once.sh`). Runs
  that started on the picker are flagged (`Launched to account picker`) and
  their cold-launch metrics are skipped.
- **D-pad:** 5 right, 4 left, ending one tile right of the start, so OK opens a
  video the normalize step did not play (replaying it would resume at the last
  position and add a seek).
- With multiple adb devices, set `ANDROID_SERIAL`. `empty` deletes only the HTTP cache (`cache/Cache`); cookies,
login, and the V8 code cache are kept.

## Metrics

| Metric | Start | End |
|---|---|---|
| Cold launch | `process_start` | Visually complete: last `image_paint` before a 3 s quiet period |
| Home UI load | YT app navigation start (`fcp nav_start_us`) | Visually complete |
| Input | D-pad `keydown` created | Next frame presented |
| Browse-to-watch | OK `keydown` created | `kSbPlayerStatePresenting` |
| Warm launch | `activity_start` after stop | `first_frame_after_visible` |
| Memory | - | `dumpsys meminfo` TOTAL PSS on home / in playback |

## Log events

| Event | Source |
|---|---|
| `process_start`, `activity_*`, `splash_hidden`, `main_frame_navigation_started` | `CobaltActivity.java` |
| `image_paint` (first paint of each visible loaded image) | `image_paint_timing_detector.cc` |
| `fcp` (+ `nav_start_us`), `lcp_stopped` | `paint_timing.cc`, `paint_timing_detector.cc` |
| `event` (keydown/keyup timings) | `window_performance.cc` |
| `page_visible`, `first_frame_after_visible` | `document.cc` |
| `player_state` | `starboard_renderer.cc` |
