---
layout: doc
title: "Starboard Configuration Reference Guide"
---

## Architecture Configuration

| Properties |
| :--- |
| **`SB_HAS_QUIRK_DOES_NOT_STACK_ALIGN_OVER_16_BYTES`**<br><br>Some platforms will not align variables on the stack with an alignment greater than 16 bytes. Platforms where this is the case should define the following quirk.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_THREAD_AFFINITY_UNSUPPORTED`**<br><br>Some platforms do not have thread affinity support. Platforms where this is the case should define the following quirk.<br><br>By default, this property is undefined. |


## Compiler Configuration

| Properties |
| :--- |
| **`SB_C_FORCE_INLINE`**<br><br>The platform's annotation for forcing a C function to be inlined.<br><br>The default value in the Stub implementation is <br>`__inline__ __attribute__((always_inline))` |
| **`SB_C_INLINE`**<br><br>The platform's annotation for marking a C function as suggested to be inlined.<br><br>The default value in the Stub implementation is `inline` |
| **`SB_C_NOINLINE`**<br><br>The platform's annotation for marking a C function as forcibly not inlined.<br><br>The default value in the Stub implementation is `__attribute__((noinline))` |
| **`SB_EXPORT_PLATFORM`**<br><br>The platform's annotation for marking a symbol as exported outside of the current shared library.<br><br>The default value in the Stub implementation is <br>`__attribute__((visibility("default")))` |
| **`SB_IMPORT_PLATFORM`**<br><br>The platform's annotation for marking a symbol as imported from outside of the current linking unit. |


## Extensions Configuration

| Properties |
| :--- |
| **`SB_HAS_STD_UNORDERED_HASH`**<br><br>Do not use <unordered_map> and <unordered_set> for the hash table types.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_LONG_LONG_HASH`**<br><br>GCC/Clang doesn't define a long long hash function, except for Android and Game consoles.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_STRING_HASH`**<br><br>GCC/Clang doesn't define a string hash function, except for Game Consoles.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_HASH_USING`**<br><br>Desktop Linux needs a using statement for the hash functions.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_HASH_VALUE`**<br><br>Set this to 1 if hash functions for custom types can be defined as a hash_value() function. Otherwise, they need to be placed inside a partially-specified hash struct template with an operator().<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_HASH_WARNING`**<br><br>Set this to 1 if use of hash_map or hash_set causes a deprecation warning (which then breaks the build).<br><br>The default value in the Stub implementation is `1` |
| **`SB_HASH_MAP_INCLUDE`**<br><br>The location to include hash_map on this platform.<br><br>The default value in the Stub implementation is `<ext/hash_map>` |
| **`SB_HASH_NAMESPACE`**<br><br>C++'s hash_map and hash_set are often found in different namespaces depending on the compiler.<br><br>The default value in the Stub implementation is `__gnu_cxx` |
| **`SB_HASH_SET_INCLUDE`**<br><br>The location to include hash_set on this platform.<br><br>The default value in the Stub implementation is `<ext/hash_set>` |


## Filesystem Configuration

| Properties |
| :--- |
| **`SB_HAS_QUIRK_FILESYSTEM_ZERO_FILEINFO_TIME`**<br><br>Some operating systems constantly return zero values for creation, access and modification time for files and directories. When this quirk is defined, we need to ignore corresponded time values in applications as well as take this fact into account in unit tests.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_FILESYSTEM_COARSE_ACCESS_TIME`**<br><br>On some platforms the file system stores access times at a coarser granularity than other times. When this quirk is defined, we assume the access time is of 1 day precision.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_HASH_FILE_NAME`**<br><br>On some platforms the file system cannot access extremely long file names. We do not need this feature on stub.<br><br>By default, this property is undefined. |


## Graphics Configuration

| Properties |
| :--- |
| **`SB_HAS_BLITTER`**<br><br>Specifies whether this platform supports a performant accelerated blitter API. The basic requirement is a scaled, clipped, alpha-blended blit.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_BILINEAR_FILTERING_SUPPORT`**<br><br>Indicates whether or not the given platform supports bilinear filtering. This can be checked to enable/disable renderer tests that verify that this is working properly.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_NV12_TEXTURE_SUPPORT`**<br><br>Indicates whether or not the given platform supports rendering of NV12 textures. These textures typically originate from video decoders.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_VIRTUAL_REALITY`**<br><br>The default value in the Stub implementation is `1` |


## I/O Configuration

| Properties |
| :--- |
| **`SB_HAS_ON_SCREEN_KEYBOARD`**<br><br>Whether the current platform implements the on screen keyboard interface.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_SPEECH_SYNTHESIS`**<br><br>Whether the current platform has speech synthesis.<br><br>The default value in the Stub implementation is `1` |


## Media Configuration

| Properties |
| :--- |
| **`SB_HAS_QUIRK_SEEK_TO_KEYFRAME`**<br><br>After a seek is triggered, the default behavior is to append video frames from the last key frame before the seek time and append audio frames from the seek time because usually all audio frames are key frames.  On platforms that cannot decode video frames without displaying them, this will cause the video being played without audio for several seconds after seeking.  When the following macro is defined, the app will append audio frames start from the timestamp that is before the timestamp of the video key frame being appended.  This quirk has been deprecated in Starboard version 12 or later.  Please see `configuration_public.md` for more details.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_SUPPORT_INT16_AUDIO_SAMPLES`**<br><br>The implementation is allowed to support kSbMediaAudioSampleTypeInt16 only when this macro is defined.<br><br>By default, this property is undefined. |


## Memory Configuration

| Properties |
| :--- |
| **`SB_HAS_MMAP`**<br><br>Whether this platform has and should use an MMAP function to map physical memory to the virtual address space.<br><br>The default value in the Stub implementation is `1` |
| **`SB_CAN_MAP_EXECUTABLE_MEMORY`**<br><br>Whether this platform can map executable memory. Implies SB_HAS_MMAP. This is required for platforms that want to JIT.<br><br>The default value in the Stub implementation is `1` |


## Network Configuration

| Properties |
| :--- |
| **`SB_HAS_IPV6`**<br><br>Specifies whether this platform supports IPV6.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_PIPE`**<br><br>Specifies whether this platform supports pipe.<br><br>The default value in the Stub implementation is `1` |


## System Header Configuration

 Any system headers listed here that are not provided by the platform will be emulated in starboard/types.h.

| Properties |
| :--- |
| **`SB_HAS_SYS_TYPES_H`**<br><br> Whether the current platform provides the standard header sys/types.h.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_SSIZE_T`**<br><br>Whether the current platform provides ssize_t.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UTF32`**<br><br>Type detection for wchar_t.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UTF16`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UNSIGNED`**<br><br>Chrome only defines this for ARMEL. Chrome has an exclusion for iOS here, we should too when we support iOS.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_QUIRK_SOCKET_BSD_HEADERS`**<br><br>This quirk is used to switch the headers included in starboard/shared/linux/socket_get_interface_address.cc for darwin system headers. It may be removed at some point in favor of a different solution.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_NO_GMTIME_R`**<br><br>Some platforms don't support gmtime_r. Platforms where this is the case should define the following quirk.<br><br>By default, this property is undefined. |


## Timing API

| Properties |
| :--- |
| **`SB_HAS_TIME_THREAD_NOW`**<br><br>Whether this platform has an API to retrieve how long the current thread has spent in the executing state.<br><br>The default value in the Stub implementation is `1` |


