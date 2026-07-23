Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Development & Build Platforms Overview

Welcome to Cobalt Development! Cobalt supports multiple operating systems, hardware architectures, and build host environments.

This guide provides an overview of the supported platforms, their target hardware, intended use cases, and recommended development workflows.

---

## Supported Build Platforms & Setup Guides

| Platform / Tooling | Type | Target Architecture & Hardware | Primary Purpose & Intended Use | Setup Guide |
| :--- | :--- | :--- | :--- | :--- |
| **Linux** | Target OS | **x86_64 Desktop** (Ubuntu / X11 / Wayland) | **Developer Workstation Environment**. Provides the fastest compile-and-debug iteration cycle for core logic, Web APIs, and Starboard verification (`nplb`), without requiring embedded TV hardware. | [setup-linux.md](setup-linux.md) |
| **Android TV (ATV)** | Target OS | **ARM / ARM64 / x86** (Smart TVs, Streaming Sticks) | **Android TV Production Target**. Compiles native engine (`libchrobalt.so`) and packages into standard Android application package (`Cobalt.apk`, `dev.cobalt.coat`). | [setup-android.md](setup-android.md) |
| **RDK** | Target OS | **ARM / ARM64** (Pay-TV Set-Top Boxes, STB reference hardware) | **Pay-TV & STB Platform Target**. Integrates Starboard for RDK (`evergreen-arm-hardfp-rdk`) for operator-managed set-top box deployments. | [setup-rdk.md](setup-rdk.md) |
| **Docker** | Host Tooling | **Linux x86_64 Host** (Containerized Builder) | **Build Environment Consistency**. Containerized build host tool that standardizes dependencies and tools across developer workstations to avoid host OS version conflicts. | [setup-docker.md](setup-docker.md) |

---

## Recommended Development Workflow

To maximize development efficiency, we recommend following a 3-stage development workflow:

1. **Stage 1: Core Logic & Web Feature Development**
   - **Platform**: `Linux` (`linux-x64x11`)
   - **Why**: Fastest build times, direct gdb/lldb integration, and immediate execution on your workstation. Run `nplb` to verify Starboard API compliance.

2. **Stage 2: Standardized Host Build Environment**
   - **Tooling**: `Docker`
   - **Why**: Use containerized build definitions to build for cross-compiled targets (like RDK or Android) without polluting or conflicting with your local host environment.

3. **Stage 3: Target Device & Media Integration Validation**
   - **Platform**: `Android TV` or `RDK`
   - **Why**: Final testing of hardware video/audio decoding, platform lifecycle (suspend/resume), remote control input handling, and system-level performance.

---

## Core Compilation & Debug Verification: Evergreen vs. Native APK

When developing, debugging, or verifying custom Cobalt modifications on device hardware, developers and SoC partners must distinguish between Cobalt's two fundamentally different runtime architectures. Build targets, compression steps, and library replacement procedures differ substantially across platforms:

| Feature / Step | Evergreen Architecture (RDK / Linux) | Native APK Architecture (Android TV) |
| :--- | :--- | :--- |
| **Target Architecture** | **Evergreen Dynamic Slot** (`use_evergreen = true`) | **Standard Android Application** (`use_evergreen = false`) |
| **Primary Build Target** | **`cobalt`** and **`lz4_compress`** | **`cobalt_apk`** |
| **Compiled Artifacts** | `app/cobalt/lib/libcobalt.so` and `libcobalt.lz4` | `apks/Cobalt.apk` (`dev.cobalt.coat`, bundling `libchrobalt.so`) |
| **Debug & Verification** | **Direct Slot Replacement**. Unpack the CRX or access Slot 0 on device, and overwrite `libcobalt.so` (or `.lz4`) via SSH/SCP or `deploy_rdk.py --only-lib`. | **Full APK Reinstallation**. Do not modify filesystem shared libraries directly; reinstall the updated package via `adb install -r -d Cobalt.apk`. |
| **Host Compression** | Required when testing production compressed library loading (`lz4_compress`). | **Not used / Inapplicable**. Native packaging directly handles shared library compression in APK. |

> [!IMPORTANT]
> **Live Replacement Safety Rules for Evergreen Platforms:**
> When replacing shared libraries (`libcobalt.so` or `libcobalt.lz4`) directly on live RDK or Linux target hardware:
> 1. **Stop Active Processes First**: Kill running `loader_app` or WPEFramework `YouTube` sessions before copying new files. Overwriting active mapped libraries without stopping the process causes immediate `Text file busy` failures or fatal kernel segmentation faults.
> 2. **Deploy Both Formats**: When testing unstripped builds, deploy both uncompressed (`.so`) and compressed (`.lz4`) library files simultaneously to avoid loader magic signature parsing exceptions.
> 3. **Keep Resources Synchronized**: If your modifications change interface definitions or resources, always push updated `content/` resources (`cobalt_shell.pak`) alongside the library binary.

---

## Evergreen Path Configuration & Vendor Deployment Setup

When deploying prebuilt or custom-built Evergreen packages (unzipped CRX packages) onto target Linux/ARM hardware, configure the following path parameters and environment variables.

### 1. Evergreen Path Parameter Mapping Reference

| Parameter / Variable | Category | Expected Target Path | Description |
| :--- | :--- | :--- | :--- |
| **`--evergreen_library`** | `elf_loader_sandbox` Switch | `<target_root>/app/cobalt/lib/libcobalt.so` (or `libcobalt.lz4`) | Points to the Cobalt Core shared library binary. |
| **`--evergreen_content`** | `elf_loader_sandbox` Switch | `<target_root>/app/cobalt/content` | Points to the web engine resource directory containing `cobalt_shell.pak`, `fonts/`, and `ssl/`. |
| **`--content`** | `loader_app` Switch | `<target_root>` | Overrides the base root directory for `kSbSystemPathContentDirectory`. `loader_app` will append `/app/cobalt/` to locate Slot 0. |
| **`kSbSystemPathContentDirectory`** | Starboard System Path API | `<target_root>` | Returned by `SbSystemGetPath()`. Used by `loader_app` to construct paths for `manifest.json`, `lib/`, and `content/`. |

#### Target Directory Structure (Cobalt 27.lts Slot 0 Layout):
```text
/usr/share/content/data/                   <-- Base Target Root (<target_root>)
├── loader_app                             <-- Partner-built Loader executable
├── elf_loader_sandbox                     <-- Sandbox test executable
└── app/
    └── cobalt/                            <-- Factory Slot 0 Directory
        ├── manifest.json                  <-- Evergreen manifest file
        ├── lib/
        │   └── libcobalt.so               <-- Core shared library
        └── content/
            ├── cobalt_shell.pak           <-- UI and web resources
            ├── fonts/                     <-- System fonts and fonts.xml
            └── ssl/                       <-- Root CA certificates
```

### 2. Platform Setup Guides for Custom Core Compilation & Prebuilt CRX Unpacking

Detailed step-by-step instructions for compiling custom Cobalt Core binaries, unzipping prebuilt `.crx` packages, and installing packages on target hardware are maintained in each platform's dedicated setup guide:

* **Linux Workstation & Desktop**:
  Refer to **[setup-linux.md](setup-linux.md#running-in-evergreen-mode)** for compiling custom `cobalt` binaries, compressing libraries with `lz4_compress`, and unpacking prebuilt `.crx` packages into Slot 0 (`app/cobalt/`).
* **RDK / Pay-TV Set-Top Boxes**:
  Refer to **[setup-rdk.md](setup-rdk.md#build-cobalt-binary-for-the-rdk-platform)** for compiling `loader_app_rdk_plugin`, deploying prebuilt `.crx` packages to RDK hardware, and staging Slot 0 (`app/cobalt/`) directories.
* **Android TV (Chrobalt ATV)**:
  Refer to **[setup-android.md](setup-android.md#5-compiling--packaging)** for building native `Cobalt.apk` (`use_evergreen = false`) and deploying over ADB. *(Note: Prebuilt `.crx` unpacking does not apply to Android TV).*

---

## Additional References

- [Supported Features](reference/supported-features.md)
- [Troubleshooting Guide](reference/troubleshooting.md)
