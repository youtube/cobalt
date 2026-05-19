# Cobalt Memory Significance Verification Rig

This directory contains the official Cobalt performance tooling suite for orchestrating automated memory experiments on Android devices, capturing system PSS/RSS allocations, and mathematically verifying the **statistical significance** of memory footprint optimizations.

---

## 1. Tooling Overview

### 📊 `verify_stat_sig.py`
The main campaign orchestrator. It runs the Watch Critical User Journey (CUJ) sequentially across two groups—**Baseline** (standard configuration) and **Experiment** (optimized configuration)—capturing system memory metrics and evaluating statistical significance.
- **Dual-Telemetry Engine:** Operates in a complete vacuum utilizing system-wide Bionic libc memory footprints (`dumpsys meminfo`), and automatically detects and extracts granular UMA breakout histograms via CDP if active.
- **Mathematical Rigor:** Rejects random background process noise by applying a non-parametric Bootstrap Permutation Test and a Directional Sign Test to calculate p-values with high confidence.

### 🔬 `simulate_significance.py`
The simulation, verification, and calibration engine. It runs synthetic normal distribution comparisons and A/A hypothesis testing in a vacuum to calibrate the sensitivity and false-positive rate of the statistical calculations.

---

## 2. Actionable Guidelines for Campaign Runs

### default: The Sweet Spot ($N = 5$ runs)
For standard, high-impact optimizations (e.g., capping Skia font caches, reducing MSE video/audio buffer budgets), **5 runs per group is the recommended default**.
- **Duration:** A complete 10-run campaign ($5$ Baseline + $5$ Experiment) plays video for 60 seconds per run, completing in exactly **12 minutes**.
- **Permutation Limit:** At $N=5$, there are $\binom{10}{5} = 252$ possible permutations, allowing the engine to calculate an absolute minimum p-value of $\approx 0.004$, which easily passes the critical threshold ($\alpha = 0.05$).

### Tiny Optimizations ($N \ge 15$ runs)
If you are testing a very subtle, low-impact code optimization (e.g., reducing a cache by $< 3$ MB), a small sample size ($N=5$) is highly underpowered and will likely report "Not Significant" due to background noise.
- **Action:** Instruct the rig to run **15 to 20 runs** per group:
  ```bash
  python3 verify_stat_sig.py --device=localhost:38879 --runs=15 --duration=60
  ```
- **Duration:** Completes in approximately 35 to 45 minutes.
- **Power:** Drastically increases the statistical power, allowing the engine to detect differences as small as $0.3$ standard deviations!

---

## 3. Under the Hood: Statistical Foundations

### Welch's t-test vs. Bootstrap Permutation Resampling
Standard parametric tests (like Student's/Welch's t-test) assume that memory allocations follow a perfectly normal Gaussian distribution. However, Smart TV heap allocations are highly skewed by garbage collection, background socket packets, and CPU scheduling spikes.

To guarantee absolute mathematical correctness, our engine uses an **Exact Bootstrap Permutation Test**:
1.  **Observed Difference:** Calculates the mean memory footprint difference between the two groups: $d_{obs} = |\mu_1 - \mu_2|$.
2.  **Null Hypothesis Pool:** Combines all $2N$ observations into a single pooled list, representing the state where the configuration changes had zero effect.
3.  **Permutation Shuffling:** Shuffles the pooled list and splits it back into two synthetic groups $10,000$ times, calculating the mean difference $d_{perm}$ for every shuffle.
4.  **p-value Calculation:** The absolute **p-value** is the proportion of shuffles where the random difference $d_{perm}$ was greater than or equal to our observed difference $d_{obs}$:
    $$p = \frac{\sum [|d_{perm}| \ge |d_{obs}|]}{10000}$$
5.  **Evaluation:** If $p < 0.05$, we reject the null hypothesis and declare the optimization **Statistically Significant** with $>95\%$ confidence.

### Why page cache flushing is mandatory
Smart TV operating systems cache aggressively. If page and inode caches are not cleared between benchmark runs, memory metrics are highly polluted, and process variance ($\sigma$) can spike to **`30 MB`**! Under 30MB of noise, a 12MB optimization is statistically undetectable.

To minimize standard deviation, our rig forcefully syncs filesystems and flushes page caches on the device before **every single run**:
```bash
adb shell sync && adb shell "echo 3 > /proc/sys/vm/drop_caches"
```
This reduces background standard deviation to **`~5 MB`**, giving the statistical engine supreme precision and power.

---

## 4. Verification & Calibration Commands

### Run Synthetic Simulations
To verify that the significance calculations correctly identify optimizations and reject noise under different variance scenarios, execute the case suite:
```bash
python3 simulate_significance.py
```

### Run A/A False Positive Calibration
To calibrate the significance threshold and verify that the engine does not generate false positives when comparing identical configurations, run 100 A/A iterations:
```bash
python3 simulate_significance.py --aa-test
```
- **Expected Result:** The empirical false-positive rate should align closely with your significance threshold (e.g., $\sim 3\%$ to $5\%$ false positives for $\alpha=0.05$).
