# Cobalt Binary Size Analysis Suite

This directory serves as the centralized suite of assessment, diagnostic, and planning tools designed to analyze Cobalt's binary footprint and facilitate the surgical reduction of binary size bloat.

## Overview
Cobalt inherits a massive amount of upstream code from Chromium. To meet strict release constraints on embedded devices, unnecessary features and sub-components must be continually audited and stripped.
Because automated code refactoring introduces severe compilation and runtime risks, binary size reduction follows a **developer-driven manual workflow** guided by the static assessment and diffing tools in this suite.

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

| Tool | Primary Mission |
| :--- | :--- |
| **`analyze_feature_disable_difficulty.py`** | Audits build graph reachability, target coupling classifications, and C++ include dependencies to generate an automated JSON blueprint and score difficulty. |
| **`analyze_size_diff.py`** | SuperSize console script to compute absolute net PSS overhead, subsystem-level growth, and targeted third-party libraries driving bloat between two build snapshots. |

---

## 1. Feature Complexity Assessment (`analyze_feature_disable_difficulty.py`)

This script acts as an advanced audit diagnostic. It analyzes SuperSize profiles, traverses the global GN build graph, and audits C++ source files to output an actionable refactoring blueprint for developers.

### Choosing Search Modes & Strategies
To query the SuperSize database, select one of the mutually exclusive search arguments below depending on how the feature is structured:

```mermaid
graph TD
    Start[Want to analyze a concept?] --> PathCheck{Does the feature have its own folder?}

    PathCheck -->|Yes| Path[Use --path "path/to/folder"]
    PathCheck -->|No| MetaCheck{Does the folder have a DIR_METADATA file?}

    MetaCheck -->|Yes| Component[Use --component "Blink>ComponentName"]
    MetaCheck -->|No| SymCheck{Do you know a main class or function name?}

    SymCheck -->|Yes| Symbol[Use --symbol "ClassName"]
    SymCheck -->|No| Namespace[Use C++ namespace: --namespace "blink::ns"]
```

1. **By Path (`--path`) — *Modular / Encapsulated***
   * **When to use**: When the feature's source code is fully isolated inside a single dedicated folder (e.g. `services/on_device_model/`).
   * **Limitation**: If the feature's source files are scattered across multiple different directories (like WebRTC or Ad Auction), `--path` is highly limited as it will only analyze a single directory, creating massive indexing gaps.
   * **Finding the value**: Find folders matching the concept:
     ```bash
     find third_party/blink/renderer/modules -type d -iname "*auction*"
     ```
   * **Example Command**:
     ```bash
     python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
       --path "third_party/blink/renderer/modules/ad_auction" \
       --size_file cobalt27.size
     ```

2. **By Component (`--component`) — *Recommended for Scattered Features***
   * **When to use**: **This is the most robust and highly recommended mode for complex, scattered, or cross-cutting features** (e.g., `Blink>InterestGroups`, `Blink>WebRTC`). Because ownership component categories are declared in `DIR_METADATA` files inside every subdirectory, this mode performs a global union query that aggregates all associated symbols across diverse repository paths (from `renderer/core`, `renderer/modules` to `content/browser`), preventing any code leakage.
   * **Finding the value**: Look at the `DIR_METADATA` file inside the feature directory for the `monorail` component:
     ```bash
     cat third_party/blink/renderer/modules/ad_auction/DIR_METADATA
     ```
   * **Example Command**:
     ```bash
     python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
       --component "Blink>AdAuction" \
       --size_file cobalt27.size
     ```

3. **By Symbol (`--symbol`) — *Surgical***
   * **When to use**: When the feature code is compiled into shared libraries rather than isolated folders, and you want to target a specific class or entry point.
   * **Finding the value**: Search the codebase for the main class declaration:
     ```bash
     git grep "class.*Auction"
     ```
   * **Example Command**:
     ```bash
     python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
       --symbol "NavigatorAuction" \
       --size_file cobalt27.size
     ```

4. **By Namespace (`--namespace`) — *C++ Scope***
   * **When to use**: When files are mixed inside a shared target, but the C++ code is wrapped in a clear namespace block.
   * **Finding the value**: Look inside any of the feature's C++ source files:
     ```cpp
     namespace blink { namespace ad_auction { ... } }
     ```
   * **Example Command**:
     ```bash
     python3 cobalt/tools/binary_size/analyze_feature_disable_difficulty.py \
       --namespace "blink::ad_auction" \
       --size_file cobalt27.size
     ```

---

### Optional Arguments
*   `--size_file` - Path to the generated `.size` file (defaults to `cobalt27.size`).
*   `--build_dir` - Path to the GN build directory (defaults to `out/android-arm_gold`).
*   `--root_target` - Top-level target to check paths from (defaults to `//cobalt:gn_all`).
*   `--json` - Export a structured JSON automation report.
*   `--verbose` - Enable verbose logging of subprocess calls.

---

### JSON Report Schema & Downstream Automation
When `--json <path>` is specified, the script outputs a structured JSON blueprint used by downstream scripts and AI agents.

```json
{
  "estimated_size_bytes": 962948.1719,
  "feature_flags": [],
  "related_paths": [],
  "targets": [
    {
      "target": "//third_party/blink/common:common",
      "type": "SHARED",
      "difficulty": "LOW",
      "action": "Low complexity (10 pts)...",
      "matched_files": [
        "third_party/blink/common/interest_group/interest_group.cc"
      ],
      "cpp_integration_audit": [
        {
          "file": "content/browser/renderer_host/render_frame_host_impl.cc",
          "referenced_headers": [
            "content/browser/interest_group/ad_auction_document_data.h"
          ]
        }
      ]
    }
  ]
}
```

#### Schema Fields:
*   `estimated_size_bytes`: Total PSS contribution of matching symbols.
*   `targets[].type`: Can be `DEDICATED` (fully isolated), `ASSEMBLY` (mostly isolated), or `SHARED` (co-compiles other code).
*   `targets[].cpp_integration_audit`: A direct audit mapping which non-feature source files `#include` which feature headers.

---

### Criteria for Feature Removal Suitability
A feature is highly suitable for manual removal if its report satisfies the following profile:
*   **Reachable**: `True` (Unreachable features provide zero size benefit).
*   **Weight Contribution**: $> 500 \text{ KB}$ PSS (Declares custom build argument in `public_features.gni` as per **Rule A** in `SKILL.md`; smaller features (< 500 KB) can use global `is_cobalt` directly).
*   **Target Classification**: `DEDICATED` (Perfectly encapsulated inside its own directory; requires simple GN list subtraction).
*   **Build Coupling**: $\le 3$ Preceding Targets (Requires editing very few `BUILD.gn` files).
*   **C++ Surface**: $\le 5$ Include Integration Points (Minimal macro-gating required).

---

### Algorithmic Limitations & Gaps of the Analysis Tool
Developers must be aware of **five critical limitations** of the static analysis tool. Because it relies strictly on static C++ `#include` parsing and production-compiled SuperSize C++ symbols, it is blind to several key Chromium/Blink pipelines:

#### 1. Omission of the Web IDL & V8 Bindings Pipeline
Web IDL files (`.idl`) describe the APIs exposed to JavaScript. Blink processes these at build time to auto-generate massive C++ binding translation units.
* **The Blocker**: Since IDL files and `.gni` lists are pre-compilation files, they do not generate SuperSize database records. The script will **never** instruct you to modify bindings paths. If you only follow the script's C++ instructions, V8 binding generators will fail to compile because they still try to generate bindings for feature classes that no longer exist.
* **Developer Fix**: You must manually open `bindings/idl_in_modules.gni` and bindings lists to filter out the feature's IDL assets.

#### 2. Exclusion of Unit Test Suites and Mocking Code
Test-runners (e.g., `blink_unittests`) compile code separately from production executables (`libchrobalt.so`).
* **The Blocker**: The script maps reachability only from the production root target (e.g., `//cobalt:gn_all`). It is completely blind to files suffixed with `_test.cc` or `_unittest.cc`. When you gate or subtract the production targets, the unit tests compiled in separate test targets will break because they still try to compile feature test files.
* **Developer Fix**: You must manually search for test-only files under the feature's path and wrap or subtract them inside the `source_set("unit_tests")` target blocks in parent directories.

#### 3. Blindness to Transitive GN Graph Dependencies
Some parent targets include the feature under their `deps` block simply to inherit public compilation configs or library linkages, even if they don't directly include the feature's headers in their C++ source code.
* **The Blocker**: The include scanner only flags parent targets whose `.cc` source files contain a literal `#include` line to the feature. If B depends on A for configs and you remove A, B's compilation might break, but the script will report $0$ includes for B.
* **Developer Fix**: Run `gn refs out/<dir> //path/to/feature` manually to identify all parent targets that list the feature in their `deps` or `public_deps`.

#### 4. Blindness to Mojo IPC Interfaces & Dependency Injection
Chromium decouples code heavily via Mojo IPC interfaces and runtime dependency injection (like registries or abstract factories).
* **The Blocker**: Renderer files and browser controllers communicate using auto-generated Mojo interfaces (e.g. `#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom-blink.h"`). They do **not** directly include the C++ implementation class header (e.g., `ad_auction_service_impl.h`). Because the script only matches production implementation headers, it reports **$0$ includes/integration points** for the Mojo caller.
* **Developer Fix**: Always check for `.mojom` files inside your feature directory. Locate where these interfaces are registered in Mojo binder maps (e.g., `browser_interface_binders.cc`) and manually wrap those bindings.

#### 5. Header-Only Files Omission
Header-only C++ classes (`.h` without a compiling `.cc` file) might be missing from SuperSize records, creating slight indexing gaps.
* **Developer Fix**: Manually check the directory for header-only files and grep for their imports.

---

## 2. Snapshot Diffing (`analyze_size_diff.py`)

Generates high-level binary footprint diffs between two compiled snapshots (e.g., across branches, pull requests, or release upgrades) to audit net growth. It runs inside the SuperSize console REPL.

### Usage Workflow
1. **Generate the Diff Archive**:
   ```bash
   ./tools/binary_size/supersize diff rdk_26.size rdk_27.size --output rdk_26_27.sizediff
   ```
2. **Run the Analysis**:
   ```bash
   ./tools/binary_size/supersize console rdk_26_27.sizediff --query='exec(open("cobalt/tools/binary_size/analyze_size_diff.py").read())'
   ```

### Output Capabilities & Feature Removal Guidance
* **Net PSS Overhead**: Measures absolute growth via `delta.raw_symbols.pss` to capture alignment padding and unmapped additions accurately.
* **Subsystem Slicing**: Aggregates net diffs by major upstream Chromium component buckets (e.g., `v8`, `Blink`, `net`) to trace high-level architectural inflation.
* **Newly Added Component Gating**: Isolates brand-new symbols added in the modified snapshot and maps them directly to official functional component tags. This clearly highlights entire new upstream feature additions driving binary bloat.
* **Third-Party Target Isolation**: Filters specifically for external libraries under `third_party/` growing by $\ge 5\text{ KB}$. Because external code is heavily decoupled, these libraries represent low-hanging fruit for surgical unbundling (via high-level GN feature gating) if their underlying functionalities are not needed.
