# Starboard Version Changelog

This document records all changes made to the Starboard interface.
This file will be updated each time a new Starboard version is released.
Each section in this file describes the changes made to the Starboard interface
since the version previous to it.

**NOTE: Starboard versions 16 and older are no longer supported.**

## Version 17
Starboard 17 fully switches to POSIX APIs.

### Removed SbCPUFeaturesGet
The functionality is migrated to use `getauxval` instead.

### Added the following POSIX symbols:
* `access`
* `aligned_alloc`
* `chmod`
* `clock_nanosleep`
* `dup`
* `dup2`
* `epoll_create`
* `epoll_create1`
* `epoll_ctl`
* `epoll_wait`
* `eventfd`
* `fchmod`
* `fchown`
* `fdatasync`
* `fdopendir`
* `gai_strerror`
* `getauxval`
* `getcwd`
* `geteuid`
* `getpeername`
* `getpid`
* `getpriority`
* `getrlimit`
* `getsockopt`
* `isatty`
* `kill`
* `link`
* `lstat`
* `madvise`
* `malloc_usable_size`
* `mincore`
* `mkdtemp`
* `mkostemp`
* `mkstemp`
* `pathconf`
* `pause`
* `pipe`
* `pipe2`
* `poll`
* `prctl`
* `pread`
* `pthread_attr_getschedpolicy`
* `pthread_attr_getscope`
* `pthread_attr_getstack`
* `pthread_attr_setschedpolicy`
* `pthread_attr_setscope`
* `pthread_attr_setstack`
* `pthread_getattr_np`
* `pthread_getschedparam`
* `pthread_kill`
* `pthread_mutexattr_destroy`
* `pthread_mutexattr_getpshared`
* `pthread_mutexattr_gettype`
* `pthread_mutexattr_init`
* `pthread_mutexattr_setpshared`
* `pthread_mutexattr_settype`
* `pthread_rwlock_destroy`
* `pthread_rwlock_init`
* `pthread_rwlock_rdlock`
* `pthread_rwlock_tryrdlock`
* `pthread_rwlock_trywrlock`
* `pthread_rwlock_unlock`
* `pthread_rwlock_wrlock`
* `pthread_setschedparam`
* `pthread_sigmask`
* `pwrite`
* `raise`
* `rand`
* `rand_r`
* `readdir`
* `readlink`
* `readv`
* `realpath`
* `recvmmsg`
* `recvmsg`
* `rename`
* `sched_getaffinity`
* `sched_get_priority_max`
* `sched_get_priority_min`
* `select`
* `sem_destroy`
* `sem_init`
* `sem_post`
* `sem_timedwait`
* `sem_wait`
* `sendmsg`
* `setpriority`
* `shutdown`
* `sigaction`
* `signal`
* `socketpair`
* `srand`
* `statvfs`
* `symlink`
* `sysconf`
* `uname`
* `unlinkat`
* `utimensat`
* `writev`

### From `starboard/time_zone.h`
 * Removed `SbTimeZoneGetCurrent`. The time offset is now derived
   from the name returned by `SbTimeZoneGetName`, using the zoneinfo
   that is in the included ICU data.

### Removed the following SbEvents:
* `kSbEventTypeUser`.
* `kSbEventTypeAccessibilitySettingsChanged`
* `kSbEventTypeAccessibilityCaptionSettingsChanged`
* `kSbEventTypeAccessibilityTextToSpeechSettingsChanged`
* `kSbEventTypeOnScreenKeyboardShown`
* `kSbEventTypeOnScreenKeyboardHidden`
* `kSbEventTypeOnScreenKeyboardFocused`
* `kSbEventTypeOnScreenKeyboardBlurred`

### Removed `starboard/accessibility.h`
The functionality is migrated to a starboard extension.

### Removed `starboard/atomic.h`
### Removed `starboard/byte_swap.h`
### Removed `starboard/condition_variable.h`
### Removed `starboard/directory.h`
### Removed `starboard/image.h`
### Removed `starboard/mutex.h`
### Removed `starboard/memory.h`
### Removed `starboard/once.h`
### Removed `starboard/socket.h`
### Removed `starboard/string.h`
### Removed `starboard/time.h`
### Removed `starboard/ui_navigation.h`
The functionality is migrated to starboard extension.
### Removed `starboard/user.h`
### Removed unused symbols from `starboard/file.h`
### Removed unused symbols from `starboard/thread.h`
### Removed unused symbols from `starboard/types.h`
  * Removed provisions for SSIZE_T
### Removed `DEPRECATED_SCOPED_PTR`
### Removed `SB_HAS_IPV6` config
All platforms support IPv6
### Remove Win32/UWP support
### Remove quirk for `INT16_AUDIO
It was only used by win32 platforms
### Removed unused configuration variables
* `kSbDefaultMmapThreshold`
* `kSbFileAltSepChar`
* `kSbFileAltSepString`
* `kSbHasMediaWebmVp9Support`
* `kSbMallocAlignment`
* `kSbMediaVideoFrameAlignment`
* `kSbMemoryLogPath`
* `kSbPreferredRgbaByteOrder`
### Removed starboad/atomic.h support for C.
The existing code was migrated to C11 atomics.

### Renamed `SbMediaIs*Supported()` functions
`SbMediaIsAudioSupported()`, `SbMediaIsSupported()`, and
`SbMediaIsVideoSupported()` were never part of the Starboard interface.
Their Sb prefixes were removed and renamed to `MediaIsAudioSupported()`,
`MediaIsSupported()`, and `MediaIsVideoSupported()`.  They are also moved from
the global namespace into ::starboard::shared::starboard::media.

### Removed unused methods
* `SbMediaGetMaxBufferCapacity`. This method is no longer used. Use
  `SbMediaGetAudioBufferBudget` and `SbMediaGetVideoBufferBudget` instead.

## Version 16
A key update in Starboard version 16 is the adoption of POSIX APIs.
For a full overview of Starboard POSIX migrations, please refer to
[migration guide](/starboard/doc/starboard_16_posix.md).

#### Added new configuration constant `kHasPartialAudioFramesSupport`
Set this to true if your platform supports partial audio frames.

#### Added support of POSIX APIs
* Added support for `pthread` create attributes
  - The standard pthread APIs `pthread_attr_init`, `pthread_attr_destroy`,
    `pthread_attr_getdetachstate`, `pthread_attr_getstacksize`,
    `pthread_attr_setdetachstate`, `pthread_attr_setstacksize` were added.

* Added Socket Waiter API for POSIX.
  - Introduced Starboard functions `SbPosixSocketWaiterAdd`, `SbPosixSocketWaiterRemove`
    and callback function `SbPosixSocketWaiterCallback` to support POSIX based socket
    APIs.

* Added standard POSIX file `open` and `close` APIs
  - The standard API `open` can be used from <fcntl.h> and `close` can be used
    from <unistd.h>.

* Added standard POSIX socket `getaddrinfo`/`freeaddrinfo` APIs
  - The standard API `getaddrinfo` and `freeaddrinfo`, can be used from <netdb.h>.

* Added standard POSIX socket `send`/`recv` APIs
  - The standard API `send`, `sendto`, `recv`, `recvfrom`, can be used
    from <sys/socket.h> and `fcntl` can be used from <fcntl.h>, to set
    socket to non-blocking.

* Added standard POSIX socket `bind`/`listen`/`connect`/`accept` APIs
  - The standard API `bind`, `listen`, `connect`, `accept` can be used
    from <sys/socket.h> and `getifaddrs`, `freeifaddrs` can be used from <ifaddrs.h>.

* Added standard POSIX `socket`/`close`/`setsockopt` APIs
  - The standard API `socket`, `setsockopt` can be used from <sys/socket.h>
    and `close` can be called to close the socket by including <unistd.h>.

### Updates / Improvements
#### Migrate existing APIs with POSIX APIs
* Migrated `SbMutex`, `SbConditionVariable` and `SbThread`
  - The standard POSIX `pthread` APIs replace the Starboard concurrency primitives.

* Migrated `SbThreadSampler` to use `pthread`
  - Switched the `SbThreadSampler` API to use `pthread` instead of `SbThread`.

* Migrated `SbThreadSetName` and `SbThreadGetName` APIs
  - Replaced the `SbThreadSetName`/`SbThreadGetName` with the POSIX
    `pthread_setname_np`/`pthread_getname_np`.

* Migrated `SbThreadLocalKey`
  - Replaced the `SbThreadLocalKey` with the POSIX `pthread_key_t`.

* Migrated `SbOnce`
  - Replaced the `SbOnce` with the POSIX `pthread_once`.

* Migrated `SbThreadSleep`
  - Replaced the `SbThreadSleep` with the POSIX `usleep` defined in the
    <unistd.h> header.

* Migrated `SbThreadYield`
  - Replaced the `SbThreadYield` with the POSIX `sched_yield` defined in the
    <sched.h> header.

* Migrated `SbDirectoryCreate`
  - The directory API `SbDirectoryCreate` has been deprecated and the standard
    API `mkdir` is used from <dirent.h> instead.

* Migrated `SbDirectoryCanOpen`, `SbFileExists`, and `SbFileGetPathInfo`
  - The directory and File APIs `SbDirectoryCanOpen`, `SbFileExists`, and
    `SbFileGetPathInfo` have been deprecated and the standard API `stat` is
    used from <sys/stat.h> instead.

* Migrated `SbLogIsTty`
  - The `SbLogIsTty` has been deprecated and `isatty` will be used if needed.

* Migrated `SbTime` APIs
  - The time APIs `SbTimeGetNow`, `SbTimeGetMonotonicNow`,
    `SbTimeIsTimeThreadNowSupported` and `SbTimeGetMonotonicThreadNow` are
    deprecated and the standard APIs `gettimeofday` from <sys/time.h> and
    `clock_gettime` from <time.h> should be used instead.

* Migrated `SbStringDuplicate`
  - The string duplicate API `SbStringDuplicate` is deprecated and the standard
    API `strdup` from <string.h> should be used instead

* Migrated `SbStringFormat` APIs
  - The StringFormat management APIs `SbStringFormat`, `SbStringFormatF`,
    `SbStringFormatWide`, `SbStringFormatUnsifeF` are deprecated and the
    standard APIs `vsnprintf`, `vfnprintf`, `vswprintf`, `snprintf`
    from <stdlib.h> should be used instead.

* Migrated `SbMemoryMap` APIs
  - The memory mapping APIs `SbMemoryMap`, `SbMemoryUnmap`, `SbMemoryProtect` and
    `SbMemoryFlush` are deprecated and the standard APIs `mmap`, `munmap`,
    `mprotect`, `msync` from <sys/mman.h> should be used instead.

* Migrated `SbMemory allocation` APIs
  - The memory management APIs `SbMemoryAllocate`, `SbMemoryReallocate`,
    `SbMemoryCalloc`, `SbMemoryAllocateAligned`, `SbMemoryDeallocate`,
    and `SbMemoryDeallocateAligned` are deprecated and the
    standard APIs `malloc`, `realloc`, `calloc`, `posix_memalign`, `free`
    from <stdlib.h> should be used instead.

* Migrated `SbStringScan` and `SbStringScanF`
  - The APIs defined in `starboard/string.h` are deprecated and the standard API
    `vsscanf` and `sscanf` are used instead.

#### Convert Starboard APIs into Starboard extensions
* Convert `SbUiNavGetInterface` Starboard API into an extension
  - The `SbUiNavGetInterface` API is deprecated and replaced with a Starboard
    extension named `SbUiNavInterface`.

* Convert `Accessibility` Starboard APIs into an extension
  - The extension is available in `starboard/extension/accessibility.h`
  The removal includes the following functions:
    * `SbAccessibilityGetCaptionSettings`
    * `SbAccessibilityGetDisplaySettings`
    * `SbAccessibilityGetTextToSpeechSettings`
    * `SbAccessibilitySetCaptionsEnabled`

* Convert `OnScreenKeyboard` Starboard APIs into an extension
  - The extension is available in `starboard/extension/on_screen_keyboard.h`
  - The removal includes the following functions:
    * `SbWindowBlurOnScreenKeyboard`
    * `SbWindowFocusOnScreenKeyboard`
    * `SbWindowGetOnScreenKeyboardBoundingRect`
    * `SbWindowHideOnScreenKeyboard`
    * `SbWindowIsOnScreenKeyboardShown`
    * `SbWindowOnScreenKeyboardIsSupported`
    * `SbWindowOnScreenKeyboardSuggestionsSupported`
    * `SbWindowSetOnScreenKeyboardKeepFocus`
    * `SbWindowShowOnScreenKeyboard`
    * `SbWindowUpdateOnScreenKeyboardSuggestions`
  - The config value of `SB_HAS_ON_SCREEN_KEYBOARD` is also removed.

#### Other changes
* `MAP_EXECUTABLE_MEMORY` changed from build-time to runtime config
  - SB_CAN_MAP_EXECUTABLE_MEMORY has been refactored into a run-time configuration
    constant `kSbCanMapExecutableMemory`.

* Changed `InstallCrashpadHandler` API
  - This API doesn't support the option to start the crashpad handler at the
    same time as the app launches anymore. Instead, the crashpad handler is
    started when a crash happens. See details in
    [starboard/doc/crash_handlers.md](/starboard/doc/crash_handlers.md).

* Media buffer pools are now enforced
  - Media buffers are now always allocated using buffer pools. The function
    `SbMediaIsBufferUsingMemoryPool` no longer has any effect and is required to
    always return `true`. It will be removed in the next Starboard version.

### Deprecations
#### x86 ABI removed for Evergreen
The x86 platform configurations, builds and ABI are no longer supported for
Evergreen.

####  Removed configs for `SB_EXPORT_PLATFORM` and `SB_IMPORT_PLATFORM`
These are auto-detected based on compilers, platforms can optionally override.

#### Removed configs for `SB_C_FORCE_INLINE`
This is now automatically defined based on compilers, platforms must not provide
a definition.

#### Deprecated `SB_C_INLINE`
Use C99 standard `inline` function specifier instead.

#### Removed `SB_C_NOINLINE`
This is only used for testing, a similar header is now found under
`starboard/shared/testing/no_inline.h`

#### Removed `SB_HAS_NV12_TEXTURE_SUPPORT`
This flag is resolved at run-time.

#### Removed `SB_HAS_PIPE`
This flag is no longer used.

#### Removed `SB_HAS_SPEECH_SYNTHESIS`
This configuration has been replaced by `SbSpeechSynthesisIsSupported` function

#### Deprecated `SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES`
`SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES` can no longer be used to enable the
use of `kSbMediaAudioSampleTypeInt16`.  The platform has to support AudioSink in
float sample and is verified by nplb test.  The enum value of
`kSbMediaAudioSampleTypeInt16Deprecated` was kept so the platform may still
choose to implement int16 sample support.  It will be removed in a future
version.

#### Removed `QUIRK_HASH_FILE_NAME`
No platforms are using this config anymore.

#### Removed `QUIRK_DOES_NOT_STACK_ALIGN_OVER_16_BYTES`
This configuration is not used in Cobalt.

#### Removed `QUIRK_SOCKET_BSD_HEADERS`
This config flag is unused.

#### Removed `FILESYSTEM_ZERO_FILEINFO_TIME` and `COARSE_ACCESS_TIME`
These are no longer used in Starboard and Cobalt.

#### Removed `VIRTUAL_REALITY`
This configuration not used in Cobalt.

#### Removed configuration for `abort_on_allocation_failure`
This flag has no effect in builds, and checked allocations are removed
in Starboard 16.

#### GLES2 configuration mandatory
SB_HAS_GLES2 configuration has been removed, and `gl_type` GN config no longer
accepts `none` as an option.

#### Removed pre-C++11 hash map configuration
Build configurations for `SB_HAS_STD_UNORDERED_HASH`, `SB_HAS_LONG_LONG_HASH`,
 `SB_HAS_STRING_HASH`, `SB_HAS_HASH_USING`, `SB_HAS_HASH_VALUE`,
 `SB_HAS_HASH_WARNING`, `SB_HASH_MAP_INCLUDE`, `SB_HASH_NAMESPACE`, and
 `SB_HASH_SET_INCLUDE` are fully removed. C++ standard `unordered_map` and
 `unordered_set` are used in all builds.

#### Deprecated `SbMediaGetBufferAlignment`
The `SbMediaGetBufferAlignment` API was deprecated, its return value is no
longer used when allocating media buffers and has to be always set to
sizeof(void*).  This is verified explicitly using nplb tests.
The app MAY take best effort to allocate media buffers aligned to an optimal
alignment for the platform, but not guaranteed.
An implementation that has specific alignment requirement should check the
alignment of the incoming buffer, and make a copy when necessary.

#### Deprecated `SbMediaGetBufferPadding`
The `SbMediaGetBufferPadding` API was deprecated, its return value is no longer
used when allocating media buffers and has to be always set to 0.  This is
verified explicitly using nplb tests.
An implementation that has specific padding requirement should make a copy of
the incoming buffer when necessary.

#### Deprecated `SbMediaGetBufferStorageType`
The `SbMediaGetBufferPadding` API was deprecated. `SbMediaBufferStorageType` was
also deprecated as a result.

#### Deprecated `SbFile` APIs
The APIs defined in `starboard/file.h` are deprecated.
- The involved functions include:
  * SbFileOpen
  * SbFileClose
  * SbFileSeek
  * SbFileRead
  * SbFileWrite
  * SbFileTruncate
  * SbFileFlush
  * SbFileGetInfo
  * SbFileGetPathInfo
  * SbFileDelete
  * SbFileCanOpen
  * SbFileModeStringToFlags
  * SbFileReadAll
  * SbFileWriteAll

#### Removed `SbByteSwapS16`, `SbByteSwapS32`, `SbByteSwapS64`, `SbByteSwapU16`, `SbByteSwapU32`, and `SbByteSwapU64`
The APIs defined in `starboard/byte_swap.h` are no longer used and have been
deprecated.

#### Removed `SbImageDecode` and `SbImageIsDecodeSupported`
The APIs defined in `starboard/image.h` are no longer used and have been
deprecated.

#### Removed `SbUserGetCurrent`, `SbUserGetSignedIn`, `SbUserGetProperty`, `SbUserGetPropertySize`, and `kSbUserMaxSignedIn`
The APIs defined in `starboard/user.h` are no longer used and have been deprecated.

#### Removed `SbUser` from `SbStorageOpenRecord` and `SbStorageDeleteRecord`
The `SbStorageOpenRecord` and `SbStorageDeleteRecord` APIs defined in
`starboard/storage.h` no longer have a parameter for `SbUser` as the APIs are
user-agnostic.

## Version 15

### SbMemoryReporter is no longer used

`SbMemoryReporter` defined in `starboard/memory_reporter.h`` is no longer used
and is considered deprecated. It will be removed in upcoming Starboard versions.
The corresponding `SbMemoryAllocateNoReport` and `SbMemoryDeallocateNoReport`
are also deprecated.

### Removed version suffixes of SbPlayer functions and structures
Renamed SbPlayerInfo2 to SbPlayerInfo, SbPlayerSeek2() to SbPlayerSeek(),and
SbPlayerGetInfo2() to SbPlayerGetInfo(), as the version suffixes are no longer
necessary.

### Cobalt extensions are now Starboard extensions
Previously named Cobalt extensions are now found under `starboard/extensions`.
The mechanism extends platform-specific functionality of Starboard via runtime
resolution, and hence more properly belongs in Starboard codebase.
This also helps break the dependency cycle between Starboard and Cobalt for
cleaner component layering.
For existing uses in Starboard ports, fallback forwarding headers are provided
in the previous location of the code in `cobalt/extensions`.

### Refined SbMediaAudioSampleInfo and SbMediaVideoSampleInfo
Moved attributes of `SbMediaAudioSampleInfo` and `SbMedidaVideoSampleInfo` that
specific to the stream (i.e. don't change per access unit) to
`SbMediaAudioStreamInfo` and `SbMediaVideoStreamInfo`.

### Add duration discard support to SbMediaAudioSampleInfo
This allows to discard specific durations from the front and the back of any
audio access units to improve the accuracy of audio playback.  For example, in
an AAC stream where each access unit contains 1024 frames, this allows to
discard the last 300 frames of the 1024 frames when the duration of the stream
isn't aligned to 1024 frames.

### Renamed SbPlayerWriteSample2() to SbPlayerWriteSamples()
To better reflect the fact that it may write multiple samples in one call.

### Add IAMF value to SbMediaAudioCodec.
This makes it possible to support IAMF in the future.

### Added SB_MODULAR_BUILD for supporting modular builds
This configuration is set for modular builds, which have:
  1. Application binary built as a shared library.
  2. Either of a) or b)
        a. Starboard built at a shared library and a separate loader_app executable.
        b. A loader_app executable with Starboard built in ( Evergreen ).

### Split C++ and C code from `starboard/atomic.h`
`starboard/atomic.h` previously included C++ wrapper code for atomic operations.
This code has been separated out and refactored into `starboard/common/atomic.h`
to break dependency cycle between Starboard interface and Starboard Common C++
library.

### Improves audio access unit processing.
  1.  Abstracted stream specific info from SbMediaAudioSampleInfo and
      SbMediaVideoSampleInfo into SbMediaAudioStreamInfo and
      SbMediaVideoStreamInfo.
  2.  Removed unused info about the audio stream.
  3.  Renamed SbPlayerWriteSample2() to SbPlayerWriteSamples().
  4.  Improved accuracy of audio write duration handling.
      The app will set audio write duration to 0.5 seconds for wired audio
      output device, and 10 seconds for wireless audio output device.
      Added `SbPlayerGetAudioConfiguration()` to allow the app to query
      active audio output devices used by the SbPlayer.
      `SbMediaSetAudioWriteDuration()` is deprecated as a result.
      Note that the app used to set audio write duration to 10 seconds at
      playback startup or seek to accommodate wireless devices, now the app
      sets the audio write duration to 0.5 seconds at all time for wired
      devices.
  5.  Refined SbMediaAudioConnector.
      Renamed `kSbMediaAudioConnectorNone` to
      `kSbMediaAudioConnectorUnknown`, as the implementation should only use
      this value when the audio output is available but its type cannot be
      determined.
  6.  Removed index from SbMediaAudioConfiguration.
      Removed unused member variable `SbMediaAudioConfiguration::index`.

### Removed SbMemoryGetStackBounds
This API is unused by Cobalt and has been deprecated.

### Add the keycode for the remote record button.
By supporting the keycode for the remote record button on Cobalt, the webapp can be
programmed to respond to the remote record button in the future.

### Removed SbSystemDeviceType and added kSbSystemPropertyDeviceType
This enumeration of device types `SbSystemDeviceType` has been deprecated. We
added new system property `kSbSystemPropertyDeviceType` for device type as a string.
All the allowed device type values are now checked by nplb test:
`starboard/nplb/system_get_property_test.cc`.

### Deprecated kSbHasAc3Audio
This constant is no longer used and has been deprecated.

### Removed BILINEAR_FILTERING_SUPPORT config
The unused macro for SB_HAS_BILINEAR_FILTERING_SUPPORT feature has been removed.

## Version 14
### Add MP3, FLAC, and PCM values to SbMediaAudioCodec.
This makes it possible to support these codecs in the future.

### Add kSbSystemPropertyAdvertisingId and kSbSystemPropertyLimitAdTracking
Adds properties for retrieving IFA identifier and related ad tracking limiting.

### Add kSbSystemDeviceTypeVideoProjector type to Starboard devices.
This adds a video projector type to Starboard devices.

### Deprecated SystemPathTestOutputDirectory.
This duplicated SystemPathDebugOutputDirectory without a useful distinction.

### Introduce a new format kSbDecodeTargetFormat3Plane10BitYUVI420Compact.
A decoder target format consisting of 10bit Y, U, and V planes.

### Deprecated the usage of SB_HAS_PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT.
The improvements on player creation and output mode query (like
`SbPlayerCreationParam` and `SbPlayerGetPreferredOutputMode()`) are always
enabled.  This change also deprecates `SbPlayerOutputModeSupported()`.

### Deprecated the usage of SB_HAS_MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT
The extra parameters (like `profile` and `level`) on `SbMediaIsVideoSupported()`
are always enabled.  This change also deprecated
`SbMediaIsTransferCharacteristicsSupported()`.

### Deprecated kSbMediaMatrixIdUnknown and added kSbMediaMatrixIdInvalid.
`kSbMediaMatrixIdUnknown` has been deprecated. `kSbMediaMatrixIdInvalid`
was added.

### Deprecated `SbMediaType type` parameter to some SbMedia buffer functions.
`SbMediaGetBufferAlignment()` and `SbMediaGetBufferPadding()` no longer accept
`SbMediaType type` as parameters.  The implementation has to return the same
values for both audio and video streams.

### Made `starboard::QueueApplication::DispatchAndDelete` private
Since `starboard::shared::starboard::QueueApplication` has an event queue,
`QueueApplication::Inject` should be used if an event just needs to be added to
the queue or `QueueApplication::InjectAndProcess` if the event needs to be
handled before returning. `QueueApplication::DispatchAndDelete` jumps the event
queue and could lead to bugs due to the event (especially lifecycle events)
being handled out of order.

### Add kSbMaxSystemPathCacheDirectorySize configuration constant.
This constant defines maximum space in bytes the cache directory
`kSbSystemPathCacheDirectory` can use. The default value is 24MiB. Platforms can
set `kSbMaxSystemPathCacheDirectorySize` to a larger value in
"starboard/<PLATFORM_PATH>/configuration_constants.cc".

### Add nplb tests to enforce performance of SbMediaCanPlayMimeAndKeySystem().
Add SbMediaCanPlayMimeAndKeySystem.ValidatePerformance to enforce the
performance of SbMediaCanPlayMimeAndKeySystem(). On platforms that fail such
tests, MimeSupportabilityCache and KeySystemSupportabilityCache can be enabled
to improve the performance. The caches store the results of previous queries and
reuse them for repeated queries. Note that if caches are enabled, the platform
need to clear the caches if there's any codec or audio/video output capability
change.

## Version 13
### Changed lifecycle events to add support for a concealed state.

* The *Pause* event is renamed to *Blur*.
* The *Unpause* event is renamed to *Focus*.
* The *Suspend* event is replaced by *Conceal* and *Freeze*.
* The *Resume* event is replaced by *Unfreeze* and *Reveal*.

Most platforms should only need to replace 'Pause' with 'Blur', 'Unpause' with
'Focus', 'Suspend' with 'Freeze', and 'Resume' with 'Reveal'.

Since there is no longer a special *Preloading* state, applications should no
longer use the *Start* event when a preloaded application is brought to the
foreground. Instead, the same event(s) used for backgrounded applications
(*Concealed* or *Frozen*) should be used.

See `cobalt/doc/lifecycle.md` for more details.

### Added network connectivity events and function.

Added `kSbEventTypeOsNetworkDisconnected` and `kSbEventTypeOsNetworkConnected`
so the platform can provide hints to the application of network connectivity
changes.

Added function `SbSystemNetworkIsDisconnected()` to allow the application to
query the network connectivity status.

### Added an event for date/time configuration changes.

If the platform detects a change in the date/time configuration (e.g. timezone
change), then it should send the new `kSbEventDateTimeConfigurationChanged`
event.

### Deprecated speech recognizer API.

The `starboard/speech_recognizer.h` APIs have been deprecated -- even for
platforms that define SB_HAS(SPEECH_RECOGNIZER). Instead, the application is
expected to use the `starboard/microphone.h` APIs.

### Updated platform-based UI navigation API.

Added functions to direct the platform's UI engine to maintain focus on an item
for a specific time before allowing focus to change. Also added a function to
perform a batch of UI updates so that UI changes are atomic.

Functionality for a few existing APIs were clarified without changing the API
itself.

### Fixed spelling on accessibility events.

Changed `kSbEventTypeAccessiblityTextToSpeechSettingsChanged` to
`kSbEventTypeAccessibilityTextToSpeechSettingsChanged`.

Changed `kSbEventTypeAccessiblitySettingsChanged` to
`kSbEventTypeAccessibilitySettingsChanged`.

### Deprecated some starboard macros.

The following macros have been removed:

* `SB_TRUE`
* `SB_FALSE`
* `SB_OVERRIDE`
* `SB_DISALLOW_COPY_AND_ASSIGN`

### Deprecated some standalone starboard functions.

The following starboard functions have been removed:

* `SbCharacterIsAlphanumeric`
* `SbCharacterIsDigit`
* `SbCharacterIsHexDigit`
* `SbCharacterIsSpace`
* `SbCharacterIsUpper`
* `SbCharacterToLower`
* `SbCharacterToUpper`
* `SbStringConcat`
* `SbStringConcatUnsafe`
* `SbStringConcatWide`
* `SbDoubleAbsolute`
* `SbDoubleExponent`
* `SbDoubleFloor`
* `SbDoubleIsFinite`
* `SbDoubleIsNan`
* `SbMemoryAlignToPageSize`
* `SbMemoryCompare`
* `SbMemoryCopy`
* `SbMemoryFindByte`
* `SbMemoryIsAligned`
* `SbMemoryIsZero`
* `SbMemoryMove`
* `SbMemorySet`
* `SbStringCompare`
* `SbStringCompareAll`
* `SbStringCompareWide`
* `SbStringCopy`
* `SbStringCopyUnsafe`
* `SbStringCopyWide`
* `SbStringFindCharacter`
* `SbStringFindLastCharacter`
* `SbStringFindString`
* `SbStringGetLength`
* `SbStringGetLengthWide`
* `SbStringParseDouble`
* `SbStringParseSignedInteger`
* `SbStringParseUInt64`
* `SbStringParseUnsignedInteger`
* `SbSystemBinarySearch`
* `SbSystemSort`

### Add SbTimeMonotonic timestamp into SbEvent.

### Remove SbTimeMonotonic timestamp from SbInputData.

### Deprecated kSbSystemCapabilitySetsInputTimestamp from SbSystemCapabilityId.

The new timestamp field from `SbEvent` will be used instead of the deprecated
timestamp in `SbInputData`.

### Moved `SbMediaIsSupported()` from `media.h` to `media_support_internal.h`.

It has never been used by Cobalt.  It is only used inside Starboard by functions
like `CanPlayMimeAndKeySystem()`, and is no longer exposed as a Starboard
interface function.

## Version 12
### Add support for platform-based UI navigation.

The system can be disabled by implementing the function
`SbUiNavGetInterface()` to return `false`.  Platform-based UI navigation
allows the platform to receive feedback on where UI elements are located and
also lets the platform control what is selected and what the scroll
parameters are.

NOTE: This API is not used in the production web app yet, so please use the
stub implementation for `SbUiNavGetInterface()` for now.

### Require the OpenGL and Skia renderers on all platforms.

The system must implement `SbGetGlesInterface()` in `starboard/gles.h`
or use the provided stub implementation.

This change also effectively deprecates the gyp variable
"enable_map_to_mesh" in favor of CobaltGraphicsExtensionApi function
`IsMapToMeshEnabled()` and the command line switch --disable_map_to_mesh.
Now, Cobalt will assume the platform supports map_to_mesh, so platforms that
do not will have to have return |false| from `IsMapToMeshEnabled()` or use
the provided command line switch.

### Require the captions API.

The system must implement the captions functions in
`starboard/accessibility.h` or use the provided stub implementations.
System caption can be disabled by implementing the function
`SbAccessibilityGetCaptionSettings(SbAccessibilityCaptionSettings*
caption_settings)` to return false as the stub implementation does.
This change also deprecates the SB_HAS_CAPTIONS flag.

### Require compilation with IPv6.

Cobalt must be able to determine at runtime if the system supports IPv6.
IPv6 can be disabled by defining SB_HAS_IPV6 to 0.

### Require the microphone API.

The system must implement the microphone functions in
`starboard/microphone.h` or use the provided stub functions.
The microphone can be disabled by having `SbMicrophoneCreate()` return
|kSbMicrophoneInvalid|.
This change also deprecates the SB_HAS_MICROPHONE flag.

###  Require the memory mapping API.

The system must implement the memory mapping functions in
`starboard/memory.h` and `starboard/shared/dlmalloc.h` or use the provided
stub implementations.
This change also deprecates the SB_HAS_MMAP flag.

### Require the on screen keyboard API.

The system must implement the on screen keyboard functions in
`starboard/window.h` or use the provided stub implementations.
The on screen keyboard can be disabled by implementing the function
`SbWindowOnScreenKeyboardIsSupported()` to return false
as the stub implementation does.

### Require speech recognizer API.

The system must implement the functions in `starboard/speech_recognizer.h`
or use the provided stub implementations.
The speech recognizer can be disabled by implementing the function
`SbSpeechRecognizerIsSupported()` to return `false` as the stub
implementation does.

### Require the speech synthesis API.

The system must implement the speech synthesis function in
`starboard/speech_synthesis.h` or use the provided stub implementations.
Speech synthesis can be disabled by implementing the function
`SbSpeechSynthesisIsSupported()` to return false as the stub
implementation does.

### Require the time thread now API.

The system must implement the time thread now functions in
`starboard/time.h` or use the provided stub implementations.
Time thread now can be disabled by implementing the function
`SbTimeIsTimeThreadNowSupported()` to return false as the stub
implementation does.

### Add SbFileAtomicReplace API.

Introduce the Starboard function SbFileAtomicReplace() to provide the ability
to atomically replace the content of a file.

### Introduces new system property kSbSystemPathStorageDirectory.

Path to directory for permanent storage. Both read and write
access are required.

### Introduce Starboard Application Binary Interface (SABI) files.

SABI files are used to describe the configuration for targets such that two
targets, built with the same SABI file and varying toolchains, have
compatible Starboard APIs and ABIs.

With this define, we have:
1) Moved architecture specific defines and configurations from
  configuration_public.h and *.gyp[i] files into SABI files.
2) Included the appropriate SABI file in each platform configuration.
3) Included the //starboard/sabi/sabi.gypi file in each platform
  configuration which consumes SABI file fields and defines a set of
  constants that are accessible when building.
4) Provided a set of tests that ensure the toolchain being used produces
  an executable or shared library that conforms to the included SABI file.

For further information on what is provided by SABI files, or how these
values are consumed, take a look at //starboard/sabi.

### Updates the API guarantees of SbMutexAcquireTry.

SbMutexAcquireTry now has undefined behavior when it is invoked on a mutex
that has already been locked by the calling thread. In addition, since
SbMutexAcquireTry was used in SbMutexDestroy, SbMutexDestroy now has
undefined behavior when invoked on a locked mutex.

### Migrate the Starboard configuration variables from macros to extern consts.

The migration allows Cobalt to make platform level decisions at runtime
instead of compile time which lets us create a more comprehensive Cobalt
binary.

This means Cobalt must remove all references to these macros that would not
translate well to constants, i.e. in compile time references or initializing
arrays. Therefore, we needed to change the functionality of the function
`SbDirectoryGetNext` in "starboard/directory.h". Because we do not want to
use variable length arrays, we pass in a c-string and length to the function
to achieve the same result as before when passing in a `SbDirectoryEntry`.

A platform will define the extern constants declared in
"starboard/configuration_constants.h". The definitions are done in
"starboard/<PLATFORM_PATH>/configuration_constants.cc".
### Improve player creation and output mode query.

1. Introduce the new type SbPlayerCreationParam that holds the common
parameters used to create an SbPlayer() and to query for the output mode
support.

2. Replace SbPlayerOutputModeSupported() by SbPlayerGetPreferredOutputMode()
so the SbPlayer implementation can explicitly indicate its preference on
output mode, when all output modes are supported.
For example, Cobalt used to always query for |kSbPlayerOutputModePunchOut|
first, without providing details about the video going to be played, and
not query for output modes if punch out is supported.  The new interface
allows the implementation to fine tune its output mode.  For example, it
may decide to use |kSbPlayerOutputModeDecodeToTexture| for low resolution
videos.

### Introduce error handling into reference SbAudioSinkPrivate.

The implementation is in:
"starboard/shared/starboard/audio_sink/audio_sink_internal.*".

### Change the thread types to be portable with stable ABI.

The following types were updated:
SbThread, SbMutex, SbOnce and SbConditionVariable.

### Introduce support of cbcs encryption scheme into SbDrmSystem.

The definition follows ISO/IEC 23001 part 7.

### Add link register to SbThreadContext.

### Make GYP configuration variables cobalt extensions instead.

This change moves all of the GYP configuration variables to be members of
the struct declared in "cobalt/extension/configuration.h". All members are
function pointers that can be set for each platform, otherwise defaults
will be used. These can be referenced through functions declared in
"cobalt/configuration/configuration.h", which will use the extension API if
available, but will otherwise fall back onto default values.

### Add the PCLMULQDQ instruction feature.

The PCLMULQDQ was added to the Starboard CPU features interface
for x86 architectures.

###  |content_type| is added to SbMediaIsVideoSupported() and
SbMediaIsAudioSupported().

### Enables a test that checks that Opus is supported.

### Add `kSbSystemPropertySystemIntegratorName`

This change also deprecates `kSbSystemPropertyOriginalDesignManufacturerName`.
The `kSbSystemPropertySystemIntegratorName` value will represent the corporate
entity responsible for submitting the device to YouTube certification and for
the device maintenance/updates.

### Deprecated the Blitter API.

Blitter API is no longer supported on any platform. Use the OpenGL ES
interface instead.

### Deprecated the Crypto API.

Crypto API is no longer supported on any platform. BoringSSL CPU
optimizations are used instead.

### Deprecate the SB_HAS_VIRTUAL_REGIONS flag as all platforms define it to 0.

### Deprecate the usage of SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER.

### Deprecated unused enums |kSbPlayerDecoderStateBufferFull| and
|kSbPlayerDecoderStateDestroyed|.

### Deprecated the usage of |SbMediaIsOutputProtected()| and
|SbMediaSetOutputProtection()|.

### Deprecated the |SB_HAS_QUIRK_SEEK_TO_KEYFRAME| macro.

### Deprecated the |SB_HAS_ASYNC_AUDIO_FRAMES_REPORTING| macro.

### Deprecated 'cobalt_minimum_frame_time_in_milliseconds'.

The variable 'cobalt_minimum_frame_time_in_milliseconds' is deprecated
in favor of the usage of
'CobaltExtensionGraphicsApi::GetMinimumFrameIntervalInMilliseconds' API.
The declaration of 'GetMinimumFrameIntervalInMilliseconds' can be found
in cobalt/renderer/backend/graphics_context.h

### Deprecate support for GLES3 features.

### Deprecate Web Extension support.

The Platform Services API should be used
instead. See cobalt/doc/platform_services.md.

### Add event for text-to-speech settings changes.

If the platform supports text-to-speech settings, it must use the new
kSbEventTypeAccessiblityTextToSpeechSettingsChanged event to inform the app
when those settings change. For older starboard versions, use
kSbEventTypeAccessiblitySettingsChanged instead.

### Add extension to SbMediaCanPlayMimeAndKeySystem() for encryptionScheme.

Now the Starboard implementation may choose to support |key_system| with extra
attributes, in order to selectively support encryption schemes on particular
containers or codecs.
The Starboard implementation needn't support |key_system| with extra attributes
if it meets the requirements for the default implementation of
`Navigator.requestMediaKeySystemAccess()`, which assumes that:
1. When the Widevine DRM system is used, all the encryption schemes ('cenc',
   'cbcs', 'cbcs-1-9') should be supported across all containers and codecs
   supported by the platform.
2. When the PlayReady DRM system is used, only 'cenc' is supported across all
   containers and codecs supported by the platform.

Please see the comment of `SbMediaCanPlayMimeAndKeySystem()` in `media.h` for
more details.

## Version 11

### Add arguments to `SbMediaIsVideoSupported`.

Add arguments for profile, level, bit depth, color primaries,
transfer characteristics, and matrix coefficients. See comments of
`SbMediaIsVideoSupported` for more details.  Also, the function
`SbMediaIsTransferCharacteristicsSupported()` is no longer necessary and is
removed.

### Add support for AC3 audio.

### Replace `kSbMediaVideoCodecVp10` with `kSbMediaVideoCodecAv1`.

### Add a new enum `kSbPlayerErrorMax` in `starboard/player.h`.

### Add new parameter `max_video_capabilities` to `SbPlayerCreate()`.

This gives the application the option to specify to the `SbPlayer` object
what the maximum video specifications will be, as a hint to the platform
on how to allocate resources for the `SbPlayer`.  For example, if the player
will never exceed a 240p playback resolution, then a software decoder may
be initialized. Please see comment in `SbPlayerCreate()` for more details.

### Refactor `SbPlayerSampleInfo` to reuse `SbPlayerSampleInfo` in filter based player.

Additionally, add audio and codec info for every sample.

### Introduce audio write duration

Add a function `SbMediaSetAudioWriteDuration()` to `starboard/media.h`
which communicates to the platform how much audio will be sent to the
platform at a time.

### Introduce Cobalt Extensions using the SbSystemGetExtension interface.

Cobalt extensions implement app & platform specific functionality.

### Deprecate unused function `SbSystemClearPlatformError()`.

### Deprecate `kSbEventTypeNetworkDisconnect` and `kSbEventTypeNetworkConnect`.

### Add support for using C++11 standard unordered maps and sets.

By setting `SB_HAS_STD_UNORDERED_HASH` to 1, a platform can be configured
to use C++11 standard hash table implementations, specifically, using:

  - `std::unordered_map<>` for `base::hash_map<>`
  - `std::unordered_multimap<>` for `base::hash_multimap<>`
  - `std::unordered_set<>` for `base::hash_set<>`
  - `std::unordered_multiset<>` for `base::hash_multiset<>`

When `SB_HAS_STD_UNORDERED_HASH` is used, it is no longer necessary to
specify `SB_HAS_LONG_LONG_HASH`, `SB_HAS_STRING_HASH`, `SB_HAS_HASH_USING`,
`SB_HAS_HASH_VALUE`, `SB_HAS_HASH_WARNING`, `SB_HASH_MAP_INCLUDE`,
`SB_HASH_NAMESPACE`, or `SB_HASH_SET_INCLUDE`.

### Adds support for specifying screen diagonal length.

When a platform knows its physical screen diagonal length, it can now provide
that data to the application via `SbWindowGetDiagonalSizeInInches()`.


### Add support for device authentication system properties.

The system properties `kSbSystemPropertyCertificationScope` and
`kSbSystemPropertyBase64EncodedCertificationSecret` have been added to enable
client apps to perform device authentication.  The values will be queried by
calls to `SbSystemGetProperty()` in `starboard/system.h`. An alternative to
providing the `kSbSystemPropertyBase64EncodedCertificationSecret` property is
to implement the SbSystemSignWithCertificationSecretKey() function, enabling
the key to remain private and secure.

### Add support for `SbThreadSampler` and `SbThreadContext`.

This is helpful for enabling the implementation of sampling-based profilers.
A full implementation is only required if the new function
`SbThreadSamplerIsSupported()` returns `true`.  A valid implementation will need
to implement the new `starboard/thread.h` functions,
`SbThreadContextGetPointer()`, `SbThreadSamplerIsSupported()`,
`SbThreadSamplerCreate()`, `SbThreadSamplerDestroy()`,
`SbThreadSamplerFreeze()`, `SbThreadSamplerThaw()`.

### Introduce functions for supplying suggestions to the on screen keyboard.

A new API in `starboard/window.h` is introduced which declares the functions
`SbWindowUpdateOnScreenKeyboardSuggestions()` and
`SbWindowOnScreenKeyboardSuggestionsSupported()`.  This is only relevant if
`SB_HAS(ON_SCREEN_KEYBOARD)`.

### Introduce new file error code `kSbFileErrorIO`.

The new code, added to `starboard/file.h`, should match "EIO" on Posix
platforms.

### Move the definition of `FormatString()` from `starboard/string.h` to `starboard/format_string.h`.

### Make the decode target content region parameters floats instead of ints.

The `SbDecodeTargetInfoContentRegion` struct is modified to accept `float`s
instead of `int`s. The primary motivation for this change is to make it so that
on platforms where it is difficult to obtain the width and height of a texture,
we can still correctly identify a precise fractional "normalized" content region
with the texture width and height set to 1.

### Add `kSbSystemPropertyOriginalDesignManufacturerName` enum value.

This change also deprecates `kSbSystemPropertyNetworkOperatorName`.
The `kSbSystemPropertyOriginalDesignManufacturerName` value will represent
the corporate entity responsible for the manufacturing/assembly of the device
on behalf of the business entity owning the brand.

### Cross-platform helper Starboard definitions factored out of core interface.

Cross-platform helper Starboard definitions refactored out of `/starboard/`
and into `/starboard/common/`. In order to more explicitly identify the core
Starboard API, multiple files, or parts of them, were moved into the static
library `/starboard/common/`.

### Log synchronization responsibility has been moved behind Starboard.

The application is no longer responsible for synchronizing (e.g. via mutex)
calls to Starboard logging, this is now expected to be done by the Starboard
implementation.  Logging functions, such as `SbLog` or `SbLogRaw`, must now have implementations that are thread-safe because they will be called from multiple
threads without external synchronization.

Additionally, the minimum logging level is no longer set by the application, and
is instead set by grabbing the value as a command-line argument within
Starboard.

### Starboard now provides a EGL and GLES interface as a structure of pointers.

To remove the direct inclusion of EGL and GLES system libraries throughout the
application, we need to move this dependency behind a Starboardized API. This
API can be found in `/starboard/egl.h` and `/starboard/gles.h`.

### Add interface for querying for CPU features, in `/starboard/cpu_features.h`.

The new interface enables the platform to communicate to the application which
CPU features are available, which can enable the application to perform certain
CPU-specific optimizations (e.g. SIMD).

### Deprecated SB_HAS_AUDIO_SPECIFIC_CONFIG_AS_POINTER and
SB_HAS_DRM_KEY_STATUSES.

These macros must always be set to 1 for Starboard version 6 or later. They will
be removed in a future version.  Any implementation that supports Starboard
version 6 or later should be modified to no longer depend on these macros, with
the assumption that their values are always 1.


## Version 10

### Introduce functions to query at runtime for media buffer settings

In particular, the following methods are introduced:
 - `SbMediaGetAudioBufferBudget`
 - `SbMediaGetBufferAlignment`
 - `SbMediaGetBufferAllocationUnit`
 - `SbMediaGetBufferGarbageCollectionDurationThreshold`
 - `SbMediaGetBufferPadding`
 - `SbMediaGetBufferStorageType`
 - `SbMediaGetInitialBufferCapacity`
 - `SbMediaGetMaxBufferCapacity`
 - `SbMediaGetProgressiveBufferBudget`
 - `SbMediaGetVideoBufferBudget`
 - `SbMediaIsBufferPoolAllocateOnDemand`
 - `SbMediaIsBufferUsingMemoryPool`

### Add support for player_filter_tests

Require compiling 'player_filter_tests' test target sources on all
platforms, including `audio_decoder_tests.cc` and `video_decoder_test.cc`. For
this Starboard API version and beyond, `SB_HAS(PLAYER_FILTER_TESTS)` is true.

### Deprecate SbMediaTime for SbTime

SbMediaTime, which is 90khz based, was used to represent timestamps and
duration related to SbPlayer.  As most of the platforms represent video
related times in milliseconds or microseconds, this causes a lot of
otherwise unnecessary conversion.  Now all timestamps and duration related
to SbPlayer are represented by SbTime directly.

### Refine sample writing of SbPlayer

Added two new functions `SbPlayerGetMaximumNumberOfSamplesPerWrite()` and
`SbPlayerWriteSample2()`.  The former allows implementation to specify the
maximum numbers of samples that can be written using the latter at once.
As it takes multiple thread context switches to call `SbPlayerWriteSample2()`
once, it can optimize performance on low end platforms by reducing the
frequency of calling `SbPlayerWriteSample2()`.

### Add support for player error messages

`SbPlayerCreate()` now accepts an additional parameter, `player_error_func`,
that can be called when an error occurs to propagate the error to the
application.

### Add support for system-level closed caption settings

`SbAccessibilityGetCaptionSettings()` and `SbAccessibilitySetCaptionsEnabled()`
along with a number of supporting structure definitions have been added
to `accessibility.h`.  Platforms will need to define SB_HAS_CAPTIONS to 1 in
order to enable the interface.

### Add support for audioless video playback

SbPlayer can be created with only a video track, without any accompanying
audio track.  The SbPlayer implementation must now be able to play back
a sole video track.

### Add support for audio only video playback

SbPlayer can be created with only an audio track, without any accompanying
video track.  The SbPlayer implementation must now be able to play back
a sole audio track.

### Require support for creating multiple SbPlayer instances

Formerly, there were no tests ensuring that calling `SbPlayerCreate()` multiple
times (without calling `SbPlayerDestroy()` in between) would not crash, and
likewise no tests ensuring that calling `SbAudioSinkCreate()` multiple times
(without calling `SbAudioSinkDestroy()` in between) would not crash.
`SbPlayerCreate()` may return `kSbPlayerInvalid` if additional players are not
supported. `SbAudioSinkCreate()` may return `kSbAudionSinkInvalid` if additional
audio sinks are not supported.

### Require stricter error handling on calls some SbPlayer* calls

Specifically, `SbPlayerCreate()`,  `SbPlayerCreateWithUrl()` and
`SbDrmCreateSystem()` must result in invalid return values (e.g.
`kSbPlayerInvalid` or `kSbDrmSystemInvalid` appropriately).

### Refine the DRM API

Specifically, the following changes have been made:
 1. Add a callback to SbDrmCreateSystem that allows a DRM system to
    signal that a DRM session has closed from the Starboard layer.
    Previously, DRM sessions could only be closed from the application
    layer.
 2. Allow calling `SbDrmSessionUpdateRequestFunc` and
    `SbDrmSessionUpdatedFunc` with extra status and optional error message.
 3. Add request type parameter to `SbDrmSessionUpdateRequestFunc` to support
    individualization, license renewal, and license release.

### Remove kSbSystemPathSourceDirectory

Test code looking for its static input files should instead use the `test`
subdirectory in `kSbSystemPathContentDirectory`.

### Remove kSbSystemPropertyPlatformUuid

This property was only ever used in platforms using `in_app_dial`.
The only usage of this system property was replaced with a
self-contained mechanism.

### Deprecate kSbMediaAudioSampleTypeInt16

`SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES` has to be defined to continue
support int16 audio samples after this version.

### Add kSbPlayerErrorCapabilityChanged to SbPlayerError

This allows the SbPlayer implementation to notify the app that its playback
capability has changed during a video playback.  For example, the system may
support vp9 decoding with an external GPU.  When the external GPU is detached,
this error code can signal the app to retry the playback, possibly with h264.

### Add support for SbSystemSupportsResume()

Platforms doesn't need to resume after suspend can return false in
`SbSystemSupportsResume()` to free up the resource used by resume after
suspend.
Please see the comment in `system.h` for more details.

### Support the `kSbKeyMicrophone` keycode

### Add support for new decode target type, `kSbDecodeTargetFormat3Plane10BitYUVI420`

Added `kSbDecodeTargetFormat3Plane10BitYUVI420` to the `SbDecodeTargetFormat`
enum in order to support 10-bit YUV textures.

### Optionally provide absolute timestamp to `SbAudioSinkConsumeFramesFunc()`

`SbAudioSinkConsumeFramesFunc()` can now optionally accept an absolute
timestamp parameter that indicates when the frames are consumed.
Platforms that have the `frames_consumed` updated asynchronously can have
more accurate audio time reporting with this extra parameter.
Please see the comment in `audio_sink.h` for more details.

### Add support for the `SbAtomic8` type and memory access functions

### Introduce `SbMemoryProtect()`

`SbMemoryProtect()` allows memory access permissions to be changed after they
have been mapped with `SbMemoryMap`.

### Add a `timestamp` field to `SbInputData`

This allows platforms to provide more precise information on exactly when
an input event was generated.  Note that if
`SbSystemHasCapability(kSbSystemCapabilitySetsInputTimestamp)` returns false,
the `timestamp` field of `SbInputData` should be ignored by applications.

### Introduces `kSbMemoryMapProtectReserved` flag.

`kSbMemoryMapProtectReserved`, which is identical to `SbMemoryMapFlags(0)`, is
introduced.  When `SbMemoryMap()` is called with `kSbMemoryMapProtectReserved`,
only virtual address space should be reserved for the mapped memory, and not
actual physical memory.

### Add support for multiple versions of ffmpeg

An extra version agnostic ffmpeg dynamic dispatch layer is added in order to
support multiple different versions of ffmpeg as may appear on user systems.

### Make linux-x64x11 builds use GLX (via Angle) instead of EGL by default

While common Cobalt code still targets EGL/GLES2, we now use Angle on
linux-x64x11 builds to translate those calls to GLX/GL calls.  Thus, from
the perspective of the system, linux-x64x11 builds now appear to use
GLX/GL.  This change was made because GLX/GL was generally found to have
better desktop support than EGL/GLES2.  The logic for this is added in the
Starboard [enable_glx_via_angle.gypi](linux/x64x11/enable_glx_via_angle.gypi)
file.

### Split `base.gypi` into `cobalt_configuration.gypi` and `base_configuration.gypi`

Up until now, both Cobalt-specific build configuration options as well as
application-independent Starboard build configuration options were mixed
together within base.gypi.  They have now been split apart, and the application
independent options have been moved into Starboard under
[base_configuration.gypi](build/base_configuration.gypi).  The Cobalt-specific
options have been left in Cobalt, though renamed to `cobalt_configuration.gypi`.

### Moved `tizen` to `contrib/tizen`.

Please see [contrib/README.md](contrib/README.md) for description of
expectations for contents in this directory.
