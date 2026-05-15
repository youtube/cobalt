Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Chrobalt Linux: Engineering Setup & Development Guide

Welcome to **Chrobalt Linux**! This guide is designed to onboard software engineers to development on Cobalt for Linux running on the Chromium browser engine.

---

## 1. Welcome & Architectural Overview

### What is Chrobalt?
**Chrobalt** is the project codename for Cobalt versions 26 and later. Starting with v26, Cobalt transitioned from a standalone repository structure to running directly inside the upstream Chromium repository architecture.

* **Chrobalt Linux** is built natively using Chromium’s `GN` and `Ninja` build systems.
* **Shared Engine Implementation**: The core browser engine and Starboard media adaptations are compiled into a modular executable named `cobalt`.

---

## 2. System Requirements & Prerequisites

Before setting up your workspace, verify that your development machine meets the following minimum hardware and software specifications:

* **Operating System**: Linux x86-64 (Ubuntu 22.04 LTS or newer recommended). Note that building Chromium/Cobalt on Windows or macOS is not supported.
* **Memory**: At least 16 GB RAM (32+ GB highly recommended for optimal C++ linking performance).
* **Disk Space**: Minimum 100 GB of available SSD storage.
* **System Tools**: `git`, `python3`, and `curl`.

### Installing Depot Tools
Chromium projects require Google's `depot_tools` suite for source code checkout and dependency management.

```bash
# Clone Google's depot_tools repository to your home directory
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools

# Temporarily add depot_tools to your PATH for the current terminal session.
# To make this persistent across sessions, add this line to your ~/.bashrc or ~/.zshrc file.
export PATH="$PATH:$HOME/depot_tools"
```

---

## 3. Repository & Workspace Setup

To configure your local Chrobalt Linux checkout, you will clone the Cobalt repository and use `gclient` to synchronize Chromium dependencies.

1. **Clone the Cobalt Repository**:
   ```bash
   # Create a working directory for the Chromium source tree
   mkdir ~/chromium && cd ~/chromium

   # Clone the Cobalt repository into the 'src' directory expected by gclient
   git clone --single-branch https://github.com/youtube/cobalt.git src
   ```

2. **Configure gclient and Synchronize Dependencies**:
   ```bash
   # Configure gclient to track the Cobalt repository as the primary source
   gclient config --name=src https://github.com/youtube/cobalt.git

   # Synchronize all Chromium dependencies, sysroots, and toolchains.
   # Note: Using --no-history performs a shallow clone for a significantly faster checkout.
   cd src
   gclient sync --no-history -r $(git rev-parse @)
   ```

3. **Install System Dependencies**:
   ```bash
   # Execute Chromium's automated dependency script to install required Linux system packages and fonts
   build/install-build-deps.sh

   # Run gclient hooks to ensure all GN toolchains and sysroots are fully configured
   gclient runhooks
   ```

---

## 4. Build Configuration (GN)

Chrobalt Linux uses `GN` to generate build files. You can maintain multiple build directories (e.g., `out/linux-x64x11_qa/`).

### Core Build Variables
When configuring your build environment, Chrobalt Linux relies on several crucial GN variables defined in `//cobalt/build/configs/`:

| GN Variable | Chrobalt Linux Value | Purpose |
| :--- | :--- | :--- |
| `target_os` | `"linux"` | Sets the target operating system to Linux. |
| `is_cobalt` | `true` | Enables Cobalt-specific browser and runtime overrides. |
| `use_starboard_media`| `true` | Integrates Starboard hardware decoding & DRM pipelines. |
| `use_evergreen` | `false` | Disables Cobalt Evergreen binary updaters (native modular build). |

### Build Flavors
Choose your target optimization level via `build_type`:

* **`debug`** (`is_debug=true`, `is_official_build=false`): Full unstripped debug symbols, zero optimization. Best for step-by-step C++ debugging.
* **`devel`** (`is_debug=false`, `is_official_build=false`): Symbols retained, partial optimization. Standard for daily development and local testing.
* **`qa`** (`is_debug=false`, `is_official_build=true`): Full optimization, symbols stripped. Matches staging and certification environments.
* **`gold`** (`is_debug=false`, `is_official_build=true`): Production release build.

### Initializing a Build Directory
To generate your Ninja configuration, execute `gn args`:

```bash
# Open an interactive text editor to configure your GN build directory
gn args out/linux-x64x11_qa
```

In the text editor that opens, enter your desired configuration:

```gn
# Set target operating system and architecture to Linux x64
target_os = "linux"
target_cpu = "x64"

# Enable Cobalt browser engine and Starboard media adaptations
is_cobalt = true
use_starboard_media = true

# Disable Evergreen updaters for a direct modular build
use_evergreen = false

# Set build optimization level (Options: "debug", "devel", "qa", "gold")
build_type = "qa"
```

---

## 5. Compiling & Execution

Chrobalt Linux packages its implementation into the `cobalt` modular executable defined in `//cobalt/BUILD.gn`.

### Building the Application
```bash
# Compile the cobalt target using autoninja, which automatically optimizes CPU core utilization
autoninja -C out/linux-x64x11_qa cobalt
```

Upon successful compilation, the output binary will be available at:
```bash
out/linux-x64x11_qa/cobalt
```

### Launching Cobalt
You can launch the compiled binary directly from the command line:

```bash
# Navigate to the build directory and execute the cobalt binary
cd out/linux-x64x11_qa
./cobalt [--url=<url>]
```

Chrobalt Linux supports standard command-line switches passed directly to the runtime:

```bash
# Launch with a specific startup URL
./cobalt --url="https://www.youtube.com/tv"

# Allow HTTP and ignore certificate errors for local testing
./cobalt --allow-http --ignore-certificate-errors
```

---

## 6. Running Cobalt in Evergreen Mode

As Evergreen support is mandatory for certification, Cobalt can also be executed in Evergreen mode on Linux.

### Initializing an Evergreen Build Directory
```bash
# Open an interactive text editor to configure your Evergreen GN build directory
gn args out/evergreen-x64_qa
```

In the text editor that opens, configure the build for Evergreen:

```gn
# Set target operating system and architecture to Linux x64
target_os = "linux"
target_cpu = "x64"

# Enable Cobalt browser engine and Evergreen binary updaters
is_cobalt = true
use_evergreen = true

# Set build optimization level to QA
build_type = "qa"
```

### Building and Running Evergreen Cobalt
```bash
# Compile the cobalt_loader target, which packages the compressed Evergreen library
autoninja -C out/evergreen-x64_qa cobalt_loader
```

To launch Cobalt in Evergreen mode, execute the generated helper script:

```bash
# Navigate to the Evergreen build directory and launch the Python loader script
cd out/evergreen-x64_qa
./cobalt_loader.py [--url=<url>]
```

---

## 7. Verification & Testing (NPLB)

The No Platform Left Behind (NPLB) test suite verifies Starboard implementation and is mandatory for certification.

### Building NPLB
```bash
# Compile the NPLB test suite in Evergreen mode
autoninja -C out/evergreen-x64_qa nplb_loader
```

### Running NPLB
You can execute NPLB using the generated Python loader script, which invokes `elf_loader_sandbox` under the hood:

```bash
# Navigate to the build directory and execute the NPLB test suite
cd out/evergreen-x64_qa
./nplb_loader.py
```

To pass standard Google Test filtering or output arguments:

```bash
# Run NPLB with a specific test filter and output results to an XML file
./nplb_loader.py -- --gtest_filter=*Posix* --gtest_output=xml:/tmp/nplb_results.xml
```

---

## 8. Debugging, Logging & Cleanup

### Viewing Logs
Cobalt logs output directly to standard stdout and stderr in the terminal.

```bash
# Run Cobalt and filter output specifically for Cobalt and Starboard events
./cobalt 2>&1 | grep -E "(starboard|Cobalt)"
```

### Debugging
Use `gdb` or `lldb` to debug the `cobalt` binary. `debug` and `devel` builds provide full symbols for tracing execution.

### Environment Cleanup
To clean build artifacts and free up disk space without deleting your source repository:

```bash
# Purge all compiled object files and artifacts in the build directory
gn clean out/linux-x64x11_qa
```

> [!CAUTION]
> If you truly want to delete your entire development environment, including all uncommitted local changes, branches, and the 100GB+ source repository:
> ```bash
> # WARNING: This permanently deletes the entire Chromium and Cobalt source tree
> rm -rf ~/chromium
> ```
