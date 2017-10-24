---
layout: doc
title: "Starboard Module Reference: directory.h"
---

Provides directory listing functions.

## Structs

### SbDirectoryEntry

Represents a directory entry.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>char</code><br>        <code>name[SB_FILE_MAX_NAME]</code></td>    <td>The name of this directory entry.</td>  </tr>
</table>

### SbDirectory

A handle to an open directory stream.

## Functions

### SbDirectoryCanOpen

**Description**

Indicates whether <code><a href="#sbdirectoryopen">SbDirectoryOpen</a></code> is allowed for the given `path`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDirectoryCanOpen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDirectoryCanOpen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDirectoryCanOpen-declaration">
<pre>
SB_EXPORT bool SbDirectoryCanOpen(const char* path);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDirectoryCanOpen-stub">

```
#include "starboard/directory.h"

bool SbDirectoryCanOpen(const char* /*path*/) {
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
    <td>The path to be checked.</td>
  </tr>
</table>

### SbDirectoryClose

**Description**

Closes an open directory stream handle. The return value indicates whether
the directory was closed successfully.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDirectoryClose-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDirectoryClose-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDirectoryClose-declaration">
<pre>
SB_EXPORT bool SbDirectoryClose(SbDirectory directory);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDirectoryClose-stub">

```
#include "starboard/directory.h"

bool SbDirectoryClose(SbDirectory /*directory*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDirectory</code><br>        <code>directory</code></td>
    <td>The directory stream handle to close.</td>
  </tr>
</table>

### SbDirectoryCreate

**Description**

Creates the directory `path`, assuming the parent directory already exists.
This function returns `true` if the directory now exists (even if it existed
before) and returns `false` if the directory does not exist.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDirectoryCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDirectoryCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDirectoryCreate-declaration">
<pre>
SB_EXPORT bool SbDirectoryCreate(const char* path);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDirectoryCreate-stub">

```
#include "starboard/directory.h"

bool SbDirectoryCreate(const char* /*path*/) {
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
    <td>The path to be created.</td>
  </tr>
</table>

### SbDirectoryGetNext

**Description**

Populates `out_entry` with the next entry in the specified directory stream,
and moves the stream forward by one entry.<br>
This function returns `true` if there was a next directory, and `false`
at the end of the directory stream.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDirectoryGetNext-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDirectoryGetNext-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDirectoryGetNext-declaration">
<pre>
SB_EXPORT bool SbDirectoryGetNext(SbDirectory directory,
                                  SbDirectoryEntry* out_entry);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDirectoryGetNext-stub">

```
#include "starboard/directory.h"

bool SbDirectoryGetNext(SbDirectory /*directory*/,
                        SbDirectoryEntry* /*out_entry*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDirectory</code><br>        <code>directory</code></td>
    <td>The directory stream from which to retrieve the next directory.</td>
  </tr>
  <tr>
    <td><code>SbDirectoryEntry*</code><br>        <code>out_entry</code></td>
    <td>The variable to be populated with the next directory entry.</td>
  </tr>
</table>

### SbDirectoryIsValid

**Description**

Returns whether the given directory stream handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbDirectoryIsValid(SbDirectory directory) {
  return directory != kSbDirectoryInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDirectory</code><br>
        <code>directory</code></td>
    <td> </td>
  </tr>
</table>

### SbDirectoryOpen

**Description**

Opens the given existing directory for listing. This function returns
kSbDirectoryInvalidHandle if it is not successful.<br>
If `out_error` is provided by the caller, it will be set to the appropriate
SbFileError code on failure.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDirectoryOpen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDirectoryOpen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDirectoryOpen-declaration">
<pre>
SB_EXPORT SbDirectory SbDirectoryOpen(const char* path, SbFileError* out_error);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDirectoryOpen-stub">

```
#include "starboard/directory.h"

SbDirectory SbDirectoryOpen(const char* /*path*/, SbFileError* /*out_error*/) {
  return kSbDirectoryInvalid;
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
    <td><code>SbFileError*</code><br>        <code>out_error</code></td>
    <td>An output parameter that, in case of an error, is set to the
reason that the directory could not be opened.</td>
  </tr>
</table>

