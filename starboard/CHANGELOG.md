# Starboard Version Changelog

This document records all changes made to the Starboard interface, up to the
current version, but not including the experimental version.  This file will
be updated each time a new Starboard version is released.  Each section in
this file describes the changes made to the Starboard interface since the
version previous to it.

**NOTE: Starboard versions 3 and below are no longer supported.**

## Experimental Version

A description of all changes currently in the experimental Starboard version
can be found in the comments of the "Experimental Feature Defines" section of
[configuration.h](configuration.h).

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
frequence of calling `SbPlayerWriteSample2()`.

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

## Version 9

### Add string label to `SbMicrophoneInfo`.

This should indicate the friendly name of the microphone type.

### Introduce additional SbSocketError enum values.

Instead of the single generic kSbSocketErrorFailed to indicate socket errors,
the enum kSbSocketErrorConnectionReset has been introduced corresponding to
various dropped TCP connection errors.  This is particularly useful in
identifying socket errors that can be retried.

### Add new keycode kSbKeyInstantReplay

Identical to OCAP's `VK_INSTANT_REPLAY`

## Version 8

### Add `SbPlayerCreateWithUrl()`, `SbPlayerSetDrmSystem()`, `SbPlayerOutputModeSupportedWithUrl()`

For platform media players that rely on using a URL (like an m3u playlist URL)
for playback, add `SbPlayerCreateWithUrl()` which takes in a URL, no video or
audio configs, and no DRM system. Allow the DRM system to be set on a running
SbPlayer exactly once for SbPlayers created with a URL. Also, since URL players
will not expose codec information, use a custom
`SbPlayerOutputModeSupportedWithUrl()` to query player output mode support.

### Add `kSbEventTypeWindowSizeChanged`

An event indicating that an `SbWindow`'s size has changed. The event data is
`SbEventWindowSizeChangedData`, containing a `SbWindow` and `SbWindowSize`.

### Add `SbWindowShowOnScreenKeyboard()`, `SbWindowHideOnScreenKeyboard()`, `SbWindowFocusOnScreenKeyboard()`, `SbWindowBlurOnScreenKeyboard()`, `SbWindowIsOnScreenKeyboardShown()`, `SbWindowSetOnScreenKeyboardKeepFocus()`

These methods show, hide, focus, and blur a native on screen keyboard,
determine if the on screen keyboard is shown, and set whether focus is kept to
the on screen keyboard. The on screen keyboard also handles
`kSbInputEventTypeInput`, which use a new field `input_text` of `SbInputData`.

## Version 7

### `SbDecodeTargetInfoPlane` can specify color plane information

Previously: Planes of type `kSbDecodeTargetFormat2PlaneYUVNV12`
were assumed to have the luma mapped to the alpha channel (`GL_ALPHA`)
and the chroma mapped to blue and alpha (`GL_LUMINANCE_ALPHA`). However,
some graphics systems require that luma is on `GL_RED_EXT` and the chroma
is on `GL_RG_EXT`.

## Version 6

### Named `SbStorageRecord`s

This extends the `SbStorage` interface with the ability to open named
`SbStorageRecord`s. Calling `SbStorageOpenRecord` and `SbStorageDeleteRecord`
with a `NULL` `name` parameter provides access to the old "default" record.

### Introduce pointer (mouse) input support

This extends the `SbInput` interface with some enum values and data members to
allow mouse, wheel, and more generic pointer input.

### Flexible audio specific config

`SbMediaAudioHeader::audio_specific_config` will be a pointer instead of an
array.

### Time Zone API Cleanup

Removes `SbTimeZoneGetDstName()` -- The Daylight Savings Time version of the
time zone.

Changes `SbTimeZoneGetName()` to be more flexible in what it is allowed to
return.

### `SbDecodeTargetNumberOfPlanesForFormat`

Adds the convenience inline function, SbDecodeTargetNumberOfPlanesForFormat() to
`starboard/decode_target.h`.

### Preload Support

Adds the `kSbEventTypePreload` event, and modifies the application state machine
to utilize it.

### Platform Error Cleanup

Removes `SbSystemPlatformErrorType` values specific to user status.

### `SbDecodeTarget` support for the UYVY (i.e. YUV 422) format

Add support for UYVY decode targets (e.g. YUV 422) via the
`kSbDecodeTargetFormat1PlaneUYVY` enum.

### Add More Remote Keys

This adds SbKey codes for:

  * Color keys
  * Closed Caption key
  * Application launch key
  * Channel Up/Down keys
  * Info key
  * Guide key
  * Last/Previous Channel key
  * Media audio track select key

### `kSbEventTypeLowMemory`

Adds a new event type -- `kSbEventTypeLowMemory` -- to allow a platform to
signal that the application may soon be terminated due to low memory
availability.

### Interface change to `SbPlayerWriteSample()`
`const` is added to `sample_buffers` and `sample_buffer_sizes` parameters.

### Support key status change
Add `key_statuses_changed_callback` parameter to `SbDrmCreateSystem()` to
support MediaKeySession::keyStatuses and MediaKeySession::onkeystatuseschange.

### Changes thumbstick direction

Change the meaning of negative values for thumbstick position from bottom
right to upper left.

## Version 5

### Add Speech Recognizer API
Introduce `starboard/speech_recognizer.h`.
This newly-introduced `starboard/speech_recognizer.h` adds the on-device speech
recognizer feature.

### Added new system property to allow platform-specific user agent suffixes
Adds `kSbSystemPropertyUserAgentAuxField` to the `SbSystemPropertyId` enum to
allow platform-specific User-Agent suffix.

### Remove unused enums from `starboard/input.h`
The following unused enum values are removed from `starboard/input.h`:
  * `kSbInputDeviceTypeMicrophone`
  * `kSbInputDeviceTypeSpeechCommand`
  * `kSbInputEventTypeAudio`
  * `kSbInputEventTypeCommand`
  * `kSbInputEventTypeGrab`
  * `kSbInputEventTypeUngrab`

## Version 4

### Decode-to-Texture Player Output Mode
Feature introducing support for decode-to-texture player output mode, and
runtime player output mode selection and detection.
In `starboard/configuration.h`,
  * `SB_IS_PLAYER_PUNCHED_OUT`, `SB_IS_PLAYER_PRODUCING_TEXTURE`, and
    `SB_IS_PLAYER_COMPOSITED` now no longer need to be defined (and should not
    be defined) by platforms.  Instead, these capabilities are detected at
    runtime via `SbPlayerOutputModeSupported()`.

In `starboard/player.h`,
  * The enum `SbPlayerOutputMode` is introduced.
  * `SbPlayerOutputModeSupported()` is introduced to let applications query
    for player output mode support.
  * `SbPlayerCreate()` now takes an additional parameter that specifies the
    desired output mode.
  * The punch out specific function `SbPlayerSetBounds()` must now be
    defined on all platforms, even if they don't support punch out (in which
    case they can implement a stub).
  * The function `SbPlayerGetCompositionHandle()` is removed.
  * The function `SbPlayerGetTextureId()` is replaced by the new
    `SbPlayerGetCurrentFrame()`, which returns a `SbDecodeTarget`.

In `starboard/decode_target.h`,
  * All get methods (`SbDecodeTargetGetPlane()` and `SbDecodeTargetGetFormat()`,
    `SbDecodeTargetIsOpaque()`) are now replaced with `SbDecodeTargetGetInfo()`.
  * The `SbDecodeTargetInfo` structure is introduced and is the return value
    type of `SbDecodeTargetGetInfo()`.
  * `SbDecdodeTargetCreate()` is now responsible for creating all its internal
    planes, and so its `planes` parameter is replaced by `width` and
    `height` parameters.
  * The GLES2 version of `SbDecdodeTargetCreate()` has its EGL types
    (`EGLDisplay`, `EGLContext`) replaced by `void*` types, so that
    `decode_target.h` can avoid #including EGL/GLES2 headers.
  * `SbDecodeTargetDestroy()` is renamed to `SbDecodeTargetRelease()`.

In `starboard/player.h`, `starboard/image.h` and `starboard/decode_target.h`,
  * Replace `SbDecodeTargetProvider` with
    `SbDecodeTargetGraphicsContextProvider`.
  * Instead of restricting Starboard implementations to only be able to run
    `SbDecodeTarget` creation and destruction code on the application's
    renderer's thread with the application's renderer's `EGLContext` current,
    Starboard implementations can now run arbitrary code on the application's
    renderer's thread with its `EGLContext` current.
  * Remove `SbDecodeTargetCreate()`, `SbDecodeTarget` creation is now an
    implementation detail to be dealt with in other Starboard API functions
    that create `SbDecodeTargets`, like `SbImageDecode()` or `SbPlayerCreate()`.

### Playback Rate
Support for setting the playback rate on an SbPlayer.  This allows for control
of the playback speed of video at runtime.

### Floating Point Input Vector
Change `input.h`'s `SbInputVector` structure to contain float members instead of
ints.

### Delete SbUserApplicationTokenResults
Deleted the vestigal struct `SbUserApplicationTokenResults` from `user.h`.

### Storage Options for Encoded Audio/Video Data
Enables the SbPlayer implementation to provide instructions to its user on
how to store audio/video data.  Encoded audio/video data is cached once being
demuxed and may occupy a significant amount of memory.  Enabling this feature
allows the SbPlayer implementation to have better control on where encoded
audio/video data is stored.

### Unified implementation of `SbMediaCanPlayMimeAndKeySystem()`
Use a unified implementation of `SbMediaCanPlayMimeAndKeySystem()` based on
`SbMediaIsSupported()`, `SbMediaIsAudioSupported()`, and
`SbMediaIsVideoSupported()`.

### Introduce `ticket` parameter to `SbDrmGenerateSessionUpdateRequest()`
Introduce `ticket` parameter to `SbDrmGenerateSessionUpdateRequest()`
and `SbDrmSessionUpdateRequestFunc` to allow distinguishing between callbacks
from multiple concurrent calls.

### Introduce `SbSocketGetInterfaceAddress()`
`SbSocketGetInterfaceAddress()` is introduced to let applications find out
which source IP address and the associated netmask will be used to connect to
the destination. This is very important for multi-homed devices, and for
certain conditions in IPv6.

### Introduce `starboard/cryptography.h`
In newly-introduced `starboard/cryptography.h`,
  * Optional support for accelerated cryptography, which can, in
    particular, be used for accelerating SSL.

### Introduce z-index parameter to `SbPlayerSetBounds()`
Allow `SbPlayerSetBounds` to use an extra parameter to indicate the z-index of
the video so multiple overlapping videos can be rendered.

### Media source buffer settings removed from `configuration.h`
Media source buffer settings in Starboard.

### Introduce `starboard/accessibility.h`
In particular, the functions `SbAccessibilityGetDisplaySettings()` and
`SbAccessibilityGetTextToSpeechSettings()` have now been introduced.

Additionally, a new Starboard event, `kSbEventTypeAccessiblitySettingsChanged`,
has been defined in `starboard/event.h`.

### HDR decode support
In `starboard/media.h`, `SbMediaColorMetadata` is now defined and it contains
HDR metadata. The field `SbMediaColorMetadata color_metadata` is now added to
`SbMediaVideoSampleInfo`.

### Add `kSbSystemDeviceTypeAndroidTV` to `starboard/system.h`
A new device type, `kSbSystemDeviceTypeAndroidTV`, is added to
starboard/system.h.

### Deprecate `SbSpeechSynthesisSetLanguage()`
SbSpeechSynthesisSetLanguage() has been deprecated.

### Request State Change Support
Added `SbSystemRequestPause()`, `SbSystemRequestUnpause()`,
`SbSystemRequestSuspend()`.

`SbSystemRequestSuspend()` in particular can be hooked into a platform's "hide"
or "minimize" window functionality.

### Font Directory Path Support
Added `kSbSystemPathFontDirectory` and `kSbSystemPathFontConfigurationDirectory`
which can be optionally specified for platforms that want to provide system
fonts to Starboard applications. The font and font configuration formats
supported are application-specific.

### Add `SB_NORETURN` to `starboard/configuration.h`.
Added attribute macro `SB_NORETURN` to allow functions to be marked as noreturn.

### Mark `SbSystemBreakIntoDebugger` `SB_NORETURN`.
Add `SB_NORETURN` to declaration of `SbSystemBreakIntoDebugger`, to allow it to
be used in a manner similar to `abort`.

### Introduce `SbAudioSinkGetMinBufferSizeInFrames()`

Introduce `SbAudioSinkGetMinBufferSizeInFrames()` to `starboard/audio_sink.h`
which communicates to the platform how many audio frames are required to ensure
that audio sink can keep playing without underflow.
