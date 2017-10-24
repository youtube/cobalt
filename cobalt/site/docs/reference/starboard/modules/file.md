---
layout: doc
title: "Starboard Module Reference: file.h"
---

Defines file system input/output functions.

## Enums

### SbFileError

kSbFileErrorAccessDenied is returned when a call fails because of a
filesystem restriction. kSbFileErrorSecurity is returned when a security
policy doesn't allow the operation to be executed.

**Values**

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
*   `kSbFileErrorMax` - Put new entries here and increment kSbFileErrorMax.

### SbFileFlags

Flags that define how a file is used in the application. These flags should
be or'd together when passed to SbFileOpen to open or create a file.<br>
The following five flags are mutually exclusive. You must specify exactly one
of them:
<ul><li>`kSbFileOpenAlways`
</li><li>`kSbFileOpenOnly`
</li><li>`kSbFileOpenTruncated`
</li><li>`kSbFileCreateAlways`
</li><li>`kSbFileCreateOnly`</li></ul>
In addition, one or more of the following flags must be specified:
<ul><li>`kSbFileRead`
</li><li>`kSbFileWrite`</li></ul>
The `kSbFileAsync` flag is optional.

**Values**

*   `kSbFileOpenOnly` - Opens a file, only if it exists.
*   `kSbFileCreateOnly` - Creates a new file, only if it does not already exist.
*   `kSbFileOpenAlways` - Opens an existing file at the specified path or creates a new file at that path.
*   `kSbFileCreateAlways` - Creates a new file at the specified path or overwrites an existing file at that path.
*   `kSbFileOpenTruncated` - Opens a file and truncates it to zero, only if it exists.
*   `kSbFileRead`
*   `kSbFileWrite`
*   `kSbFileAsync` - May allow asynchronous I/O on some platforms, meaning that calls to Read or Write will only return the data that is readily available.

### SbFileWhence

This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.

**Values**

*   `kSbFileFromBegin`
*   `kSbFileFromCurrent`
*   `kSbFileFromEnd`

## Structs

### SbFileInfo

Used to hold information about a file.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int64_t</code><br>        <code>size</code></td>    <td>The size of the file in bytes. Undefined when is_directory is true.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>is_directory</code></td>    <td>Whether the file corresponds to a directory.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>is_symbolic_link</code></td>    <td>Whether the file corresponds to a symbolic link.</td>  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>last_modified</code></td>    <td>The last modified time of a file.</td>  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>last_accessed</code></td>    <td>The last accessed time of a file.</td>  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>creation_time</code></td>    <td>The creation time of a file.</td>  </tr>
</table>

### SbFilePrivate

Private structure representing an open file.

## Functions

### SbFileCanOpen

**Description**

Indicates whether <code><a href="#sbfileopen">SbFileOpen()</a></code> with the given `flags` is allowed for `path`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileCanOpen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileCanOpen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileCanOpen-declaration">
<pre>
SB_EXPORT bool SbFileCanOpen(const char* path, int flags);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileCanOpen-stub">

```
#include "starboard/file.h"

bool SbFileCanOpen(const char* /*path*/, int /*flags*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>path</code></td>
    <td>The absolute path to be checked.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>flags</code></td>
    <td>The flags that are being evaluated for the given <code>path</code>.</td>
  </tr>
</table>

### SbFileClose

**Description**

Closes `file`. The return value indicates whether the file was closed
successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileClose-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileClose-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileClose-declaration">
<pre>
SB_EXPORT bool SbFileClose(SbFile file);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileClose-stub">

```
#include "starboard/file.h"

bool SbFileClose(SbFile /*file*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The absolute path of the file to be closed.</td>
  </tr>
</table>

### SbFileDelete

**Description**

Deletes the regular file, symlink, or empty directory at `path`. This
function is used primarily to clean up after unit tests. On some platforms,
this function fails if the file in question is being held open.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileDelete-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileDelete-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileDelete-declaration">
<pre>
SB_EXPORT bool SbFileDelete(const char* path);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileDelete-stub">

```
#include "starboard/file.h"

bool SbFileDelete(const char* /*path*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>path</code></td>
    <td>The absolute path fo the file, symlink, or directory to be deleted.</td>
  </tr>
</table>

### SbFileExists

**Description**

Indicates whether a file or directory exists at `path`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileExists-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileExists-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileExists-declaration">
<pre>
SB_EXPORT bool SbFileExists(const char* path);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileExists-stub">

```
#include "starboard/file.h"

bool SbFileExists(const char* /*path*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>path</code></td>
    <td>The absolute path of the file or directory being checked.</td>
  </tr>
</table>

### SbFileFlush

**Description**

Flushes the write buffer to `file`. Data written via <code><a href="#sbfilewrite">SbFileWrite</a></code> is not
necessarily committed until the SbFile is flushed or closed.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileFlush-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileFlush-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileFlush-declaration">
<pre>
SB_EXPORT bool SbFileFlush(SbFile file);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileFlush-stub">

```
#include "starboard/file.h"

bool SbFileFlush(SbFile /*file*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The SbFile to which the write buffer is flushed.</td>
  </tr>
</table>

### SbFileGetInfo

**Description**

Retrieves information about `file`. The return value indicates whether the
file information was retrieved successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileGetInfo-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileGetInfo-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileGetInfo-declaration">
<pre>
SB_EXPORT bool SbFileGetInfo(SbFile file, SbFileInfo* out_info);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileGetInfo-stub">

```
#include "starboard/file.h"

bool SbFileGetInfo(SbFile /*file*/, SbFileInfo* /*out_info*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The SbFile for which information is retrieved.</td>
  </tr>
  <tr>
    <td><code>SbFileInfo*</code><br>        <code>out_info</code></td>
    <td>The variable into which the retrieved data is placed. This
variable is not touched if the operation is not successful.</td>
  </tr>
</table>

### SbFileGetPathInfo

**Description**

Retrieves information about the file at `path`. The return value indicates
whether the file information was retrieved successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileGetPathInfo-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileGetPathInfo-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileGetPathInfo-declaration">
<pre>
SB_EXPORT bool SbFileGetPathInfo(const char* path, SbFileInfo* out_info);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileGetPathInfo-stub">

```
#include "starboard/file.h"

bool SbFileGetPathInfo(const char* /*path*/, SbFileInfo* /*out_info*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>path</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbFileInfo*</code><br>        <code>out_info</code></td>
    <td>The variable into which the retrieved data is placed. This
variable is not touched if the operation is not successful.</td>
  </tr>
</table>

### SbFileIsValid

**Description**

Returns whether the given file handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbFileIsValid(SbFile file) {
  return file != kSbFileInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>
        <code>file</code></td>
    <td> </td>
  </tr>
</table>

### SbFileModeStringToFlags

**Description**

Converts an ISO `fopen()` mode string into flags that can be equivalently
passed into <code><a href="#sbfileopen">SbFileOpen()</a></code>.

**Declaration**

```
SB_EXPORT int SbFileModeStringToFlags(const char* mode);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>mode</code></td>
    <td>The mode string to be converted into flags.</td>
  </tr>
</table>

### SbFileOpen

**Description**

Opens the file at `path`, which must be absolute, creating it if specified by
`flags`. The read/write position is at the beginning of the file.<br>
Note that this function only guarantees the correct behavior when `path`
points to a file. The behavior is undefined if `path` points to a directory.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileOpen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileOpen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileOpen-declaration">
<pre>
SB_EXPORT SbFile SbFileOpen(const char* path,
                            int flags,
                            bool* out_created,
                            SbFileError* out_error);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileOpen-stub">

```
#include "starboard/file.h"

SbFile SbFileOpen(const char* /*path*/,
                  int /*flags*/,
                  bool* /*out_created*/,
                  SbFileError* /*out_error*/) {
  return kSbFileInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>        <code>path</code></td>
    <td>The absolute path of the file to be opened.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>flags</code></td>
    <td><code>SbFileFlags</code> that determine how the file is used in the
application. Among other things, <code>flags</code> can indicate whether the
application should create <code>path</code> if <code>path</code> does not already exist.</td>
  </tr>
  <tr>
    <td><code>bool*</code><br>        <code>out_created</code></td>
    <td>Starboard sets this value to <code>true</code> if a new file is created
or if an old file is truncated to zero length to simulate a new file,
which can happen if the <code>kSbFileCreateAlways</code> flag is set. Otherwise,
Starboard sets this value to <code>false</code>.</td>
  </tr>
  <tr>
    <td><code>SbFileError*</code><br>        <code>out_error</code></td>
    <td>If <code>path</code> cannot be created, this is set to <code>kSbFileInvalid</code>.
Otherwise, it is <code>NULL</code>.</td>
  </tr>
</table>

### SbFileRead

**Description**

Reads `size` bytes (or until EOF is reached) from `file` into `data`,
starting at the file's current position.<br>
The return value specifies the number of bytes read or `-1` if there was
an error. Note that this function does NOT make a best effort to read all
data on all platforms; rather, it just reads however many bytes are quickly
available. However, this function can be run in a loop to make it a
best-effort read.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileRead-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileRead-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileRead-declaration">
<pre>
SB_EXPORT int SbFileRead(SbFile file, char* data, int size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileRead-stub">

```
#include "starboard/file.h"

int SbFileRead(SbFile /*file*/, char* /*data*/, int /*size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The SbFile from which to read data.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>data</code></td>
    <td>The variable to which data is read.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>size</code></td>
    <td>The amount of data (in bytes) to read.</td>
  </tr>
</table>

### SbFileReadAll

**Description**

Reads `size` bytes (or until EOF is reached) from `file` into `data`,
starting from the beginning of the file.<br>
The return value specifies the number of bytes read or `-1` if there was
an error. Note that, unlike `SbFileRead`, this function does make a best
effort to read all data on all platforms. As such, it is not intended for
stream-oriented files but instead for cases when the normal expectation is
that `size` bytes can be read unless there is an error.

**Declaration**

```
static inline int SbFileReadAll(SbFile file, char* data, int size) {
  if (!SbFileIsValid(file) || size < 0) {
    return -1;
  }
  int bytes_read = 0;
  int rv;
  do {
    rv = SbFileRead(file, data + bytes_read, size - bytes_read);
    if (bytes_read <= 0) {
      break;
    }
    bytes_read += rv;
  } while (bytes_read < size);
  return bytes_read ? bytes_read : rv;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The SbFile from which to read data.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>data</code></td>
    <td>The variable to which data is read.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>size</code></td>
    <td>The amount of data (in bytes) to read.</td>
  </tr>
</table>

### SbFileSeek

**Description**

Changes the current read/write position in `file`. The return value
identifies the resultant current read/write position in the file (relative
to the start) or `-1` in case of an error. This function might not support
seeking past the end of the file on some platforms.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileSeek-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileSeek-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileSeek-declaration">
<pre>
SB_EXPORT int64_t SbFileSeek(SbFile file, SbFileWhence whence, int64_t offset);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileSeek-stub">

```
#include "starboard/file.h"

int64_t SbFileSeek(SbFile /*file*/,
                   SbFileWhence /*whence*/,
                   int64_t /*offset*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The SbFile in which the read/write position will be changed.</td>
  </tr>
  <tr>
    <td><code>SbFileWhence</code><br>        <code>whence</code></td>
    <td>The starting read/write position. The position is modified relative
to this value.</td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>        <code>offset</code></td>
    <td>The amount that the read/write position is changed, relative to
<code>whence</code>.</td>
  </tr>
</table>

### SbFileTruncate

**Description**

Truncates the given `file` to the given `length`. The return value indicates
whether the file was truncated successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileTruncate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileTruncate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileTruncate-declaration">
<pre>
SB_EXPORT bool SbFileTruncate(SbFile file, int64_t length);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileTruncate-stub">

```
#include "starboard/file.h"

bool SbFileTruncate(SbFile /*file*/, int64_t /*length*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The file to be truncated.</td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>        <code>length</code></td>
    <td>The expected length of the file after it is truncated. If <code>length</code>
is greater than the current size of the file, then the file is extended
with zeros. If <code>length</code> is negative, then the function is a no-op and
returns <code>false</code>.</td>
  </tr>
</table>

### SbFileWrite

**Description**

Writes the given buffer into `file` at the file's current position,
overwriting any data that was previously there.<br>
The return value identifies the number of bytes written, or `-1` on error.
Note that this function does NOT make a best effort to write all data;
rather, it writes however many bytes can be written quickly. Generally, this
means that it writes however many bytes as possible without blocking on IO.
Run this function in a loop to ensure that all data is written.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbFileWrite-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbFileWrite-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbFileWrite-declaration">
<pre>
SB_EXPORT int SbFileWrite(SbFile file, const char* data, int size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbFileWrite-stub">

```
#include "starboard/file.h"

int SbFileWrite(SbFile /*file*/, const char* /*data*/, int /*size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The SbFile to which data will be written.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>data</code></td>
    <td>The data to be written.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>size</code></td>
    <td>The amount of data (in bytes) to write.</td>
  </tr>
</table>

### SbFileWriteAll

**Description**

Writes the given buffer into `file`, starting at the beginning of the file,
and overwriting any data that was previously there. Unlike <code><a href="#sbfilewrite">SbFileWrite</a></code>, this
function does make a best effort to write all data on all platforms.<br>
The return value identifies the number of bytes written, or `-1` on error.

**Declaration**

```
static inline int SbFileWriteAll(SbFile file, const char* data, int size) {
  if (!SbFileIsValid(file) || size < 0) {
    return -1;
  }
  int bytes_written = 0;
  int rv;
  do {
    rv = SbFileWrite(file, data + bytes_written, size - bytes_written);
    if (bytes_written <= 0) {
      break;
    }
    bytes_written += rv;
  } while (bytes_written < size);
  return bytes_written ? bytes_written : rv;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbFile</code><br>        <code>file</code></td>
    <td>The file to which data will be written.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>data</code></td>
    <td>The data to be written.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>size</code></td>
    <td>The amount of data (in bytes) to write.</td>
  </tr>
</table>

