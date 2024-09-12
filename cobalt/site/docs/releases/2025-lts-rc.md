Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# [Jul 1st 2024] Cobalt 25.LTS.1 Release Candidate

## [Cobalt 25.lts.1 Release](https://developers.google.com/youtube/devices/living-room/cobalt/cobalt-evergreen-version-table)

The Cobalt team is thrilled to announce the RC (Release Candidate) of [Cobalt 25
LTS 1](https://github.com/youtube/cobalt/releases/tag/25.lts.1) !

Evergreen binaries are available on GitHub
([5.1.2](https://github.com/youtube/cobalt/releases/tag/25.lts.1))

Cobalt 25 LTS includes Starboard API version 16 for porters and its Stable
release version will support all features that are required for 2025 YouTube
Certification. The code is available at Cobalt open source
[25.lts.1+](https://github.com/youtube/cobalt/releases/tag/25.lts.1) branch.
Read
[branching](https://github.com/youtube/cobalt/blob/25.lts.1/cobalt/doc/branching.md)
/
[versioning](https://github.com/youtube/cobalt/blob/25.lts.1/cobalt/doc/versioning.md)
documents for more information.

There are newly implemented features, improvements, and simplified code. We have
outlined the changelist below with a brief description of Cobalt 25 LTS and
Starboard 16 changes.

IMPORTANT NOTE: As this version is a RC (Release Candidate), the code is
subject to change while we expect to have no more Starboard API changes.
**This is ready for integration and we STRONGLY RECOMMEND you start integration
of this Cobalt 25 LTS RC on your respective platforms early and report issues
ASAP** so there is ample time to investigate and resolve any detected issues
before Cobalt 25 LTS Stable Release later this year.

The Evergreen static version channels command (`co 25lts1`) can be used to
update builds to the `25.LTS.1` Cobalt version for testing by following the
steps provided
[here](https://developers.google.com/youtube/devices/living-room/cobalt/cobalt-evergreen-faq).

## Cobalt Changes

[Full change log](https://github.com/youtube/cobalt/blob/25.lts.1/cobalt/CHANGELOG.md)
for more information.

### New Features / Support

*   **Chromium M114 Update:** Upgraded most Chromium components to align with
    the M114 milestone release - including syncing the build environment and
    compilers.

*   **HTTP/3 with IETF QUIC:** Integrated HTTP/3 with IETF standard QUIC for
    enhance video playback performance in challenging network conditions.

*   **Immersive Audio Model and Formats (IAMF):** Added support for IAMF audio.

*   **Android S Support:** Extended compatibility to Android S.

### Updates / Improvements

*   **Embedded ICU Data:** Linked ICU data directly into the Cobalt binary,
    reducing storage (via binary compression) and simplifying updates.

*   **Logging and Telemetry Enhancements:** Improved logging and telemetry for
    update reliability and crash reporting.

### Evergreen

*   **LZ4 Compressed Binaries:** Made LZ4 compressed binaries the default
    distribution format.

*   **Evergreen-Full on AOSP:** Enabled Evergreen-Full support on AOSP (Android
    Open Source).

## Starboard changes

[Full change log](https://github.com/youtube/cobalt/blob/25.lts.1/starboard/CHANGELOG.md)
for more information.

### New Features / Support

*   **POSIX API Adoption:** Adopted POSIX APIs (see migration guide for
    details), reducing the total number of Starboard interface functions to 191
    (vs 274 in Starboard 15).

*   **Partial Audio Frame Support:** Introduced a new configuration constant
    (`kHasPartialAudioFramesSupport`) for partial audio frames.

### Updates / Improvements

*   **pthread Migration:** Migrated various components (e g.,
    `SbConditionVariable`, `SbOnce`, `SbThreadLocalKey`, etc…) to pthread.

*   **Memory and File System Migration:** Migrated memory allocation functions
    (`SbMemoryAllocate`, `SbMemoryMap` etc…) to `malloc`, `mmap` and file system
    functions (`SbDirectoryOpen` or `SbDirectoryClose` for example) to
    equivalent `opendir`, `closedir`, etc..

*   **Socket API Migration:** Migrated `SbSocket` to socket APIs.

*   **Starboard Extensions :** `Accessibility` and `OnScreenKeyboard` Starboard
    APIs are converted to Starboard Extensions (available in
    `starboard/extension/accessibility.h` and
    `starboard/extension on_screen_keyboard.h` respectively).

*   **Build Standard Upgrade:** Upgraded the C build standard to C11.

    *   Removed `SB_C_NOINLINE`.

    *   Deprecated `SB_C_INLINE` and removed config for `SB_C_FORCE_INLINE`.

    *   Removed configs for `SB_EXPORT_PLATFORM` and `SB_IMPORT_PLATFORM`.

*   **Configuration Changes:**

    *   Changed `MAP_EXECUTABLE_MEMORY` from build-time to runtime config
        (`kSbCanMapExecutableMemory`).

*   **Additional Migrations:** Migrated `SbDirectoryCanOpen`, `SbFileExists`,
    and `SbFileGetPathInfo` to `stat` , and `SbFileOpen`, `SbFileClose` to
    `open`, `close`.

### Deprecations

*   **Deprecated Evergreen-x86 ABI:** The x86 platform configurations, builds
    and ABI are no longer supported for Evergreen.

*   **OnScreenKeyboard:** Deprecated `OnScreenKeyboard`.

*   **Unused Configurations:** Deprecated various unused configurations (e.g.,
    `SB_HAS_GLES2`, `SB_HAS_NV12_TEXTURE_SUPPORT`, `VIRTUAL_REALITY`,
    `DeployPathPatterns`, `SPEECH_SYNTHESIS`, `FILESYSTEM_ZERO_FILEINFO_TIME`,
    `SB_HAS_PIPE`).

*   **Audio and Media:**

    *   Deprecated `SB_HAS_QU` (`SUPPORT_INT16_AUDIO_SAMPLES`).

    *   Deprecated `SbMediaGetBufferAlignment`, `SbMediaGetBufferPadding`, and
        `SbMediaGetBufferStorageType`.

    *   Deprecated `SbMediaIsBufferUsingMemoryPool`, memory pool is always used
    (`SbMediaIsBufferUsingMemoryPool` is required to be true).

*   **Starboard Hash Configs:** Deprecated Starboard hash configs.

## *2025 YouTube Certification*

Remember to use `25.lts.stable` for the 2025 YouTube certification and for your
product release. Cobalt Known Issues with Status, Fixes, and Mitigations can be
found in the YouTube Partner Portal.

## *Contact Points*

Please contact our support channels if you have any problems, questions, or
feedback.

Thank you,\
On behalf of the Cobalt team
