Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Chrobalt ATV: Engineering Setup & Development Guide

Welcome to **Chrobalt ATV**! This guide is designed to onboard software engineers to Cobalt development for Android TV (ATV) on the Chromium browser engine.

---

## 1. Welcome & Architectural Overview

### What is Chrobalt?

**Chrobalt** is the project codename for Cobalt versions 26 and later. Starting with v26, Cobalt transitioned from a standalone repository to running inside the upstream Chromium repository.

* **Chrobalt ATV** is built natively using Chromium’s `GN` and `Ninja` build systems.
* **Shared Engine**: The core browser engine and Starboard media adaptations compile into a single shared library named `libchrobalt.so`.
* **APK Packaging**: The native library and its resources package into a standard Android APK (`Cobalt.apk`) under the `dev.cobalt.coat` package.

---

## 2. System Requirements & Prerequisites

Before setting up your workspace, verify that your development machine meets the following minimum hardware and software specifications:

* **Operating System**: Linux x86-64 (Ubuntu 22.04 LTS or newer recommended). Note that building Chromium/Cobalt for Android on Windows or macOS is not supported.
* **Memory**: At least 16 GB RAM (32+ GB highly recommended for optimal C++ linking performance).
* **Disk Space**: Minimum 100 GB of available SSD storage.
* **System Tools**: `git`, `python3`, and `curl`.

### Installing Depot Tools
Chromium projects require Google's `depot_tools` suite for source code checkout and dependency management.

```bash
# Clone depot_tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools

# Add depot_tools to your PATH in ~/.bashrc or ~/.zshrc
export PATH="$PATH:$HOME/depot_tools"
```

---

## 3. Repository & Workspace Setup

There are two supported approaches to configuring your local Chrobalt ATV checkout.

### Option A: Automated Checkout Setup (Recommended)

If you are initializing a fresh workspace, use the automated Cobalt checkout script:

```bash
python3 .agent/skills/cobalt-new-checkout/scripts/cobalt_new_checkout.py --non-interactive --github-user "<your_github_username>"
```

### Option B: Standard Chromium Checkout (Manual)

If you prefer to manage the checkout manually or are converting an existing Linux Chromium checkout:

1. **Fetch the Android Source Tree**

   ```bash
   mkdir ~/chromium && cd ~/chromium
   fetch --nohooks android
   # Note: Use --no-history for a faster checkout without full commit history.
   ```

2. **Converting an Existing Checkout**
   If you already have a Linux checkout, append `android` to `target_os` in your root `.gclient` configuration:

   ```python
   target_os = [ 'linux', 'android' ]
   ```
   Then run `gclient sync`.

3. **Install System and Android Dependencies**
   Execute Chromium's automated dependency script to install required Linux packages, the Android SDK, and NDK toolchains:

   ```bash
   cd src
   build/install-build-deps.sh
   gclient runhooks
   ```

4. **Set Up Android Debug Keystore**
   Generate a local debug keystore required for signing development APKs:

   ```bash
   keytool -genkey -v -keystore ~/.android/debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000
   ```

---

## 4. Build Configuration (GN)

Chrobalt ATV uses `GN` to generate build files. You can maintain multiple build directories (e.g., `out/android-arm_qa/`).

### Core Build Variables

When configuring your build environment, Chrobalt ATV relies on several crucial GN variables defined in `//cobalt/build/configs/`:

| GN Variable | Chrobalt ATV Value | Purpose |
| :--- | :--- | :--- |
| `target_os` | `"android"` | Sets the target operating system to Android. |
| `is_cobalt` | `true` | Enables Cobalt-specific browser and runtime overrides. |
| `is_androidtv` | `true` | Optimizes UI, input, and media pipelines for Android TV. |
| `use_starboard_media` | `true` | Integrates Starboard hardware decoding & DRM pipelines. |
| `is_starboard` | `false` | Disables legacy standalone Starboard OS wrappers. |
| `use_evergreen` | `false` | Disables Cobalt Evergreen binary updaters (native APK build). |

### Build Flavors

Choose your target optimization level via `build_type`:

* **`debug`** (`is_debug=true`, `is_official_build=false`): Full unstripped debug symbols, zero optimization. Best for step-by-step C++ debugging.
* **`devel`** (`is_debug=false`, `is_official_build=false`): Symbols retained, partial optimization. Standard for daily development and local testing.
* **`qa`** (`is_debug=false`, `is_official_build=true`): Full optimization, symbols stripped. Matches staging and certification environments.
* **`gold`** (`is_debug=false`, `is_official_build=true`): Production release build.

### Initializing a Build Directory

Generate the Ninja configuration using `gn args`:

```bash
gn args out/android-arm_qa
```

In the text editor that opens, enter your desired configuration:

```gn
target_os = "android"
target_cpu = "arm"         # Use "arm64" for 64-bit ATV hardware (verify via adb shell getprop ro.product.cpu.abi)

is_cobalt = true
is_androidtv = true
use_starboard_media = true

build_type = "qa"       # Options: "debug", "devel", "qa", "gold"
```

---

## 5. Compiling & Packaging

Chrobalt ATV packages its implementation into `Cobalt.apk` defined in `//cobalt/android/BUILD.gn`.

### Building the Application APK

Build the target using `autoninja` (which automatically optimizes core utilization):

```bash
autoninja -C out/android-arm_qa cobalt_apk
```

Upon successful compilation, the output APK will be available at:

```bash
out/android-arm_qa/apks/Cobalt.apk
```

---

## 6. Device Deployment & Execution

### Device Setup

1. On your Android TV or Google TV device, navigate to **Settings > System > About**.
2. Highlight **Android TV OS build** and press the select button seven times until the "You are now a developer" toast appears.
3. Return to **Settings > System > Developer options** and enable **USB debugging**.
4. Connect your workstation to the device via USB or Wi-Fi debugging and authorize the connection. Verify connectivity:

   ```bash
   adb devices
   ```

### Installing the APK

You can install the compiled APK directly via `adb`:

```bash
adb install -r out/android-arm_qa/apks/Cobalt.apk
```

### Launching with Command-Line Arguments

Chrobalt ATV uses the `--esa commandLineArgs` parameter to pass configuration switches and feature flags to the runtime.

**Basic Launch**:

```bash
adb shell am start dev.cobalt.coat/dev.cobalt.app.MainActivity
```

**Launching with Feature Flags**:
Separate individual flags with commas (`,`) and multiple values within a flag with semicolons (`;`):

```bash
# Enable a specific feature
adb shell am start --esa commandLineArgs 'enable-features=MyCoolFeature' dev.cobalt.coat/dev.cobalt.app.MainActivity

# Combine feature toggles and JS engine switches
adb shell am start --esa commandLineArgs 'enable-features=FeatureA;FeatureB,disable-features=LegacyFeature,js-flags=--no-opt' dev.cobalt.coat/dev.cobalt.app.MainActivity
```

To force-stop the running instance before relaunching:

```bash
adb shell am force-stop dev.cobalt.coat
```

---

## 7. Debugging, Logging & Testing

### Viewing Logs (Logcat)

Cobalt logs to the standard Android logcat. To monitor execution and filter for Cobalt, Starboard, and Chromium events:

```bash
adb logcat -s "starboard:*" "Cobalt:*" "chromium:*"
```

## 8. Environment Cleanup

Run the following commands to reset your Android build environment, remove signing keys, or purge cached SDK/NDK toolchains:

```bash
# Uninstall APK from target device
adb uninstall dev.cobalt.coat

# Delete cached Android signing keys and emulator configs
rm -rf ~/.android

# Remove downloaded SDK/NDK toolchains (if installed outside depot_tools)
rm -rf ~/starboard-toolchains
```
