# Cobalt Binary Size Analysis Suite

This directory serves as the centralized suite of assessment and diagnostic tools designed to analyze Cobalt's binary footprint, and facilitate the surgical reduction of binary size bloat.

## Overview

Cobalt inherits a massive amount of upstream code from Chromium. To meet strict release constraints on embedded devices, unnecessary features and sub-components must be continually audited and stripped.

Because automated code refactoring introduces severe compilation and runtime risks, binary size reduction follows a **developer-driven manual workflow** guided by static assessment tooling.

---

## Common Input Generation (SuperSize Profiles)

Most tools in this suite rely on Chromium's SuperSize (`.size`) files to map the Proportional Set Size (PSS) byte contributions of compiled symbols.

### 1. Building the Release Binary
Always compile a release build of Cobalt using `autoninja` to ensure the binary reflects true production optimizations and linker dead-stripping:

```bash
autoninja -C out/android-arm_gold cobalt
```

### 2. Archiving the Size File
Use Chromium's SuperSize tool to archive the compiled artifacts into a `.size` file.

*   **For Android builds** (profiling the shared library or APK):
    ```bash
    python3 tools/binary_size/supersize archive cobalt27.size \
      -f out/android-arm_gold/lib/libcobalt.so -v
    ```
*   **For Linux builds**:
    ```bash
    python3 tools/binary_size/supersize archive cobalt.size \
      -f out/Release/cobalt -v
    ```

For more details on diffing builds (`.sizediff`) or launching the web visualizer, consult the official [SuperSize Playbook](https://chromium.googlesource.com/chromium/src/tools/+/HEAD/binary_size/libsupersize/README.md).

---

## Tooling Index

Below is the index of available diagnostic and analysis tools in this suite.

| Tool | Primary Mission |
| :--- | :--- |
| **`analyze_feature_disable_difficulty.py`** | Audits build graph reachability and C++ caller coupling to score the difficulty of manually removing a feature. |
| **`analyze_size_diff.py`** | SuperSize console script to compute absolute net PSS overhead, subsystem-level growth, and targeted third-party libraries driving bloat between two build snapshots. |

---

## Tool Details

### 1. Feature Complexity Assessment (`analyze_feature_disable_difficulty.py`)

This script acts as an advanced audit diagnostic. It analyzes SuperSize profiles, traverses the global GN build graph, and audits C++ source files to output an actionable refactoring blueprint for developers.

#### Input Modes
Select one of the mutually exclusive target modes to map a feature's footprint:

*   `--path` - Analyze all targets under a specific folder path.
    ```bash
    python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
      --path services/on_device_model
    ```
*   `--namespace` - Analyze symbols declared within a specific C++ namespace.
    ```bash
    python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
      --namespace optimization_guide
    ```
*   `--component` - Analyze a SuperSize component category.
    ```bash
    python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
      --component Blink>Language
    ```
*   `--symbol` - Trace a specific compiled symbol name.

#### Optional Arguments
*   `--size_file` - Path to the generated `.size` file (defaults to `cobalt27.size`).
*   `--build_dir` - Path to the GN build directory (defaults to `out/android-arm_gold`).
*   `--root_target` - Top-level target to check paths from (defaults to `//cobalt:gn_all`).
*   `--json` - Export a structured JSON automation report.
*   `--verbose` - Enable verbose logging of subprocess calls.

#### Interpreting Reports (The Chain of Discovery)
To cleanly remove a feature using the assessment report, follow this human-in-the-loop workflow:
1.  **Identify Funnel Targets**: Open the `BUILD.gn` files for the direct preceding callers listed under `funnel_targets`.
2.  **Trace Feature Flags**: Read the calling C++ files in those funnel layers to locate the C++ runtime feature flags (e.g., `kOptimizationGuideOnDeviceModel`) used to gate the feature.
3.  **Grep Expansion**: Search the repository for the flag strings and base names to uncover companion WebUI registration files, Mojo interfaces (`.mojom`), and uncompiled resource bundles (`.pak`) that SuperSize missed.
4.  **Surgical Subtraction**: Apply conditional subtractions (`deps -=`) inside the correct conditional blocks in `BUILD.gn`, and wrap C++ invocation blocks in `#if !BUILDFLAG(IS_COBALT)`.

#### Criteria for Feature Removal Suitability
A feature is highly suitable for manual removal if its report satisfies the following profile:
*   **Reachable**: `True` (Unreachable features provide zero size benefit).
*   **Weight Contribution**: $> 500 \text{ KB}$ PSS (Justifies refactoring maintenance overhead).
*   **Target Classification**: `DEDICATED` (Perfectly encapsulated inside its own directory).
*   **Build Coupling**: $\le 3$ Funnel Targets (Requires editing very few `BUILD.gn` files).
*   **C++ Surface**: $\le 5$ Include Integration Points (Minimal macro-gating required).

---

### 2. Snapshot Diffing (`analyze_size_diff.py`)

Generates high-level binary footprint diffs between two compiled snapshots (e.g., across branches, pull requests, or release upgrades) to audit net growth. It runs inside the SuperSize console REPL.

#### Usage Workflow
1. **Generate the Diff Archive**:
   ```bash
   ./tools/binary_size/supersize diff rdk_26.size rdk_27.size --output rdk_26_27.sizediff
   ```
2. **Run the Analysis**:
   ```bash
   ./tools/binary_size/supersize console rdk_26_27.sizediff --query='exec(open("cobalt/tools/binary_size/analyze_size_diff.py").read())'
   ```

#### Output Capabilities & Feature Removal Guidance
* **Net PSS Overhead**: Measures absolute growth via `delta.raw_symbols.pss` to capture alignment padding and unmapped additions accurately.
* **Subsystem Slicing**: Aggregates net diffs by major upstream Chromium component buckets (e.g., `v8`, `Blink`, `net`) to trace high-level architectural inflation.
* **Newly Added Component Gating**: Isolates brand-new symbols added in the modified snapshot and maps them directly to official functional component tags. This clearly highlights entire new upstream feature additions driving binary bloat.
* **Third-Party Target Isolation**: Filters specifically for external libraries under `third_party/` growing by $\ge 5\text{ KB}$. Because external code is heavily decoupled, these libraries represent low-hanging fruit for surgical unbundling (via high-level GN feature gating) if their underlying functionalities are not needed.

---
