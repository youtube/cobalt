# Cobalt Memory Significance Verification Rig & Win Hunting Pipeline

This directory contains the official Cobalt performance tooling suite for orchestrating automated memory profiling experiments, mathematically verifying the **statistical significance** of optimizations, and systematically hunting for memory footprint wins across discovered configuration switches.

---

## 1. Tooling Architecture

### Harvester (`harvest_flags.py`)
Dynamically extracts togglable configuration switches and feature flags from both upstream Blink/Chromium and Cobalt-specific C++ source files (`cobalt/browser/switches.cc`, `cobalt/shell/common/shell_switches.cc`, `cobalt/browser/features.cc`, etc.).
*   **Double-Token Filtering:** Parses both variable names and string values to exclude purely numeric constants (`0`, `1`), constant value lists (containing `_`), and non-hyphenated helpers, guaranteeing zero false-positives in the final stack.

### Sweeper Orchestrator (`win_hunter.py`)
The master sweep coordinator. It systematically loops through discovered flags in `scanned_flags.txt`, runs programmatically configured Baseline vs. Experiment campaigns using `verify_stat_sig.py`, and outputs a ranked leaderboard.
*   **Exploration Sweeps:** Exposes `--shuffle` to randomly sample harvested flags, resolving alphabetical bottlenecks.
*   **Targeted Scans:** Exposes `--filter` to sweep flags matching specific regex queries (e.g. `--filter "gpu|canvas"`).
*   **JSON Results Integration:** Reads structured campaign outputs dynamically via JSON, completely avoiding fragile stdout parsing.

### Primary Runner (`verify_stat_sig.py`)
The significance campaign runner. It loops over a selected CUJ (or loops sequentially over all 4 CUJs if `--cuj all` is requested), flushes page caches on the target device, and runs A/B resampled statistical calculations.
*   **100% Linter Compliant:** Features clean imports, zero pylint overrides, specific exception catches, and standard lazy-logging formatted calls.
*   **Wait-For-Port TCP Polling:** Active socket loops (`wait_for_devtools_port`) deterministically confirm DevTools loopback port initialization, replacing fragile sleeps.
*   **Leaking & Security Fixes:** Subprocess execution uses secure list command arrays (`shell=False`) and open context managers to cleanly release parent file descriptors.
*   **Dynamic Logcat Fallback:** Parses diagnostics line-by-line from device adb streams, ensuring graceful CDP connection recovery.

### Consolidated Math Library (`stats_utils.py`)
Eliminates mathematical code duplication by providing shared bootstrap `calculate_stats` and non-parametric `permutation_test_p_value` helpers.

### Background Scroller Helper (`scroll_cdp.py`)
Lightweight WebSocket scroller driver that dispatches D-pad Down keyboard events to simulate automated browsing during random navigation CUJ scenarios.

---

## 2. Automated Memory Win Hunting Pipeline (3-Phase Sweep)

The rig includes a fully automated pipeline to systematically search the Cobalt codebase for hidden memory optimizations:

### 🛰️ Phase 1: Identify Flags to Experiment With
Run the Harvester static analyzer to parse the C++ codebase and output discovered configurations to `scanned_flags.txt`:
```bash
python3 harvest_flags.py
```

### 🔬 Phase 2: Systematic Sweep Experiments
Launch the Sweeper Orchestrator to run Baseline vs. Experiment campaigns for each discovered flag. Use `--shuffle` to explore different flags randomly:
```bash
python3 win_hunter.py --runs=3 --duration=45 --max-flags=20 --shuffle
```
*To execute a targeted sweep on a specific subsystem (e.g. only GPU and Canvas flags):*
```bash
python3 win_hunter.py --runs=3 --duration=45 --filter "gpu|canvas" --enable-granular-memory
```

### 📊 Phase 3: Sorted Footprint Reductions Leaderboard
At the end of the sweeps campaign, the pipeline automatically sorts the results by **Mean Footprint Savings** and writes a ranked markdown report directly to:
`win_leaderboard.md`

An example generated leaderboard:
| Rank | Optimization switch / feature | Type | p-value | paired consistency | Status | **Mean Memory savings** |
| :---: | :--- | :---: | :---: | :---: | :---: | :--- |
| 1 | `kSmallerInterestArea` | Feature | 0.0024 | PASS | 🟢 SIGNIFICANT | **14.32 MB** |
| 2 | `--disable-gpu-rasterization` | Switch | 0.0115 | PASS | 🟢 SIGNIFICANT | **8.50 MB** |
| 3 | `kExperimentConfigExpiration` | Feature | 0.8421 | FAIL | 🔴 NOT SIG | **0.04 MB** |

---

## 3. Stand-alone Significance Campaigns

To run a targeted significance campaign comparing baseline vs. experiment configurations specifically on a single CUJ (e.g., `watch` video playback):
```bash
python3 verify_stat_sig.py --runs=5 --duration=60 --cuj=watch
```

### Optional Granular Memory Breakdowns
To scrape and report detailed Cobalt internal sub-allocators (`Memory.Cobalt.AllocationVolume.*`):
```bash
python3 verify_stat_sig.py --cuj=watch --enable-granular-memory
```

---

## 4. Manual Local A/B Testing & Custom Flags Cheat Sheet

To run a manual A/B statistical significance experiment on a local device, you configure and pass different arguments to `--baseline-args` and `--experiment-args`.

Here is a cheat sheet of how to format and enable various types of switches and feature flags:

### A. Toggling Standard Chromium/Cobalt CLI Switches
Standard switches are passed directly (comma-separated).
*   **Baseline (Standard):** default switches.
*   **Experiment (Optimized):** appending your switch (e.g., `--disable-gpu-rasterization` or `--enable-threaded-texture-mailboxes`).
```bash
python3 verify_stat_sig.py \
  --cuj=watch \
  --runs=5 \
  --baseline-args="--remote-allow-origins=*,--enable-metrics" \
  --experiment-args="--remote-allow-origins=*,--enable-metrics,--disable-gpu-rasterization"
```

### B. Toggling C++ BASE_FEATURE Flags (Blink/Content)
C++ feature flags are toggled using the standard `--enable-features` and `--disable-features` switches.
*   **Baseline (Disabled):** `--disable-features=FeatureName`
*   **Experiment (Enabled):** `--enable-features=FeatureName`
```bash
python3 verify_stat_sig.py \
  --cuj=watch \
  --runs=5 \
  --baseline-args="--remote-allow-origins=*,--disable-features=SmallerInterestArea" \
  --experiment-args="--remote-allow-origins=*,--enable-features=SmallerInterestArea"
```

### C. Toggling V8 Engine & JS flags
V8 switches are passed inside the `--js-flags` argument wrapper.
*   **Baseline (Standard):** default JS memory.
*   **Experiment (Capped V8 Heap):** `--js-flags="--max-old-space-size=128"`
```bash
python3 verify_stat_sig.py \
  --cuj=watch \
  --runs=5 \
  --baseline-args="--remote-allow-origins=*" \
  --experiment-args="--remote-allow-origins=*,--js-flags=--max-old-space-size=128"
```

### D. Mixing Multiple Custom Configurations
You can combine switches, features, and V8 options in a single sweep campaign by comma-separating them:
```bash
python3 verify_stat_sig.py \
  --cuj=watch \
  --runs=5 \
  --baseline-args="--remote-allow-origins=*,--enable-metrics" \
  --experiment-args="--remote-allow-origins=*,--enable-metrics,--disable-gpu-rasterization,--enable-features=SmallerInterestArea,--js-flags=--max-old-space-size=128"
```

---

## 5. Style & Linter Compliance

Run the `pre-commit` linter suite on all files to confirm style correctness before staging and committing:
```bash
pre-commit run --files verify_stat_sig.py stats_utils.py simulate_significance.py scroll_cdp.py cuj_definitions.py harvest_flags.py win_hunter.py
```
pylint configurations require strict adherence to lazy logging (`logging.info("msg %s", arg)`). Eager f-strings or string concatenations inside logging calls are strictly forbidden.
