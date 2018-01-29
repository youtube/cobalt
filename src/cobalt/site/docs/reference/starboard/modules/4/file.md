---
layout: doc
title: "Starboard Module Reference: file.h"
---

Defines file system input/output functions.

## Macros ##

### kSbFileInvalid ###

Well-defined value for an invalid file handle.

## Enums ##

### SbFileError ###

kSbFileErrorAccessDenied is returned when a call fails because of a filesystem
restriction. kSbFileErrorSecurity is returned when a security policy doesn't
allow the operation to be executed.

#### Values ####

*   `kSbFileOk`
*   `kSbFileErrorFailed`
*   `kSbFileErrorInUse`
*   `kSbFileErrorExists`
*   `kSbFileErrorNotFound`
*   `kSbFileErrorAccessDenied`
*   `kSbFileErrorTooManyOpened`
*   `kSbFileErrorNoMemory`
*   `kSbFileErrorNoSpace`
*   `kSbFileErrorNotADirectory`
*   `kSbFileErrorInvalidOperation`
*   `kSbFileErrorSecurity`
*   `kSbFileErrorAbort`
*   `kSbFileErrorNotAFile`
*   `kSbFileErrorNotEmpty`
*   `kSbFileErrorInvalidUrl`
*   `kSbFileErrorMax`

    Put new entries here and increment kSbFileErrorMax.

### SbFileFlags ###

Flags that define how a file is used in the application. These flags should be
or'd together when passed to SbFileOpen to open or create a file.

The following five flags are mutually exclusive. You must specify exactly one of
them:

*   `kSbFileOpenAlways`

*   `kSbFileOpenOnly`

*   `kSbFileOpenTruncated`

*   `kSbFileCreateAlways`

*   `kSbFileCreateOnly`

In addition, one or more of the following flags must be specified:

*   `kSbFileRead`

*   `kSbFileWrite`

The `kSbFileAsync` flag is optional.

#### Values ####

*   `kSbFileOpenOnly`
*   `kSbFileCreateOnly`
*   `kSbFileOpenAlways`

    does not already exist.
*   `kSbFileCreateAlways`

    path or creates a new file at that path.
*   `kSbFileOpenTruncated`

    or overwrites an existing file at that path.
*   `kSbFileRead`

    only if it exists.
*   `kSbFileWrite`
*   `kSbFileAsync`

### SbFileWhence ###

This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.

#### Values ####

*   `kSbFileFromBegin`
*   `kSbFileFromCurrent`
*   `kSbFileFromEnd`

## Typedefs ##

### SbFile ###

A handle to an open file.

#### Definition ####

```
typedef SbFilePrivate* SbFile
```

## Structs ##

### SbFileInfo ###

Used to hold information about a file.

#### Members ####

*   `int64_t size`

    The size of the file in bytes. Undefined when is_directory is true.
*   `bool is_directory`

    Whether the file corresponds to a directory.
*   `bool is_symbolic_link`

    Whether the file corresponds to a symbolic link.
*   `SbTime last_modified`

    The last modified time of a file.
*   `SbTime last_accessed`

    The last accessed time of a file.
*   `SbTime creation_time`

    The creation time of a file.

## Functions ##

### SbFileCanOpen ###

Indicates whether SbFileOpen() with the given `flags` is allowed for `path`.

`path`: The absolute path to be checked. `flags`: The flags that are being
evaluated for the given `path`.

#### Declaration ####

```
bool SbFileCanOpen(const char *path, int flags)
```

### SbFileClose ###

Closes `file`. The return value indicates whether the file was closed
successfully.

`file`: The absolute path of the file to be closed.

#### Declaration ####

```
bool SbFileClose(SbFile file)
```

### SbFileDelete ###

Deletes the regular file, symlink, or empty directory at `path`. This function
is used primarily to clean up after unit tests. On some platforms, this function
fails if the file in question is being held open.

`path`: The absolute path fo the file, symlink, or directory to be deleted.

#### Declaration ####

```
bool SbFileDelete(const char *path)
```

### SbFileExists ###

Indicates whether a file or directory exists at `path`.

`path`: The absolute path of the file or directory being checked.

#### Declaration ####

```
bool SbFileExists(const char *path)
```

### SbFileFlush ###

Flushes the write buffer to `file`. Data written via SbFileWrite is not
necessarily committed until the SbFile is flushed or closed.

`file`: The SbFile to which the write buffer is flushed.

#### Declaration ####

```
bool SbFileFlush(SbFile file)
```

### SbFileGetInfo ###

Retrieves information about `file`. The return value indicates whether the file
information was retrieved successfully.

`file`: The SbFile for which information is retrieved. `out_info`: The variable
into which the retrieved data is placed. This variable is not touched if the
operation is not successful.

#### Declaration ####

```
bool SbFileGetInfo(SbFile file, SbFileInfo *out_info)
```

### SbFileGetPathInfo ###

Retrieves information about the file at `path`. The return value indicates
whether the file information was retrieved successfully.

`file`: The absolute path of the file for which information is retrieved.
`out_info`: The variable into which the retrieved data is placed. This variable
is not touched if the operation is not successful.

#### Declaration ####

```
bool SbFileGetPathInfo(const char *path, SbFileInfo *out_info)
```

### SbFileIsValid ###

Returns whether the given file handle is valid.

#### Declaration ####

```
static bool SbFileIsValid(SbFile file)
```

### SbFileModeStringToFlags ###

Converts an ISO `fopen()` mode string into flags that can be equivalently passed
into SbFileOpen().

`mode`: The mode string to be converted into flags.

#### Declaration ####

```
int SbFileModeStringToFlags(const char *mode)
```

### SbFileOpen ###

Opens the file at `path`, which must be absolute, creating it if specified by
`flags`. The read/write position is at the beginning of the file.

Note that this function only guarantees the correct behavior when `path` points
to a file. The behavior is undefined if `path` points to a directory.

`path`: The absolute path of the file to be opened. `flags`: `SbFileFlags` that
determine how the file is used in the application. Among other things, `flags`
can indicate whether the application should create `path` if `path` does not
already exist. `out_created`: Starboard sets this value to `true` if a new file
is created or if an old file is truncated to zero length to simulate a new file,
which can happen if the `kSbFileCreateAlways` flag is set. Otherwise, Starboard
sets this value to `false`. `out_error`: If `path` cannot be created, this is
set to `kSbFileInvalid`. Otherwise, it is `NULL`.

#### Declaration ####

```
SbFile SbFileOpen(const char *path, int flags, bool *out_created, SbFileError *out_error)
```

### SbFileRead ###

Reads `size` bytes (or until EOF is reached) from `file` into `data`, starting
at the file's current position.

The return value specifies the number of bytes read or `-1` if there was an
error. Note that this function does NOT make a best effort to read all data on
all platforms; rather, it just reads however many bytes are quickly available.
However, this function can be run in a loop to make it a best-effort read.

`file`: The SbFile from which to read data. `data`: The variable to which data
is read. `size`: The amount of data (in bytes) to read.

#### Declaration ####

```
int SbFileRead(SbFile file, char *data, int size)
```

### SbFileReadAll ###

Reads `size` bytes (or until EOF is reached) from `file` into `data`, starting
from the beginning of the file.

The return value specifies the number of bytes read or `-1` if there was an
error. Note that, unlike `SbFileRead`, this function does make a best effort to
read all data on all platforms. As such, it is not intended for stream-oriented
files but instead for cases when the normal expectation is that `size` bytes can
be read unless there is an error.

`file`: The SbFile from which to read data. `data`: The variable to which data
is read. `size`: The amount of data (in bytes) to read.

#### Declaration ####

```
static int SbFileReadAll(SbFile file, char *data, int size)
```

### SbFileSeek ###

Changes the current read/write position in `file`. The return value identifies
the resultant current read/write position in the file (relative to the start) or
`-1` in case of an error. This function might not support seeking past the end
of the file on some platforms.

`file`: The SbFile in which the read/write position will be changed. `whence`:
The starting read/write position. The position is modified relative to this
value. `offset`: The amount that the read/write position is changed, relative to
`whence`.

#### Declaration ####

```
int64_t SbFileSeek(SbFile file, SbFileWhence whence, int64_t offset)
```

### SbFileTruncate ###

Truncates the given `file` to the given `length`. The return value indicates
whether the file was truncated successfully.

`file`: The file to be truncated. `length`: The expected length of the file
after it is truncated. If `length` is greater than the current size of the file,
then the file is extended with zeros. If `length` is negative, then the function
is a no-op and returns `false`.

#### Declaration ####

```
bool SbFileTruncate(SbFile file, int64_t length)
```

### SbFileWrite ###

Writes the given buffer into `file` at the file's current position, overwriting
any data that was previously there.

The return value identifies the number of bytes written, or `-1` on error. Note
that this function does NOT make a best effort to write all data; rather, it
writes however many bytes can be written quickly. Generally, this means that it
writes however many bytes as possible without blocking on IO. Run this function
in a loop to ensure that all data is written.

`file`: The SbFile to which data will be written. `data`: The data to be
written. `size`: The amount of data (in bytes) to write.

#### Declaration ####

```
int SbFileWrite(SbFile file, const char *data, int size)
```

### SbFileWriteAll ###

Writes the given buffer into `file`, starting at the beginning of the file, and
overwriting any data that was previously there. Unlike SbFileWrite, this
function does make a best effort to write all data on all platforms.

The return value identifies the number of bytes written, or `-1` on error.

`file`: The file to which data will be written. `data`: The data to be written.
`size`: The amount of data (in bytes) to write.

#### Declaration ####

```
static int SbFileWriteAll(SbFile file, const char *data, int size)
```

