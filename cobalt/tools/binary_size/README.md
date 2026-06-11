# Cobalt Binary Size Analysis Suite

This directory contains the assessment, diagnostic, and planning tools designed to analyze Cobalt's binary footprint and facilitate surgical feature gating and component removal.

---

## 1. Input Generation (SuperSize Profiles)
Most tools in this suite rely on Chromium's SuperSize (`.size`) files to map symbol sizes.

1. **Build the Release Binary**:
   ```bash
   autoninja -C out/android-arm_gold cobalt_apk
   ```
2. **Archive the Size File**:
   * **Android**:
     ```bash
     python3 tools/binary_size/supersize archive cobalt27.size \
       -f out/android-arm_gold/lib/libcobalt.so -v
     ```
---

## 2. Tooling Index & How-To

### A. Feature Complexity Assessment (`analyze_feature_disable_difficulty.py`)
This script audits the global GN build graph and C++ source files to generate an automated refactoring plan (JSON report) for disabling a feature.

#### Usage:
Choose a query strategy depending on how the feature is structured in the code:
* **By Path** (isolated directories):
  ```bash
  python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
    --path "third_party/blink/renderer/modules/ad_auction" --size_file cobalt27.size
  ```
* **By Component** (scattered features - *Highly Recommended*):
  ```bash
  python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
    --component "Blink>AdAuction" --size_file cobalt27.size
  ```
* **By Symbol** (specific entry classes):
  ```bash
  python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
    --symbol "NavigatorAuction" --size_file cobalt27.size
  ```
* **Add `--json <path>`** to export a structured JSON blueprint for downstream scripts/agents:
  ```bash
  python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
    --component "Blink>AdAuction" --json report.json
  ```

#### Understanding the Output:
* **`estimated_size_bytes`**: Potential PSS footprint saved by removing this feature.
* **`targets[].type`**:
  * `DEDICATED`: Feature-only files/folders. Can be removed using simple GN list subtraction (`-=`).
  * `SHARED`: Shares code with core features. Must be pruned dynamically using `filter_exclude()`.
* **`targets[].cpp_integration_audit`**: Lists exactly which core C++ files `#include` feature headers. These must be macro-gated using preprocessor flags.

---

### B. Snapshot Diffing (`analyze_binary_size_diff.py`)
This script compares two compiled build snapshots (e.g., across branches or pull requests) to audit net binary size changes, new components, and third-party dependency growth.

#### Usage:
1. **Generate the Diff Archive**:
   ```bash
   ./tools/binary_size/supersize diff baseline.size modified.size --output diff.sizediff
   ```
2. **Run the Analysis** inside the Supersize console:
   ```bash
   ./tools/binary_size/supersize console diff.sizediff \
     --query='exec(open("cobalt/tools/binary_size/analyze_binary_size_diff.py").read())'
   ```

#### Understanding the Output:
* **Total Diff Metrics**: Shows net size change, added symbols, removed symbols, and changed symbols.
* **Growth by Chromium Component**: Groups positive size growth by high-level subsystem tags (e.g. `v8`, `Blink`, `net`).
* **Newly Added Components / Third-Party Growth**: Isolates new symbols and third-party directories under `third_party/` growing by $\ge 5$ KB, highlighting features or external libraries driving binary inflation.

---

## 3. Implementation Rules & Checklists
Once you have the assessment results, follow the corresponding PR review checklists and gating rules:
* For general feature gating, custom flags, preprocessor directives, and IWYU rules, see: **[Surgical Feature Gating Rules](surgical_feature_gating_rules.md)**.
* For Blink/Renderer module exclusion, Partial IDL interface patterns, and C++ stubbing, see: **[Hybrid Feature Removal Rules](hybrid_feature_removal_rules.md)**.
