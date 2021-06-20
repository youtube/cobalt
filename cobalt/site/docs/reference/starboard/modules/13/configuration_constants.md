---
layout: doc
title: "Starboard Module Reference: configuration_constants.h"
---

Declares all configuration variables we will need to use at runtime. These
variables describe the current platform in detail to allow cobalt to make
runtime decisions based on per platform configurations.

## Variables ##

### kSbDefaultMmapThreshold ###

Determines the threshold of allocation size that should be done with mmap (if
available), rather than allocated within the core heap.

### kSbFileAltSepChar ###

The current platform's alternate file path component separator character. This
is like SB_FILE_SEP_CHAR, except if your platform supports an alternate
character, then you can place that here. For example, on windows machines, the
primary separator character is probably '\', but the alternate is '/'.

### kSbFileAltSepString ###

The string form of SB_FILE_ALT_SEP_CHAR.

### kSbFileMaxName ###

The current platform's maximum length of the name of a single directory entry,
not including the absolute path.

### kSbFileMaxOpen ###

The current platform's maximum number of files that can be opened at the same
time by one process.

### kSbFileMaxPath ###

The current platform's maximum length of an absolute path.

### kSbFileSepChar ###

The current platform's file path component separator character. This is the
character that appears after a directory in a file path. For example, the
absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path
component separator character.

### kSbFileSepString ###

The string form of SB_FILE_SEP_CHAR.

### kSbHasAc3Audio ###

Allow ac3 and ec3 support

### kSbHasMediaWebmVp9Support ###

Specifies whether this platform has webm/vp9 support. This should be set to non-
zero on platforms with webm/vp9 support.

### kSbHasThreadPrioritySupport ###

Whether the current platform supports thread priorities.

### kSbMallocAlignment ###

Determines the alignment that allocations should have on this platform.

### kSbMaxThreadLocalKeys ###

The maximum number of thread local storage keys supported by this platform. This
comes from _POSIX_THREAD_KEYS_MAX. The value of PTHREAD_KEYS_MAX is higher, but
unit tests show that the implementation doesn't support nearly as many keys.

### kSbMaxThreadNameLength ###

The maximum length of the name for a thread, including the NULL-terminator.

### kSbMaxThreads ###

Defines the maximum number of simultaneous threads for this platform. Some
platforms require sharing thread handles with other kinds of system handles,
like mutexes, so we want to keep this manageable.

### kSbMediaMaxAudioBitrateInBitsPerSecond ###

The maximum audio bitrate the platform can decode. The following value equals to
5M bytes per seconds which is more than enough for compressed audio.

### kSbMediaMaxVideoBitrateInBitsPerSecond ###

The maximum video bitrate the platform can decode. The following value equals to
8M bytes per seconds which is more than enough for compressed video.

### kSbMediaVideoFrameAlignment ###

Specifies how video frame buffers must be aligned on this platform.

### kSbMemoryLogPath ###

Defines the path where memory debugging logs should be written to.

### kSbMemoryPageSize ###

The memory page size, which controls the size of chunks on memory that
allocators deal with, and the alignment of those chunks. This doesn't have to be
the hardware-defined physical page size, but it should be a multiple of it.

### kSbNetworkReceiveBufferSize ###

Specifies the network receive buffer size in bytes, set via
SbSocketSetReceiveBufferSize().

Setting this to 0 indicates that SbSocketSetReceiveBufferSize() should not be
called. Use this for OSs (such as Linux) where receive buffer auto-tuning is
better.

On some platforms, this may affect max TCP window size which may dramatically
affect throughput in the presence of latency.

If your platform does not have a good TCP auto-tuning mechanism, a setting of
(128 * 1024) here is recommended.

### kSbPathSepChar ###

The current platform's search path component separator character. When
specifying an ordered list of absolute paths of directories to search for a
given reason, this is the character that appears between entries. For example,
the search path of "/etc/search/first:/etc/search/second" uses ':' as a search
path component separator character.

### kSbPathSepString ###

The string form of SB_PATH_SEP_CHAR.

### kSbPreferredRgbaByteOrder ###

Specifies the preferred byte order of color channels in a pixel. Refer to
starboard/configuration.h for the possible values. EGL/GLES platforms should
generally prefer a byte order of RGBA, regardless of endianness.

### kSbUserMaxSignedIn ###

The maximum number of users that can be signed in at the same time.
