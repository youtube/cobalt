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

### 2. Compiling Custom Debug CRX / Core Packages for SoC Partners

SoC Partners and OEMs can compile custom Cobalt Core packages with full debugging symbols enabled to inspect C++ stack traces on partner hardware:

1. **Initialize GN for Debug / Devel Build**:
   ```bash
   cobalt/build/gn.py -p <platform> -c devel --no-rbe
   ```
   *(e.g., `-p evergreen-arm-hardfp` for 32-bit ARM, or `-p evergreen-arm64` for 64-bit ARM)*

2. **Compile Uncompressed Cobalt Core & Resources**:
   ```bash
   autoninja -C out/<platform>_devel cobalt_loader
   ```
   This generates the uncompressed debug shared library (`out/<platform>_devel/app/cobalt/lib/libcobalt.so`) and resource files.

3. **(Optional) Compress Library for Production Testing**:
   To test LZ4 compressed library loading (`libcobalt.lz4`), run Cobalt's host compression tool:
   ```bash
   out/<platform>_devel/clang_x64/lz4_compress \
     out/<platform>_devel/app/cobalt/lib/libcobalt.so \
     out/<platform>_devel/app/cobalt/lib/libcobalt.lz4
   ```

---

## Additional References

- [Supported Features](reference/supported-features.md)
- [Troubleshooting Guide](reference/troubleshooting.md)
