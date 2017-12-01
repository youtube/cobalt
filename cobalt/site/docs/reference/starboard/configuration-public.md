---
layout: doc
title: "Starboard Configuration Reference Guide"
---

## Architecture Configuration

| Properties |
| :--- |
| **`SB_IS_BIG_ENDIAN`**<br><br>Whether the current platform is big endian. SB_IS_LITTLE_ENDIAN will be automatically set based on this.<br><br>The default value in the Stub implementation is `0` |
| **`SB_IS_ARCH_ARM`**<br><br>Whether the current platform is an ARM architecture.<br><br>The default value in the Stub implementation is `0` |
| **`SB_IS_ARCH_MIPS`**<br><br>Whether the current platform is a MIPS architecture.<br><br>The default value in the Stub implementation is `0` |
| **`SB_IS_ARCH_PPC`**<br><br>Whether the current platform is a PPC architecture.<br><br>The default value in the Stub implementation is `0` |
| **`SB_IS_ARCH_X86`**<br><br>Whether the current platform is an x86 architecture.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_32_BIT`**<br><br>Assume a 64-bit architecture.<br><br>The default value in the Stub implementation is `0` |
| **`SB_IS_64_BIT`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_32_BIT_POINTERS`**<br><br>Whether the current platform's pointers are 32-bit. Whether the current platform's longs are 32-bit.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_32_BIT_LONG`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_32_BIT_POINTERS`**<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_32_BIT_LONG`**<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_64_BIT_POINTERS`**<br><br>Whether the current platform's pointers are 64-bit. Whether the current platform's longs are 64-bit.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_64_BIT_LONG`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_64_BIT_POINTERS`**<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_64_BIT_LONG`**<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_MANY_CORES`**<br><br> Whether the current platform is expected to have many cores (> 6), or a wildly varying number of cores.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_1_CORE`**<br><br>Whether the current platform is expected to have exactly 1 core.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_2_CORES`**<br><br>Whether the current platform is expected to have exactly 2 cores.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_4_CORES`**<br><br>Whether the current platform is expected to have exactly 4 cores.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_6_CORES`**<br><br>Whether the current platform is expected to have exactly 6 cores.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_THREAD_PRIORITY_SUPPORT`**<br><br>Whether the current platform supports thread priorities.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_CROSS_CORE_SCHEDULER`**<br><br>Whether the current platform's thread scheduler will automatically balance threads between cores, as opposed to systems where threads will only ever run on the specifically pinned core.<br><br>The default value in the Stub implementation is `1` |
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
| **`SB_HAS_QUIRK_COMPILER_SAYS_GNUC_BUT_ISNT`**<br><br>On some platforms the &#95;&#95;GNUC&#95;&#95; is defined even though parts of the functionality are missing. Setting this to non-zero allows disabling missing functionality encountered.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_HASFEATURE_NOT_DEFINED_BUT_IT_IS`**<br><br>On some compilers, the frontend has a quirk such that #ifdef cannot correctly detect &#95;&#95;has_feature is defined, and an example error you get is:<br><br>By default, this property is undefined. |


## Decoder-only Params

| Properties |
| :--- |
| **`SB_MEDIA_BUFFER_ALIGNMENT`**<br><br>Specifies how media buffers must be aligned on this platform as some decoders may have special requirement on the alignment of buffers being decoded.<br><br>The default value in the Stub implementation is `128U` |
| **`SB_MEDIA_VIDEO_FRAME_ALIGNMENT`**<br><br>Specifies how video frame buffers must be aligned on this platform.<br><br>The default value in the Stub implementation is `256U` |
| **`SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES`**<br><br>The encoded video frames are compressed in different ways, so their decoding time can vary a lot.  Occasionally a single frame can take longer time to decode than the average time per frame.  The player has to cache some frames to account for such inconsistency.  The number of frames being cached are controlled by SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES and SB_MEDIA_MAXIMUM_VIDEO_FRAMES.  Specify the number of video frames to be cached before the playback starts. Note that setting this value too large may increase the playback start delay.<br><br>The default value in the Stub implementation is `4` |
| **`SB_MEDIA_MAXIMUM_VIDEO_FRAMES`**<br><br>Specify the number of video frames to be cached during playback.  A large value leads to more stable fps but also causes the app to use more memory.<br><br>The default value in the Stub implementation is `12` |


## Extensions Configuration

| Properties |
| :--- |
| **`SB_HAS_LONG_LONG_HASH`**<br><br>GCC/Clang doesn't define a long long hash function, except for Android and Game consoles.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_STRING_HASH`**<br><br>GCC/Clang doesn't define a string hash function, except for Game Consoles.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_HASH_USING`**<br><br>Desktop Linux needs a using statement for the hash functions.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_HASH_VALUE`**<br><br>Set this to 1 if hash functions for custom types can be defined as a hash_value() function. Otherwise, they need to be placed inside a partially-specified hash struct template with an operator().<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_HASH_WARNING`**<br><br>Set this to 1 if use of hash_map or hash_set causes a deprecation warning (which then breaks the build).<br><br>The default value in the Stub implementation is `1` |
| **`SB_HASH_MAP_INCLUDE`**<br><br>The location to include hash_map on this platform.<br><br>The default value in the Stub implementation is `<ext/hash_map>` |
| **`SB_HASH_NAMESPACE`**<br><br>C++'s hash_map and hash_set are often found in different namespaces depending on the compiler.<br><br>The default value in the Stub implementation is `__gnu_cxx` |
| **`SB_HASH_SET_INCLUDE`**<br><br>The location to include hash_set on this platform.<br><br>The default value in the Stub implementation is `<ext/hash_set>` |
| **`SB_VA_COPY(dest, source)`**<br><br>Define this to how this platform copies varargs blocks.<br><br>The default value in the Stub implementation is `va_copy(dest, source)` |


## Filesystem Configuration

| Properties |
| :--- |
| **`SB_FILE_MAX_NAME`**<br><br>The current platform's maximum length of the name of a single directory entry, not including the absolute path.<br><br>The default value in the Stub implementation is `64` |
| **`SB_FILE_MAX_PATH`**<br><br>The current platform's maximum length of an absolute path.<br><br>The default value in the Stub implementation is `4096` |
| **`SB_FILE_MAX_OPEN`**<br><br>The current platform's maximum number of files that can be opened at the same time by one process.<br><br>The default value in the Stub implementation is `64` |
| **`SB_FILE_SEP_CHAR`**<br><br>The current platform's file path component separator character. This is the character that appears after a directory in a file path. For example, the absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path component separator character.<br><br>The default value in the Stub implementation is `'/'` |
| **`SB_FILE_ALT_SEP_CHAR`**<br><br>The current platform's alternate file path component separator character. This is like SB_FILE_SEP_CHAR, except if your platform supports an alternate character, then you can place that here. For example, on windows machines, the primary separator character is probably '\', but the alternate is '/'.<br><br>The default value in the Stub implementation is `'/'` |
| **`SB_PATH_SEP_CHAR`**<br><br>The current platform's search path component separator character. When specifying an ordered list of absolute paths of directories to search for a given reason, this is the character that appears between entries. For example, the search path of "/etc/search/first:/etc/search/second" uses ':' as a search path component separator character.<br><br>The default value in the Stub implementation is `':'` |
| **`SB_FILE_SEP_STRING`**<br><br>The string form of SB_FILE_SEP_CHAR.<br><br>The default value in the Stub implementation is `"/"` |
| **`SB_FILE_ALT_SEP_STRING`**<br><br>The string form of SB_FILE_ALT_SEP_CHAR.<br><br>The default value in the Stub implementation is `"/"` |
| **`SB_PATH_SEP_STRING`**<br><br>The string form of SB_PATH_SEP_CHAR.<br><br>The default value in the Stub implementation is `":"` |
| **`SB_HAS_QUIRK_FILESYSTEM_COARSE_ACCESS_TIME`**<br><br>On some platforms the file system stores access times at a coarser granularity than other times. When this quirk is defined, we assume the access time is of 1 day precision.<br><br>By default, this property is undefined. |


## Graphics Configuration

| Properties |
| :--- |
| **`SB_HAS_BLITTER`**<br><br>Specifies whether this platform supports a performant accelerated blitter API. The basic requirement is a scaled, clipped, alpha-blended blit.<br><br>The default value in the Stub implementation is `0` |
| **`SB_PREFERRED_RGBA_BYTE_ORDER`**<br><br>Specifies the preferred byte order of color channels in a pixel. Refer to starboard/configuration.h for the possible values. EGL/GLES platforms should generally prefer a byte order of RGBA, regardless of endianness.<br><br>The default value in the Stub implementation is <br>`SB_PREFERRED_RGBA_BYTE_ORDER_RGBA` |
| **`SB_HAS_BILINEAR_FILTERING_SUPPORT`**<br><br>Indicates whether or not the given platform supports bilinear filtering. This can be checked to enable/disable renderer tests that verify that this is working properly.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_NV12_TEXTURE_SUPPORT`**<br><br>Indicates whether or not the given platform supports rendering of NV12 textures. These textures typically originate from video decoders.<br><br>The default value in the Stub implementation is `0` |
| **`SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER`**<br><br>Whether the current platform should frequently flip its display buffer.  If this is not required (i.e. SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER is set to 0), then optimizations are enabled so the display buffer is not flipped if the scene hasn't changed.<br><br>The default value in the Stub implementation is `0` |
| **`SB_HAS_VIRTUAL_REALITY`**<br><br>The default value in the Stub implementation is `1` |


## Media Configuration

| Properties |
| :--- |
| **`SB_HAS_QUIRK_SEEK_TO_KEYFRAME`**<br><br>After a seek is triggerred, the default behavior is to append video frames from the last key frame before the seek time and append audio frames from the seek time because usually all audio frames are key frames.  On platforms that cannot decode video frames without displaying them, this will cause the video being played without audio for several seconds after seeking.  When the following macro is defined, the app will append audio frames start from the timestamp that is before the timestamp of the video key frame being appended.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_NO_FFS`**<br><br>dlmalloc will use the ffs intrinsic if available.  Platforms on which this is not available should define the following quirk.<br><br>By default, this property is undefined. |
| **`SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND`**<br><br>The maximum audio bitrate the platform can decode.  The following value equals to 5M bytes per seconds which is more than enough for compressed audio.<br><br>The default value in the Stub implementation is `(40 * 1024 * 1024)` |
| **`SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND`**<br><br>The maximum video bitrate the platform can decode.  The following value equals to 25M bytes per seconds which is more than enough for compressed video.<br><br>The default value in the Stub implementation is `(200 * 1024 * 1024)` |
| **`SB_HAS_MEDIA_WEBM_VP9_SUPPORT`**<br><br>Specifies whether this platform has webm/vp9 support.  This should be set to non-zero on platforms with webm/vp9 support.<br><br>The default value in the Stub implementation is `0` |
| **`SB_MEDIA_THREAD_STACK_SIZE`**<br><br>Specifies the stack size for threads created inside media stack.  Set to 0 to use the default thread stack size.  Set to non-zero to explicitly set the stack size for media stack threads.<br><br>The default value in the Stub implementation is `0U` |


## Memory Configuration

| Properties |
| :--- |
| **`SB_MEMORY_PAGE_SIZE`**<br><br>The memory page size, which controls the size of chunks on memory that allocators deal with, and the alignment of those chunks. This doesn't have to be the hardware-defined physical page size, but it should be a multiple of it.<br><br>The default value in the Stub implementation is `4096` |
| **`SB_HAS_MMAP`**<br><br>Whether this platform has and should use an MMAP function to map physical memory to the virtual address space.<br><br>The default value in the Stub implementation is `1` |
| **`SB_CAN_MAP_EXECUTABLE_MEMORY`**<br><br>Whether this platform can map executable memory. Implies SB_HAS_MMAP. This is required for platforms that want to JIT.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_VIRTUAL_REGIONS`**<br><br>Whether this platform has and should use an growable heap (e.g. with sbrk()) to map physical memory to the virtual address space.<br><br>The default value in the Stub implementation is `0` |
| **`SB_NETWORK_IO_BUFFER_ALIGNMENT`**<br><br>Specifies the alignment for IO Buffers, in bytes. Some low-level network APIs may require buffers to have a specific alignment, and this is the place to specify that.<br><br>The default value in the Stub implementation is `16` |
| **`SB_MALLOC_ALIGNMENT`**<br><br>Determines the alignment that allocations should have on this platform.<br><br>The default value in the Stub implementation is `((size_t)16U)` |
| **`SB_DEFAULT_MMAP_THRESHOLD`**<br><br>Determines the threshhold of allocation size that should be done with mmap (if available), rather than allocated within the core heap.<br><br>The default value in the Stub implementation is `((size_t)(256 * 1024U))` |
| **`SB_MEMORY_LOG_PATH`**<br><br>Defines the path where memory debugging logs should be written to.<br><br>The default value in the Stub implementation is `"/tmp/starboard"` |


## Network Configuration

| Properties |
| :--- |
| **`SB_HAS_IPV6`**<br><br>Specifies whether this platform supports IPV6.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_PIPE`**<br><br>Specifies whether this platform supports pipe.<br><br>The default value in the Stub implementation is `1` |


## System Header Configuration

 Any system headers listed here that are not provided by the platform will be emulated in starboard/types.h.

| Properties |
| :--- |
| **`SB_HAS_STDARG_H`**<br><br> Whether the current platform provides the standard header stdarg.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_STDBOOL_H`**<br><br>Whether the current platform provides the standard header stdbool.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_STDDEF_H`**<br><br>Whether the current platform provides the standard header stddef.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_STDINT_H`**<br><br>Whether the current platform provides the standard header stdint.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_INTTYPES_H`**<br><br>Whether the current platform provides the standard header inttypes.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_WCHAR_H`**<br><br>Whether the current platform provides the standard header wchar.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_LIMITS_H`**<br><br>Whether the current platform provides the standard header limits.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_FLOAT_H`**<br><br>Whether the current platform provides the standard header float.h.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_SSIZE_T`**<br><br>Whether the current platform provides ssize_t.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_MICROPHONE`**<br><br>Whether the current platform has microphone supported.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_SPEECH_RECOGNIZER`**<br><br>Whether the current platform has speech recognizer.<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_SPEECH_SYNTHESIS`**<br><br>Whether the current platform has speech synthesis.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UTF32`**<br><br>Type detection for wchar_t.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UTF16`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_UNSIGNED`**<br><br>Chrome only defines these two if ARMEL or MIPSEL are defined. Chrome has an exclusion for iOS here, we should too when we support iOS.<br><br>The default value in the Stub implementation is `1` |
| **`SB_IS_WCHAR_T_SIGNED`**<br><br>The default value in the Stub implementation is `1` |
| **`SB_HAS_QUIRK_MEMSET_IN_SYSTEM_HEADERS`**<br><br>Some platforms have memset predefined in system headers. Platforms where this is the case should define the following quirk.<br><br>By default, this property is undefined. |
| **`SB_HAS_QUIRK_SOCKET_BSD_HEADERS`**<br><br>This quirk is used to switch the headers included in starboard/shared/linux/socket_get_interface_address.cc for darwin system headers. It may be removed at some point in favor of a different solution.<br><br>By default, this property is undefined. |


## Thread Configuration

| Properties |
| :--- |
| **`SB_MAX_THREADS`**<br><br>Defines the maximum number of simultaneous threads for this platform. Some platforms require sharing thread handles with other kinds of system handles, like mutexes, so we want to keep this managable.<br><br>The default value in the Stub implementation is `90` |
| **`SB_MAX_THREAD_LOCAL_KEYS`**<br><br>The maximum number of thread local storage keys supported by this platform.<br><br>The default value in the Stub implementation is `512` |
| **`SB_MAX_THREAD_NAME_LENGTH`**<br><br>The maximum length of the name for a thread, including the NULL-terminator.<br><br>The default value in the Stub implementation is `16;` |


## Timing API

| Properties |
| :--- |
| **`SB_HAS_TIME_THREAD_NOW`**<br><br>Whether this platform has an API to retrieve how long the current thread has spent in the executing state.<br><br>The default value in the Stub implementation is `1` |


## Tuneable Parameters

| Properties |
| :--- |
| **`SB_NETWORK_RECEIVE_BUFFER_SIZE`**<br><br>Specifies the network receive buffer size in bytes, set via SbSocketSetReceiveBufferSize().  Setting this to 0 indicates that SbSocketSetReceiveBufferSize() should not be called. Use this for OSs (such as Linux) where receive buffer auto-tuning is better.  On some platforms, this may affect max TCP window size which may dramatically affect throughput in the presence of latency.  If your platform does not have a good TCP auto-tuning mechanism, a setting of (128 * 1024) here is recommended.<br><br>The default value in the Stub implementation is `(0)` |


## User Configuration

| Properties |
| :--- |
| **`SB_USER_MAX_SIGNED_IN`**<br><br>The maximum number of users that can be signed in at the same time.<br><br>The default value in the Stub implementation is `1` |


