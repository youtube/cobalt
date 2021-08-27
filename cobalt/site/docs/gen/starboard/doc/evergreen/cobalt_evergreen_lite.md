---
layout: doc
title: "Evergreen Lite Partner Doc"
---
Evergreen Lite Partner Doc


## What is Cobalt Evergreen Lite?

Evergreen Lite is a Cobalt configuration similar to Evergreen Full. Evergreen
Lite takes advantage of the same Evergreen software architecture, but removes
the need for additional storage and is missing the defining cloud-based Cobalt
Updater feature used in Evergreen Full.

Evergreen Lite relies on separating the Starboard (platform) and Cobalt (core)
components of a Cobalt implementation into the following discrete components:

![Evergreen Lite Overvew](resources/evergreen_lite_overview.png)

## Components

Google-built (on Google toolchain)


*   Cobalt Core
    *   Pre-built shared library available for all supported architectures
*   Cobalt Updater - disabled

Partner-built (on Partner toolchain)



*   Starboard
    *   Platform-specific implementation
    *   Contains system dependencies (e.g. libpthread.so, libEGL.so)
*   Cobalt Loader (Loader App)
    *   Loads the Cobalt core shared library
    *   An ELF loader is used to load the Cobalt core and resolves symbols with
        Starboard APIs when Cobalt starts up in Evergreen mode
*   Crash handler
    *   Uploads crash reports to Google server when crash happens

With this new Cobalt Evergreen platform architecture, less engineering effort is
 necessary for a full Cobalt integration/deployment.

**The idea here is you should only need to implement Starboard one time (as
long as the Starboard API version is supported by Cobalt), and any Cobalt
Core-level binary updates are provided by Google with pre-built
configurations/symbols via our open-source releases
([GitHub](https://github.com/youtube/cobalt/releases))**. These pre-built
Cobalt Core Evergreen binaries should be a direct replacement to update Cobalt
without any engineering work required. As Cobalt Core binaries are pre-built,
they should only require platform testing. NOTE that certain new Cobalt
features may require Starboard changes, so if you want to take advantage of
some of these new features, Starboard changes may be necessary.

### Benefits compared to non-Evergreen

*   Less engineering work/accelerated timeline for Cobalt
integration/deployment as Google builds Cobalt core code and partners only need
to build and maintain the Starboard layer
*   Performance enhancements as the Cobalt core is built with modern toolchain
*   Crash reports are uploaded to Google backend and monitored by Google, so
they can be acted on and addressed more quickly

### New in Evergreen Lite compared to non-Evergreen

*   New `loader_app` and `crashpad_handler` components required to be built
on platform toolchains
*   No Cobalt Core customization is allowed because the vanilla Cobalt Core
binary is provided by Google.

### Differences compared to Evergreen Full

*   The Google-control cloud-based automatic updates are disabled. Instead,
Cobalt provides the update binary to partners, and partners control the release
process
*   No extra storage required comparing to the existing software requirements

## How is Evergreen different from porting Cobalt previously?

Same as the [Evergreen full doc](cobalt_evergreen_overview.md).

## Building Cobalt Evergreen Components

`kSbSystemPathStorageDirectory` is not required to implement. Set both
`sb_evergreen_compatible` and `sb_evergreen_compatible_lite` to `1`s in the `gyp`
platform config. The remaining is the same as the Evergreen full doc.

## How does the update work with Evergreen Lite?

Cobalt will release the Cobalt Core binary to partners for each Cobalt LTS
major and minor release, and partners decide whether to update their devices
with the latest Cobalt Core code via firmware OTA update. To update, partners
only need to put the new Cobalt Core binary at the system image location under
`<kSbSystemPathContentDirectory>/app/cobalt`. More about the system image slot
is explained below.

## Platform Requirements

Cobalt Evergreen currently supports the following

Target Architectures:

*   `x86_32`
*   `x86_64`
*   `armv7 32`
*   `armv8 64`

Supported Javascript Engines

*   V8

## Building and Running Tests

Same as the Evergreen Full doc -
[cobalt_evergreen_overview.md](cobalt_evergreen_overview.md).

## System Design

### Cobalt Evergreen Components

Cobalt updater is disabled. The binary will not check for updates by sending
requests to the Google update server, nor download updates from the Google
Download server.

### Cobalt Evergreen Interfaces

Same as the Evergreen Full doc -
[cobalt_evergreen_overview.md](cobalt_evergreen_overview.md).

### System Image Slot

Evergreen Lite will have only one system image slot.  This is stored in the
directory specified by `kSbSystemPathContentDirectory` under the
`app/cobalt` subdirectory.

```
.
├── content <--(kSbSystemPathContentDirectory)
│   └── fonts <--(kSbSystemPathFontDirectory, `standard` or `limit` configuration)
│   └── app
│       └── cobalt <--(System image, provided by Google)
│           ├── content <--(relative path defined in kSystemImageContentPath)
│           │   ├── fonts <--(`empty` configuration)
│           │   ├── icu
│           │   ├── licenses
│           │   ├── ssl
│           ├── lib
│           │   └── libcobalt.so
│           └── manifest.json
└── loader_app <--(Cobalt loader binary)
└── crashpad_handler <--(Cobalt crash handler binary)
```

### Fonts

Same as the Evergreen Full doc -
[cobalt_evergreen_overview.md](cobalt_evergreen_overview.md).

## How to run Evergreen Lite

Launch Cobalt with the loader app binary with the `evergreen_lite` flag

```
$ ./loader_app --evergreen_lite
```
## FAQ

### What’s the path from Evergreen Lite to Evergreen Full?

*   Provision storage for the installation slots to contain downloaded update
binaries - `kSbSystemPathStorageDirectory `and configure the slots as instructed
in the Evergreen full doc
*   Configure icu table under `kSbSystemPathStorageDirectory` to be shared
    among slots
*   Set `sb_evergreen_compatible_lite` to 0
*   Implement the handling of pending updates
*   Rebuild and rerun `nplb_evergreen_compat_tests`
*   Launch Cobalt with loader app without the `evergreen_lite` flag

More details can be found in the Evergreen Full doc.
