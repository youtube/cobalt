Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

{# ![Cobalt 25 LTS RC](../images/cobalt-release-logo.png "Cobalt 25 LTS RC") #}

# [Jul 1st 2024] Cobalt 25.LTS.1 Release Candidate

## [Cobalt 25.lts.1 Release](https://developers.google.com/youtube/devices/living-room/cobalt/cobalt-evergreen-version-table)

The Cobalt team is thrilled to announce the RC (Release Candidate) of [Cobalt 25 LTS 1 (build number 1034666)](https://github.com/youtube/cobalt/releases/tag/25.lts.1)
!

Evergreen binaries are available on GitHub ([5.1.2](https://github.com/youtube/cobalt/releases/tag/25.lts.1))

Cobalt 25 LTS includes Starboard API version 16 for porters and its Stable release version will support all features that are required for 2025 YouTube Certification. The code is available at Cobalt open source [25.lts.1+](https://github.com/youtube/cobalt/releases/tag/25.lts.1) branch. (Read branching/versioning documents for more information).


There are newly implemented features, improvements, and simplified code. We have outlined the changelist below with a brief description of Cobalt 25 LTS and Starboard 16 changes. More detailed information can be found in the respective <span style="color:green">*CHANGELOG.md*</span> files ([Cobalt](https://github.com/youtube/cobalt/blob/25.lts.1%2B/cobalt/CHANGELOG.md)), [Starboard](https://github.com/youtube/cobalt/blob/main/starboard/CHANGELOG.md#version-16).


 <span style="text-decoration:underline">IMPORTANT NOTE</span>: As this version is a RC (Release Candidate), the code is subject to change while we expect to have no more Starboard API changes. **This is ready for integration and we STRONGLY RECOMMEND you start integration of this Cobalt 25 LTS RC on your respective platforms early and report issues ASAP** so there is ample time to investigate and resolve any detected issues before Cobalt 25 LTS Stable Release later this year.


The Evergreen static version channels command (<span style="color:green">co 25lts1</span>) can be used to update builds to the "25.LTS.1" Cobalt version for testing following the steps provided [here](https://developers.google.com/youtube/devices/living-room/cobalt/cobalt-evergreen-faq)


## <span >Cobalt Changes</span> <br>([Full Cobalt/change log](https://github.com/youtube/cobalt/blob/25.lts.1/cobalt/CHANGELOG.md) for more information)

### New Features / Support

* **Chromium M114 Update:** Upgraded most Chromium components to align with the M114 milestone release - including syncing the build environment and compilers.

* **HTTP/3 with IETF QUIC:** Integrated HTTP/3 with IETF standard QUIC for enhance video playback performance in challenging network conditions.

* **Immersive Audio Model and Formats (IAMF):** Added support for IAMF audio.

* **Android S Support:** Extended compatibility to Android S.

### Updates / Improvements

* **Embedded ICU Data:** Linked ICU data directly into the Cobalt binary, reducing storage (via binary compression) and simplifying updates.

* **Logging and Telemetry Enhancements:** Improved logging and telemetry for update reliability and crash reporting.

### Evergreen

* **LZ4 Compressed Binaries:** Made LZ4 compressed binaries the default distribution format.

* **Evergreen-Full on AOSP:** Enabled Evergreen-Full support on AOSP (Android Open Source).



## <span >Starboard Changes</span> <br>([Full starboard/change log](https://github.com/youtube/cobalt/blob/25.lts.1/starboard/CHANGELOG.md))

### New Features / Support

* **POSIX API Adoption:** Adopted POSIX APIs (see migration guide for details), reducing the total number of Starboard interface functions to 191 (vs 274 in Starboard 15).

* **Partial Audio Frame Support:** Introduced a new configuration constant (<span style="color:green">kHasPartialAudioFramesSupport</span>) for partial audio frames.

### Updates / Improvements

* **pthread Migration:** Migrated various components (e g., <span style="color:green">SbConditionVariable</span>, <span style="color:green">SbOnce</span>, <span style="color:green">SbThreadLocalKey</span>, etc…) to pthread.

* **Memory and File System Migration:** Migrated memory allocation functions (<span style="color:green">SbMemoryAllocate</span>, <span style="color:green">SbMemoryMap</span> etc…) to <span style="color:green">malloc, mmap</span> and file system functions 
(<span style="color:green">SbDirectoryOpen</span>, <span style="color:green">SbDirectoryClose</span> etc) to equivalent opendir, closedir, etc.

* **Socket API Migration:** Migrated <span style="color:green">SbSocket</span> to socket APIs.

* **Starboard Extensions :** <span style="color:green">Accessibility</span> and <span style="color:green">OnScreenKeyboard</span> Starboard APIs are converted to Starboard Extensions (available in <span style="color:green">starboard extension/accessibility.h</span> and <span style="color:green">starboard/extension on_screen_keyboard.h</span> respectively).

* **Build Standard Upgrade:** Upgraded the C build standard to C11.

  * Removed <span style="color:green">SB_C_NOINLINE</span>.

  * Deprecated <span style="color:green">SB_C_INLINE</span> and removed config for <span style="color:green">SB_C_FORCE_INLINE</span>.

  * Removed configs for <span style="color:green">SB_EXPORT_PLATFORM</span> and <span style="color:green">SB_IMPORT_PLATFORM</span>.

* **Configuration Changes:**

  * Changed <span style="color:green">MAP_EXECUTABLE_MEMORY</span> from build-time to runtime config (<span style="color:green">kSbCanMapExecutableMemory</span>).

* **Additional Migrations:** Migrated <span style="color:green">SbDirectoryCanOpen, SbFileExists,</span> and <span style="color:green">SbFileGetPathInfo</span> to <span style="color:green">stat</span> , and <span style="color:green">SbFileOpen, SbFileClose </span>etc… to <span style="color:green">open, close</span>.

### Deprecations

* **Deprecated Evergreen-x86 ABI:** The x86 platform configurations, builds and ABI are no longer supported for Evergreen.

* **OnScreenKeyboard:** Deprecated <span style="color:green">OnScreenKeyboard</span>.

* **Unused Configurations:** Deprecated various unused configurations (e.g., <span style="color:green">SB_HAS_GLES2</span>, <span style="color:green">SB_HAS_NV12_TEXTURE_SUPPORT</span>, <span style="color:green">VIRTUAL_REALITY</span>, <span style="color:green">DeployPathPatterns</span>, <span style="color:green">SPEECH_SYNTHESIS</span>, <span style="color:green">FILESYSTEM_ZERO_FILEINFO_TIME</span>, <span style="color:green">SB_HAS_PIPE</span>).

* **Audio and Media:**

  * Deprecated <span style="color:green">SB_HAS_QU</span> (<span style="color:green">SUPPORT_INT16_AUDIO_SAMPLES</span>).

  * Deprecated <span style="color:green">SbMediaGetBufferAlignment</span>, <span style="color:green">SbMediaGetBufferPadding</span>, and <span style="color:green">SbMediaGetBufferStorageType</span>.

  * Deprecated <span style="color:green">SbMediaIsBufferUsingMemoryPool</span>, memory pool is always used (<span style="color:green">SbMediaIsBufferUsingMemoryPool</span> is required to be true).

* **Starboard Hash Configs:** Deprecated Starboard hash configs.


## <span style="color:red">*2025 YouTube Certification*</span>

Remember to use "25.lts.stable" for the 2025 YouTube certification and for your product release. Cobalt Known Issues with Status, Fixes, and Mitigations can be found in the YouTube Partner Portal.


## <span style="color:red">*Contact Points*</span>

Please contact our support channels if you have any problems, questions, or feedback.

Thank you,\
On behalf of the Cobalt team
