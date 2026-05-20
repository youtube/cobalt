# Cobalt Memory Significance Verification Rig

This directory contains the Cobalt performance tooling suite for orchestrating automated memory experiments on Linux and Android devices, capturing system PSS/RSS allocations, and mathematically verifying the **statistical significance** of memory footprint optimizations.

---

## 1. Tooling Architecture

### 📊 `verify_stat_sig.py`
The primary significance profiling campaign orchestrator. It runs Baseline (standard flags) and Experiment (optimized flags) configurations sequentially across Critical User Journeys (CUJs), captures sample distributions, and performs non-parametric calculations to calculate statistical significance.
- **Dynamic Scraper & Reporting:** Automatically scrapes target UMA breakout histograms via Chrome DevTools Protocol (CDP). Supports lazy log formatting and is 100% presubmit-compliant.
- **Timing & Sync Hardening:** Uses active TCP loopback polling (`wait_for_devtools_port`) instead of fragile timing sleeps to deterministically verify DevTools loopback port connection.
- **Log-Parsing Fallback:** Line-by-line diagnostics parser that dynamically reads local Linux log files or adb logcat device outputs, formatting them into CDP-scraped CSV lines to ensure graceful CDP connection recovery.
- **Resource Safety:** Bypasses dangerous shell command injection risks via direct safe list-based execution. Closes Popen file descriptors cleanly using standard context managers to prevent leaks.

### ⚙️ `cuj_definitions.py`
Centrally specifies the unified 4-Pillar Cobalt Critical User Journeys:
*   `browse`: BROWSE CUJ (Randomized UI navigation to `https://www.youtube.com/tv`, automatically launching the D-pad scrolling driver subprocess).
*   `watch`: WATCH CUJ (Direct video playback to `https://www.youtube.com/tv#/watch?v=AB-4pS2Og1g`).
*   `baseline`: BASELINE CUJ (Idle `about:blank`).
*   `combined`: COMBINED CUJ (Direct video playback + randomized D-pad page browsing).

### 📜 `stats_utils.py`
Factored mathematical module containing shared helper functions (`calculate_stats` and non-parametric bootstrap `permutation_test_p_value` using an in-place Knuth shuffle step) shared by both scripts to prevent mathematical code duplication.

### 🕹️ `scroll_cdp.py`
Stand-alone automated scroll driver that connects to Cobalt's remote DevTools port over raw TCP sockets and dispatches native D-pad Down Virtual Key events to simulate automated page browsing.

### 🔬 `simulate_significance.py`
The statistical power analyst, simulator, and A/A calibration suite. Uses synthetic normal distribution resampling to verify the engine's precision and false-positive thresholds.

---

## 2. Operational Guide & Command Line Examples

### Single CUJ Significance Campaign
To run a significance campaign comparing baseline vs. experiment configurations specifically on a single named CUJ (e.g., `watch` video playback):
```bash
python3 verify_stat_sig.py --platform=android --runs=5 --duration=60 --cuj=watch
```

### Exhaustive 4-Pillar Campaign Sweeps
To execute Baseline vs. Experiment sweeps sequentially over all 4 Critical User Journeys in a single campaign:
```bash
python3 verify_stat_sig.py --platform=android --runs=5 --duration=60 --cuj=all
```

### Optional Granular Memory Metrics Collection
Standard memory optimizations usually do not have granular metrics enabled by default. To enable the scraping and reporting of detailed Cobalt allocation volumes (`Memory.Cobalt.AllocationVolume.*`), pass the optional command-line flag:
```bash
python3 verify_stat_sig.py --cuj=watch --enable-granular-memory
```

---

## 3. Mapping named CUJs to specific Feature Flags

If a specific Critical User Journey requires **specific, hardcoded feature flags** to exercise its target code path, you can map them directly inside the centralized scenario specification in [cuj_definitions.py](file:///usr/local/google/home/avvall/cobalt/src/cobalt/tools/memory_verification_rig/cuj_definitions.py):

#### 1. Append specific launch flags to the CUJ definition:
```python
# cuj_definitions.py
CUJS = {
    "media_clamp": {
        "name": "5. MEDIA CLAMP CUJ (Buffer limits verification)",
        "url": "https://www.youtube.com/tv#/watch?v=ABC123XYZ",
        "is_random_nav": False,
        # Feature flags specifically associated with this CUJ to exercise this code path!
        "baseline_args": "--mse-video-buffer-size-limit-mb=30,--mse-audio-buffer-size-limit-mb=5",
        "experiment_args": "--mse-video-buffer-size-limit-mb=10,--mse-audio-buffer-size-limit-mb=2",
    },
}
```

#### 2. Execute the scenario cleanly:
```bash
python3 verify_stat_sig.py --cuj=media_clamp
```
The primary orchestrator `verify_stat_sig.py` will automatically merge the CUJ-specific launch flags into your sweep campaign arguments at runtime! If a CUJ does not specify its own launch flags, it seamlessly falls back to the standard global baseline/experiment arguments passed on the command line.

---

## 4. Verification & Style Checking

Run the local `pre-commit` linter suite on the modified files to confirm style compliance before staging and committing:
```bash
pre-commit run --files verify_stat_sig.py stats_utils.py simulate_significance.py scroll_cdp.py cuj_definitions.py
```
pylint and yapf check configurations are strictly checked against the Cobalt/Chromium lazy-logging formatting guides (e.g. using lazy parameters `%s` instead of f-strings inside logging statements). Bypassing logging rules via pylint ignores is forbidden.
