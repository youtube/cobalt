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

## Custom Evergreen Setup, Path Configurations & Debugging for SoC Partners

When deploying prebuilt or custom-built Evergreen packages (unzipped CRX packages) onto target Linux/ARM hardware, configure the following path parameters and environment variables.

### 1. Evergreen Path Parameter Mapping Reference

| Parameter / Variable | Category | Expected Target Path | Description |
| :--- | :--- | :--- | :--- |
| **`--evergreen_library`** | `elf_loader_sandbox` Switch | `<target_root>/app/cobalt/lib/libcobalt.so` (or `libcobalt.lz4`) | Points to the Cobalt Core shared library binary. |
| **`--evergreen_content`** | `elf_loader_sandbox` Switch | `<target_root>/app/cobalt/content` | Points to the web engine resource directory containing `cobalt_shell.pak`, `fonts/`, and `ssl/`. |
| **`--content`** | `loader_app` Switch | `<target_root>` | Overrides the base root directory for `kSbSystemPathContentDirectory`. `loader_app` will append `/app/cobalt/` to locate Slot 0. |
| **`kSbSystemPathContentDirectory`** | Starboard System Path API | `<target_root>` | Returned by `SbSystemGetPath()`. Used by `loader_app` to construct paths for `manifest.json`, `lib/`, and `content/`. |

#### Example Target Directory Structure (Slot 0 Layout):
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

### 2. Compiling Custom Debug Packages for SoC Partners (By Platform)

SoC Partners and OEMs can compile custom Cobalt debug builds with full unstripped symbols enabled to inspect C++ stack traces on partner hardware. Because of the architectural differences between Evergreen and Native APK implementations, select the steps appropriate for your target environment:

#### Option A: RDK & Linux Workstation (Evergreen Dynamic Replacement)
For Evergreen builds (`use_evergreen = true`), partners can build standalone debug core binaries to replace existing files in Slot 0:

1. **Initialize GN for Debug / Devel Build**:
   ```bash
   cobalt/build/gn.py -p <platform> -c devel --no-rbe
   ```
   *(e.g., `-p evergreen-arm-hardfp-rdk` for ARM set-top boxes, or `-p evergreen-x64` for desktop testing)*

2. **Compile Uncompressed Cobalt Core & Compression Tool**:
   ```bash
   autoninja -C out/<platform>_devel cobalt lz4_compress
   ```
   This generates the uncompressed debug shared library (`out/<platform>_devel/app/cobalt/lib/libcobalt.so`), resource files, and the host compression tool. Specifying `cobalt lz4_compress` directly targets the core library and compression binary without building unneeded loader wrapper targets.

3. **(Optional) Compress Library for Production Testing**:
   To verify LZ4 compressed library loading (`libcobalt.lz4`) on devices expecting compressed binaries, run Cobalt's host compression tool:
   ```bash
   out/<platform>_devel/clang_x64/lz4_compress \
     out/<platform>_devel/app/cobalt/lib/libcobalt.so \
     out/<platform>_devel/app/cobalt/lib/libcobalt.lz4
   ```
   *(To deploy to an RDK board, use `deploy_rdk.py --only-lib` or copy directly via SCP after stopping active loader sessions).*

#### Option B: Android TV / Chrobalt ATV (Native APK Deployment)
For Android TV, Evergreen dynamic updater mechanisms are disabled (`use_evergreen = false`). To debug core engine logic on ATV hardware, build and reinstall a complete debug APK package:

1. **Configure GN for Android TV**:
   ```bash
   gn args out/android-arm_devel
   ```
   Ensure your build arguments include:
   ```gn
   target_os = "android"
   target_cpu = "arm"       # Or "arm64" for AArch64 Android hardware
   is_cobalt = true
   is_androidtv = true
   use_starboard_media = true
   use_evergreen = false    # Native APK build
   build_type = "devel"     # Or "debug" for full unstripped debugging
   ```

2. **Compile Application APK**:
   ```bash
   autoninja -C out/android-arm_devel cobalt_apk
   ```
   This packages the unstripped native engine (`libchrobalt.so`) directly into `out/android-arm_devel/apks/Cobalt.apk` (`dev.cobalt.coat`). No `lz4_compress` step is required or supported.

3. **Deploy & Install via ADB**:
   Do not extract or copy individual `.so` files into the Android filesystem. Reinstall the updated APK directly over ADB:
   ```bash
   adb install -r -d out/android-arm_devel/apks/Cobalt.apk
   adb shell am start -n dev.cobalt.coat/dev.cobalt.app.MainActivity
   ```

### 3. Deploying Official Google Prebuilt CRX Packages for Vendors (Cobalt 27.lts)

In production integration and certification, SoC Partners and Vendors do not compile Cobalt Core (`libcobalt.so`) from source. Instead, vendors build `loader_app` and Starboard components, and deploy official Google-built prebuilt CRX packages (`.crx`) onto the device's Slot 0 directory structure.

Follow this step-by-step procedure to download, unpack, and deploy official CRX packages in Cobalt 27.lts:

#### Step 1: Download Official Prebuilt CRX Package
Select the official prebuilt CRX file matching your device architecture (e.g. `arm-hardfp`, `arm64`, `x64`), Starboard API version, and build configuration (`release`, `qa`, `devel`) from GitHub Releases (e.g. `https://github.com/youtube/cobalt/releases`) or Google release distribution channels:

```bash
export LOCAL_CRX_DIR=/tmp/cobalt_dl
rm -rf $LOCAL_CRX_DIR && mkdir -p $LOCAL_CRX_DIR

# Set download URL for the targeted release tag and architecture
COBALT_CRX_URL="https://github.com/youtube/cobalt/releases/download/<version>/cobalt_evergreen_<version>_<arch>_<config>.crx"
wget $COBALT_CRX_URL -O $LOCAL_CRX_DIR/cobalt_prebuilt.crx
```

#### Step 2: Unpack the CRX Package
A Cobalt `.crx` file is a ZIP package containing the Cobalt Core shared library, manifest, and web engine assets. Extract the package using `unzip`:

```bash
unzip $LOCAL_CRX_DIR/cobalt_prebuilt.crx -d $LOCAL_CRX_DIR/cobalt_prebuilt
```

This extracts the following Slot 0 components inside `$LOCAL_CRX_DIR/cobalt_prebuilt/`:
- `manifest.json` (Evergreen manifest file)
- `lib/libcobalt.so` (or `libcobalt.lz4` compressed binary)
- `content/` (`cobalt_shell.pak`, `fonts/`, `ssl/`)

#### Step 3: Copy Unpacked Files to Slot 0 Layout (`app/cobalt/`)

> [!IMPORTANT]
> **Cobalt 27.lts Slot 0 Directory Layout Change vs. 25.lts:**
> Unlike Cobalt 25.lts (which placed CRX contents directly into `$EVERGREEN_DIR/` or `$EVERGREEN_DIR/install/lib/`), Cobalt 27.lts requires all Slot 0 factory artifacts to be located strictly under `<target_root>/app/cobalt/`.

##### Option A: Staging in Local Build Output Directory (`$EVERGREEN_DIR`)
If bundling CRX contents into your host build directory before creating a device deployment archive:

```bash
export EVERGREEN_DIR=out/evergreen-arm-hardfp-rdk_qa

# Create Slot 0 target directory structure
mkdir -p $EVERGREEN_DIR/app/cobalt/lib
mkdir -p $EVERGREEN_DIR/app/cobalt/content

# Copy manifest, shared library, and content resources
cp -f $LOCAL_CRX_DIR/cobalt_prebuilt/manifest.json $EVERGREEN_DIR/app/cobalt/
cp -rf $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* $EVERGREEN_DIR/app/cobalt/lib/
cp -rf $LOCAL_CRX_DIR/cobalt_prebuilt/content/* $EVERGREEN_DIR/app/cobalt/content/
```

##### Option B: Direct Deployment to Target Device
If pushing unzipped CRX files directly to target device hardware (e.g. `/usr/share/content/data/` or `$COBALT_CONTENT_DIR`):

```bash
# Ensure target Slot 0 directories exist on device
ssh root@<device_ip> "mkdir -p /usr/share/content/data/app/cobalt/lib /usr/share/content/data/app/cobalt/content"

# Deploy manifest, shared library, and content assets
scp $LOCAL_CRX_DIR/cobalt_prebuilt/manifest.json root@<device_ip>:/usr/share/content/data/app/cobalt/
scp -r $LOCAL_CRX_DIR/cobalt_prebuilt/lib/* root@<device_ip>:/usr/share/content/data/app/cobalt/lib/
scp -r $LOCAL_CRX_DIR/cobalt_prebuilt/content/* root@<device_ip>:/usr/share/content/data/app/cobalt/content/
```

---

## Additional References

- [Supported Features](reference/supported-features.md)
- [Troubleshooting Guide](reference/troubleshooting.md)
