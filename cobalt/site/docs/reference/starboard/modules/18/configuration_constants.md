Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `configuration_constants.h`

Declares all configuration variables we will need to use at runtime. These
variables describe the current platform in detail to allow cobalt to make
runtime decisions based on per platform configurations.

## Variables

### kSbCanMapExecutableMemory

Whether this platform can map executable memory. This is required for platforms
that want to JIT.

### kSbFileMaxName

The current platform's maximum length of the name of a single directory entry,
not including the absolute path.

### kSbFileMaxPath

The current platform's maximum length of an absolute path.

### kSbFileSepChar

The current platform's file path component separator character. This is the
character that appears after a directory in a file path. For example, the
absolute canonical path of the file "/path/to/a/file.txt" uses '/' as a path
component separator character.

### kSbFileSepString

The string form of SB_FILE_SEP_CHAR.

### kSbMaxSystemPathCacheDirectorySize

The maximum size the cache directory is allowed to use in bytes.

### kSbMaxThreads

Defines the maximum number of simultaneous threads for this platform. Some
platforms require sharing thread handles with other kinds of system handles,
like mutexes, so we want to keep this manageable.

### kSbMediaMaxAudioBitrateInBitsPerSecond

The maximum audio bitrate the platform can decode. The following value equals to
5M bytes per seconds which is more than enough for compressed audio.

### kSbMediaMaxVideoBitrateInBitsPerSecond

The maximum video bitrate the platform can decode. The following value equals to
8M bytes per seconds which is more than enough for compressed video.

### kSbMemoryPageSize

The memory page size, which controls the size of chunks on memory that
allocators deal with, and the alignment of those chunks. This doesn't have to be
the hardware-defined physical page size, but it should be a multiple of it.
