---
layout: doc
title: "Starboard Module Reference: system.h"
---

Defines a broad set of APIs that allow the client application to query
build and runtime properties of the enclosing system.

## Enums

### SbSystemCapabilityId

Runtime capabilities are boolean properties of a platform that can't be
determined at compile-time. They may vary from device to device, but they
will not change over the course of a single execution. They often specify
particular behavior of other APIs within the bounds of their operating range.

**Values**

*   `kSbSystemCapabilityReversedEnterAndBack` - Whether this system has reversed Enter and Back keys.
*   `kSbSystemCapabilityCanQueryGPUMemoryStats` - Whether this system has the ability to report on GPU memory usage.If (and only if) a system has this capcability willSbSystemGetTotalGPUMemory() and SbSystemGetUsedGPUMemory() be valid tocall.

### SbSystemConnectionType

Enumeration of network connection types.

**Values**

*   `kSbSystemConnectionTypeWired` - The system is on a wired connection.
*   `kSbSystemConnectionTypeWireless` - The system is on a wireless connection.
*   `kSbSystemConnectionTypeUnknown` - The system connection type is unknown.

### SbSystemDeviceType

Enumeration of device types.

**Values**

*   `kSbSystemDeviceTypeBlueRayDiskPlayer` - Blue-ray Disc Player (BDP).
*   `kSbSystemDeviceTypeGameConsole` - A relatively high-powered TV device used primarily for playing games.
*   `kSbSystemDeviceTypeOverTheTopBox` - Over the top (OTT) devices stream content via the Internet over anothertype of network, e.g. cable or satellite.
*   `kSbSystemDeviceTypeSetTopBox` - Set top boxes (STBs) stream content primarily over cable or satellite.Some STBs can also stream OTT content via the Internet.
*   `kSbSystemDeviceTypeTV` - A Smart TV is a TV that can directly run applications that stream OTTcontent via the Internet.
*   `kSbSystemDeviceTypeDesktopPC` - Desktop PC.
*   `kSbSystemDeviceTypeAndroidTV` - An Android TV Device.
*   `kSbSystemDeviceTypeUnknown` - Unknown device.

### SbSystemPathId

Enumeration of special paths that the platform can define.

**Values**

*   `kSbSystemPathContentDirectory` - Path to where the local content files that ship with the binary areavailable.
*   `kSbSystemPathCacheDirectory` - Path to the directory that can be used as a local file cache, ifavailable.
*   `kSbSystemPathDebugOutputDirectory` - Path to the directory where debug output (e.g. logs, trace output,screenshots) can be written into.
*   `kSbSystemPathFontDirectory` - Path to a directory where system font files can be found. Should only bespecified on platforms that provide fonts usable by Starboard applications.
*   `kSbSystemPathFontConfigurationDirectory` - Path to a directory where system font configuration metadata can befound. May be the same directory as |kSbSystemPathFontDirectory|, but notnecessarily. Should only be specified on platforms that provide fontsusable by Starboard applications.
*   `kSbSystemPathSourceDirectory` - Path to the directory containing the root of the source tree.
*   `kSbSystemPathTempDirectory` - Path to a directory where temporary files can be written.
*   `kSbSystemPathTestOutputDirectory` - Path to a directory where test results can be written.
*   `kSbSystemPathExecutableFile` - Full path to the executable file.

### SbSystemPlatformErrorResponse

Possible responses for `SbSystemPlatformErrorCallback`.

**Values**

*   `kSbSystemPlatformErrorResponsePositive`
*   `kSbSystemPlatformErrorResponseNegative`

### SbSystemPlatformErrorType

Enumeration of possible values for the `type` parameter passed to  the
`SbSystemRaisePlatformError` function.

**Values**

*   `kSbSystemPlatformErrorTypeConnectionError` - Cobalt received a network connection error, or a network disconnectionevent. If the |response| passed to |SbSystemPlatformErrorCallback| is|kSbSystemPlatformErrorResponsePositive| then the request should beretried, otherwise the app should be stopped.
*   `kSbSystemPlatformErrorTypeUserSignedOut` - The current user is not signed in.

### SbSystemPropertyId

System properties that can be queried for. Many of these are used in
User-Agent string generation.

**Values**

*   `kSbSystemPropertyChipsetModelNumber` - The full model number of the main platform chipset, including anyvendor-specific prefixes.
*   `kSbSystemPropertyFirmwareVersion` - The production firmware version number which the device is currentlyrunning.
*   `kSbSystemPropertyFriendlyName` - A friendly name for this actual device. It may include user-personalizationlike "Upstairs Bedroom." It may be displayed to users as part of some kindof device selection (e.g. in-app DIAL).
*   `kSbSystemPropertyManufacturerName` - A deprecated alias for |kSbSystemPropertyBrandName|.
*   `kSbSystemPropertyBrandName` - The name of the brand under which the device is being sold.
*   `kSbSystemPropertyModelName` - The final production model number of the device.
*   `kSbSystemPropertyModelYear` - The year the device was launched, e.g. "2016".
*   `kSbSystemPropertyNetworkOperatorName` - The name of the network operator that owns the target device, ifapplicable.
*   `kSbSystemPropertyPlatformName` - The name of the operating system and platform, suitable for inclusion in aUser-Agent, say.
*   `kSbSystemPropertyPlatformUuid` - A universally-unique ID for the current user.
*   `kSbSystemPropertySpeechApiKey` - The Google Speech API key. The platform manufacturer is responsiblefor registering a Google Speech API key for their products. In the APIConsole (http://developers.google.com/console), you can enable theSpeech APIs and generate a Speech API key.
*   `kSbSystemPropertyUserAgentAuxField` - A field that, if available, is appended to the user agent

## Structs

### SbSystemPlatformErrorPrivate

Private structure used to represent a raised platform error.

## Functions

### SbSystemBinarySearch

**Description**

Binary searches a sorted table `base` of `element_count` objects, each
element `element_width` bytes in size for an element that `comparator`
compares equal to `key`.<br>
This function is meant to be a drop-in replacement for `bsearch`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemBinarySearch-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemBinarySearch-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemBinarySearch-declaration">
<pre>
SB_EXPORT void* SbSystemBinarySearch(const void* key,
                                     const void* base,
                                     size_t element_count,
                                     size_t element_width,
                                     SbSystemComparator comparator);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemBinarySearch-stub">

```
#include "starboard/system.h"

void* SbSystemBinarySearch(const void* /*key*/,
                           const void* /*base*/,
                           size_t /*element_count*/,
                           size_t /*element_width*/,
                           SbSystemComparator /*comparator*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const void*</code><br>        <code>key</code></td>
    <td>The key to search for in the table.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>base</code></td>
    <td>The sorted table of elements to be searched.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>element_count</code></td>
    <td>The number of elements in the table.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>element_width</code></td>
    <td>The size, in bytes, of each element in the table.</td>
  </tr>
  <tr>
    <td><code>SbSystemComparator</code><br>        <code>comparator</code></td>
    <td>A value that indicates how the element in the table should
compare to the specified <code>key</code>.</td>
  </tr>
</table>

### SbSystemBreakIntoDebugger

**Description**

Breaks the current program into the debugger, if a debugger is attached.
If a debugger is not attached, this function aborts the program.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemBreakIntoDebugger-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemBreakIntoDebugger-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemBreakIntoDebugger-declaration">
<pre>
SB_NORETURN SB_EXPORT void SbSystemBreakIntoDebugger();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemBreakIntoDebugger-stub">

```
#include "starboard/system.h"

void SbSystemBreakIntoDebugger() {
#if SB_IS(COMPILER_GCC)
  __builtin_unreachable();
#endif
}
```

  </div>
</div>

### SbSystemClearLastError

**Description**

Clears the last error set by a Starboard call in the current thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemClearLastError-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemClearLastError-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemClearLastError-declaration">
<pre>
SB_EXPORT void SbSystemClearLastError();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemClearLastError-stub">

```
#include "starboard/system.h"

void SbSystemClearLastError() {
}
```

  </div>
</div>

### SbSystemClearPlatformError

**Description**

Clears a platform error that was previously raised by a call to
`SbSystemRaisePlatformError`. The platform may use this, for example,
to close a dialog that was opened in response to the error.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemClearPlatformError-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemClearPlatformError-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemClearPlatformError-declaration">
<pre>
SB_EXPORT void SbSystemClearPlatformError(SbSystemPlatformError handle);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemClearPlatformError-stub">

```
#include "starboard/system.h"

void SbSystemClearPlatformError(SbSystemPlatformError handle) {
  SB_UNREFERENCED_PARAMETER(handle);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemPlatformError</code><br>        <code>handle</code></td>
    <td>The platform error to be cleared.</td>
  </tr>
</table>

### SbSystemGetConnectionType

**Description**

Returns the device's current network connection type.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetConnectionType-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetConnectionType-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetConnectionType-declaration">
<pre>
SB_EXPORT SbSystemConnectionType SbSystemGetConnectionType();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetConnectionType-stub">

```
#include "starboard/system.h"

SbSystemConnectionType SbSystemGetConnectionType() {
  return kSbSystemConnectionTypeUnknown;
}
```

  </div>
</div>

### SbSystemGetDeviceType

**Description**

Returns the type of the device.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetDeviceType-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetDeviceType-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetDeviceType-declaration">
<pre>
SB_EXPORT SbSystemDeviceType SbSystemGetDeviceType();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetDeviceType-stub">

```
#include "starboard/system.h"

SbSystemDeviceType SbSystemGetDeviceType() {
  return kSbSystemDeviceTypeUnknown;
}
```

  </div>
</div>

### SbSystemGetErrorString

**Description**

Generates a human-readable string for an error. The return value specifies
the total desired length of the string.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetErrorString-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetErrorString-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetErrorString-declaration">
<pre>
SB_EXPORT int SbSystemGetErrorString(SbSystemError error,
                                     char* out_string,
                                     int string_length);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetErrorString-stub">

```
#include "starboard/system.h"

int SbSystemGetErrorString(SbSystemError /*error*/,
                           char* /*out_string*/,
                           int /*string_length*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemError</code><br>        <code>error</code></td>
    <td>The error for which a human-readable string is generated.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>out_string</code></td>
    <td>The generated string. This value may be null, and it is
always terminated with a null byte.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>string_length</code></td>
    <td>The maximum length of the error string.</td>
  </tr>
</table>

### SbSystemGetLastError

**Description**

Gets the last platform-specific error code produced by any Starboard call in
the current thread for diagnostic purposes. Semantic reactions to Starboard
function call results should be modeled explicitly.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetLastError-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetLastError-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetLastError-declaration">
<pre>
SB_EXPORT SbSystemError SbSystemGetLastError();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetLastError-stub">

```
#include "starboard/system.h"

SbSystemError SbSystemGetLastError() {
  return 0;
}
```

  </div>
</div>

### SbSystemGetLocaleId

**Description**

Gets the system's current POSIX-style Locale ID. The locale represents the
location, language, and cultural conventions that the system wants to use,
which affects which text is displayed to the user as well as how displayed
numbers, dates, currency, and similar values are formatted.<br>
At its simplest, the locale ID can just be a BCP 47 language code, like
`en_US`. Currently, POSIX also wants to include the encoding as in
`en_US.UTF8`. POSIX also allows a couple very bare-bones locales, like "C"
or "POSIX", but they are not supported here. POSIX also supports different
locale settings for a few different purposes, but Starboard only exposes one
locale at a time.<br>
RFC 5646 describes BCP 47 language codes:
https://tools.ietf.org/html/bcp47<br>
For more information than you probably want about POSIX locales, see:
http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetLocaleId-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetLocaleId-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetLocaleId-declaration">
<pre>
SB_EXPORT const char* SbSystemGetLocaleId();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetLocaleId-stub">

```
#include "starboard/system.h"

const char* SbSystemGetLocaleId() {
  return NULL;
}
```

  </div>
</div>

### SbSystemGetNumberOfProcessors

**Description**

Returns the number of processor cores available to this application. If the
process is sandboxed to a subset of the physical cores, the function returns
that sandboxed limit.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetNumberOfProcessors-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetNumberOfProcessors-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetNumberOfProcessors-declaration">
<pre>
SB_EXPORT int SbSystemGetNumberOfProcessors();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetNumberOfProcessors-stub">

```
#include "starboard/system.h"

int SbSystemGetNumberOfProcessors() {
  return 1;
}
```

  </div>
</div>

### SbSystemGetPath

**Description**

Retrieves the platform-defined system path specified by `path_id` and
places it as a zero-terminated string into the user-allocated `out_path`
unless it is longer than `path_length` - 1. This implementation must be
thread-safe.<br>
This function returns `true` if the path is retrieved successfully. It
returns `false` under any of the following conditions and, in any such
case, `out_path` is not changed:
<ul><li>`path_id` is invalid for this platform
</li><li>`path_length` is too short for the given result
</li><li>`out_path` is NULL</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetPath-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetPath-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetPath-declaration">
<pre>
SB_EXPORT bool SbSystemGetPath(SbSystemPathId path_id,
                               char* out_path,
                               int path_length);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetPath-stub">

```
#include "starboard/system.h"

bool SbSystemGetPath(SbSystemPathId /*path_id*/, char* /*out_path*/,
                     int /*path_size*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemPathId</code><br>        <code>path_id</code></td>
    <td>The system path to be retrieved.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>out_path</code></td>
    <td>The platform-defined system path specified by <code>path_id</code>.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>path_length</code></td>
    <td>The length of the system path.</td>
  </tr>
</table>

### SbSystemGetProperty

**Description**

Retrieves the platform-defined system property specified by `property_id` and
places its value as a zero-terminated string into the user-allocated
`out_value` unless it is longer than `value_length` - 1. This implementation
must be thread-safe.<br>
This function returns `true` if the property is retrieved successfully. It
returns `false` under any of the following conditions and, in any such
case, `out_value` is not changed:
<ul><li>`property_id` is invalid for this platform
</li><li>`value_length` is too short for the given result
</li><li>`out_value` is NULL</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetProperty-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetProperty-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetProperty-declaration">
<pre>
SB_EXPORT bool SbSystemGetProperty(SbSystemPropertyId property_id,
                                   char* out_value,
                                   int value_length);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetProperty-stub">

```
#include "starboard/system.h"

bool SbSystemGetProperty(SbSystemPropertyId /*property_id*/,
                         char* /*out_value*/,
                         int /*value_length*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemPropertyId</code><br>        <code>property_id</code></td>
    <td>The system path to be retrieved.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>out_value</code></td>
    <td>The platform-defined system property specified by <code>property_id</code>.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>value_length</code></td>
    <td>The length of the system property.</td>
  </tr>
</table>

### SbSystemGetRandomData

**Description**

A cryptographically secure random number generator that produces an
arbitrary, non-negative number of `buffer_size` random, non-negative bytes.
The generated number is placed in `out_buffer`. This function does not
require manual seeding.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetRandomData-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetRandomData-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSystemGetRandomData-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetRandomData-declaration">
<pre>
SB_EXPORT void SbSystemGetRandomData(void* out_buffer, int buffer_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetRandomData-stub">

```
#include "starboard/system.h"

void SbSystemGetRandomData(void* /*out_buffer*/, int /*buffer_size*/) {
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSystemGetRandomData-linux">

```
#include "starboard/system.h"

#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/once.h"

namespace {

// We keep the open file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive).
class URandomFile {
 public:
  URandomFile() {
    file_ =
        SbFileOpen("/dev/urandom", kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    SB_DCHECK(SbFileIsValid(file_)) << "Cannot open /dev/urandom";
  }

  ~URandomFile() { SbFileClose(file_); }

  SbFile file() const { return file_; }

 private:
  SbFile file_;
};

// A file that will produce any number of very random bytes.
URandomFile* g_urandom_file = NULL;

// Control to initialize g_urandom_file.
SbOnceControl g_urandom_file_once = SB_ONCE_INITIALIZER;

// Lazily initialize g_urandom_file.
void InitializeRandom() {
  SB_DCHECK(g_urandom_file == NULL);
  g_urandom_file = new URandomFile();
}

}  // namespace

void SbSystemGetRandomData(void* out_buffer, int buffer_size) {
  SB_DCHECK(out_buffer);
  char* buffer = reinterpret_cast<char*>(out_buffer);
  int remaining = buffer_size;
  bool once_result = SbOnce(&g_urandom_file_once, &InitializeRandom);
  SB_DCHECK(once_result);

  SbFile file = g_urandom_file->file();
  do {
    // This is unsynchronized access to the File that could happen from multiple
    // threads. It doesn't appear that there is any locking in the Chromium
    // POSIX implementation that is very similar.
    int result = SbFileRead(file, buffer, remaining);
    if (result <= 0)
      break;

    remaining -= result;
    buffer += result;
  } while (remaining);

  SB_CHECK(remaining == 0);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>out_buffer</code></td>
    <td>A pointer for the generated random number. This value must
not be null.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>buffer_size</code></td>
    <td>The size of the random number, in bytes.</td>
  </tr>
</table>

### SbSystemGetRandomUInt64

**Description**

A cryptographically secure random number generator that gets 64 random bits
and returns them as an `uint64_t`. This function does not require manual
seeding.

**Declaration**

```
SB_EXPORT uint64_t SbSystemGetRandomUInt64();
```

### SbSystemGetStack

**Description**

Places up to `stack_size` instruction pointer addresses of the current
execution stack into `out_stack`. The return value specifies the number
of entries added.<br>
The returned stack frames are in "downward" order from the calling frame
toward the entry point of the thread. So, if all the stack frames do not
fit, the ones truncated will be the less interesting ones toward the thread
entry point.<br>
This function is used in crash signal handlers and, therefore, it must
be async-signal-safe on platforms that support signals. The following
document discusses what it means to be async-signal-safe on POSIX:
http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetStack-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetStack-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSystemGetStack-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetStack-declaration">
<pre>
SB_EXPORT int SbSystemGetStack(void** out_stack, int stack_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetStack-stub">

```
#include "starboard/system.h"

int SbSystemGetStack(void** /*out_stack*/, int /*stack_size*/) {
  return 0;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSystemGetStack-linux">

```
#include "starboard/system.h"

#include <execinfo.h>

#include <algorithm>

int SbSystemGetStack(void** out_stack, int stack_size) {
  int count = std::max(backtrace(out_stack, stack_size), 0);

  if (count < 1) {
    // No stack, for some reason.
    return count;
  }

  // We have an extra stack frame (for this very function), so let's remove it.
  for (int i = 1; i < count; ++i) {
    out_stack[i - 1] = out_stack[i];
  }

  return count - 1;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void**</code><br>        <code>out_stack</code></td>
    <td>A non-NULL array of <code>void *</code> of at least <code>stack_size</code> entries.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>stack_size</code></td>
    <td>The maximum number of instruction pointer addresses to be
placed into <code>out_stack</code> from the current execution stack.</td>
  </tr>
</table>

### SbSystemGetTotalCPUMemory

**Description**

Returns the total CPU memory (in bytes) potentially available to this
application. If the process is sandboxed to a maximum allowable limit,
the function returns the lesser of the physical and sandbox limits.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetTotalCPUMemory-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetTotalCPUMemory-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSystemGetTotalCPUMemory-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetTotalCPUMemory-declaration">
<pre>
SB_EXPORT int64_t SbSystemGetTotalCPUMemory();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetTotalCPUMemory-stub">

```
#include "starboard/system.h"

int64_t SbSystemGetTotalCPUMemory() {
  return 0;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSystemGetTotalCPUMemory-linux">

```
#include "starboard/system.h"

#include <unistd.h>

#include "starboard/log.h"

int64_t SbSystemGetTotalCPUMemory() {
  long pages = sysconf(_SC_PHYS_PAGES);     // NOLINT[runtime/int]
  long page_size = sysconf(_SC_PAGE_SIZE);  // NOLINT[runtime/int]
  if (pages == -1 || page_size == -1) {
    SB_NOTREACHED();
    return 0;
  }

  return static_cast<int64_t>(pages) * page_size;
}
```

  </div>
</div>

### SbSystemGetTotalGPUMemory

**Description**

Returns the total GPU memory (in bytes) available for use by this
application. This function may only be called the return value for calls to
SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats) is `true`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetTotalGPUMemory-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetTotalGPUMemory-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetTotalGPUMemory-declaration">
<pre>
SB_EXPORT int64_t SbSystemGetTotalGPUMemory();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetTotalGPUMemory-stub">

```
#include "starboard/system.h"

int64_t SbSystemGetTotalGPUMemory() {
  return 0;
}
```

  </div>
</div>

### SbSystemGetUsedCPUMemory

**Description**

Returns the total physical CPU memory (in bytes) used by this application.
This value should always be less than (or, in particularly exciting
situations, equal to) <code><a href="#sbsystemgettotalcpumemory">SbSystemGetTotalCPUMemory()</a></code>.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetUsedCPUMemory-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetUsedCPUMemory-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSystemGetUsedCPUMemory-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetUsedCPUMemory-declaration">
<pre>
SB_EXPORT int64_t SbSystemGetUsedCPUMemory();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetUsedCPUMemory-stub">

```
#include "starboard/system.h"

int64_t SbSystemGetUsedCPUMemory() {
  return 0;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSystemGetUsedCPUMemory-linux">

```
#include "starboard/system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/string.h"

// We find the current amount of used memory on Linux by opening
// '/proc/self/status' and scan the file for its "VmRSS" entry.  Essentially,
// we need to parse a buffer that has the following format:
//
// xxxxxx:       45327 kB
// yyyyyy:          23 kB
// VmRSS:        87432 kB
// zzzzzz:        3213 kB
// ...
//
// And here, we would want to return the value 87432 * 1024.
// See http://man7.org/linux/man-pages/man5/proc.5.html for more details.

// Searches for the value of VmRSS and returns it.  Will modify |buffer| in
// order to do so quickly and easily.
int64_t SearchForVmRSSValue(char* buffer, size_t length) {
  // Search for the string ""VmRSS:".
  const char kSearchString[] = "\nVmRSS:";
  enum State {
    // We are currently searching for kSearchString
    kSearchingForSearchString,
    // We found the search string and are advancing through spaces/tabs until
    // we see a number.
    kAdvancingSpacesToNumber,
    // We found the number and are now searching for the end of it.
    kFindingEndOfNumber,
  };
  State state = kSearchingForSearchString;
  const char* number_start = NULL;
  for (size_t i = 0; i < length - sizeof(kSearchString); ++i) {
    if (state == kSearchingForSearchString) {
      if (SbStringCompare(&buffer[i], kSearchString,
                          sizeof(kSearchString) - 1) == 0) {
        // Advance until we find a number.
        state = kAdvancingSpacesToNumber;
        i += sizeof(kSearchString) - 2;
      }
    } else if (state == kAdvancingSpacesToNumber) {
      if (buffer[i] >= '0' && buffer[i] <= '9') {
        // We found the start of the number, record where that is and then
        // continue searching for the end of the number.
        number_start = &buffer[i];
        state = kFindingEndOfNumber;
      }
    } else {
      SB_DCHECK(state == kFindingEndOfNumber);
      if (buffer[i] < '0' || buffer[i] > '9') {
        // Drop a null at the end of the number so that we can call atoi() on
        // it and return.
        buffer[i] = '\0';
        return SbStringAToI(number_start);
      }
    }
  }

  SB_LOG(ERROR) << "Could not find 'VmRSS:' in /proc/self/status.";
  return 0;
}

int64_t SbSystemGetUsedCPUMemory() {
  // Read our process' current physical memory usage from /proc/self/status.
  // This requires a bit of parsing through the output to find the value for
  // the "VmRSS" field which indicates used physical memory.
  starboard::ScopedFile status_file("/proc/self/status",
                                    kSbFileOpenOnly | kSbFileRead);
  if (!status_file.IsValid()) {
    SB_LOG(ERROR)
        << "Error opening /proc/self/status in order to query self memory "
           "usage.";
    return 0;
  }

  // Read the entire file into memory.
  const int kBufferSize = 2048;
  char buffer[kBufferSize];
  int remaining = kBufferSize;
  char* output_pointer = buffer;
  do {
    int result = status_file.Read(output_pointer, remaining);
    if (result <= 0)
      break;

    remaining -= result;
    output_pointer += result;
  } while (remaining);

  // Return the result, multiplied by 1024 because it is given in kilobytes.
  return SearchForVmRSSValue(buffer,
                             static_cast<size_t>(output_pointer - buffer)) *
         1024;
}
```

  </div>
</div>

### SbSystemGetUsedGPUMemory

**Description**

Returns the current amount of GPU memory (in bytes) that is currently being
used by this application. This function may only be called if the return
value for calls to
SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats) is `true`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemGetUsedGPUMemory-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemGetUsedGPUMemory-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemGetUsedGPUMemory-declaration">
<pre>
SB_EXPORT int64_t SbSystemGetUsedGPUMemory();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemGetUsedGPUMemory-stub">

```
#include "starboard/system.h"

int64_t SbSystemGetUsedGPUMemory() {
  return 0;
}
```

  </div>
</div>

### SbSystemHasCapability

**Description**

Returns whether the platform has the runtime capability specified by
`capability_id`. Returns false for any unknown capabilities. This
implementation must be thread-safe.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemHasCapability-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemHasCapability-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemHasCapability-declaration">
<pre>
SB_EXPORT bool SbSystemHasCapability(SbSystemCapabilityId capability_id);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemHasCapability-stub">

```
#include "starboard/system.h"

bool SbSystemHasCapability(SbSystemCapabilityId /*capability_id*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemCapabilityId</code><br>        <code>capability_id</code></td>
    <td>The runtime capability to check.</td>
  </tr>
</table>

### SbSystemHideSplashScreen

**Description**

Hides the system splash screen on systems that support a splash screen that
is displayed while the application is loading. This function may be called
from any thread and must be idempotent.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemHideSplashScreen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemHideSplashScreen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemHideSplashScreen-declaration">
<pre>
SB_EXPORT void SbSystemHideSplashScreen();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemHideSplashScreen-stub">

```
#include "starboard/system.h"

void SbSystemHideSplashScreen() {}
```

  </div>
</div>

### SbSystemIsDebuggerAttached

**Description**

Attempts to determine whether the current program is running inside or
attached to a debugger. The function returns `false` if neither of those
cases is true.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemIsDebuggerAttached-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemIsDebuggerAttached-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSystemIsDebuggerAttached-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemIsDebuggerAttached-declaration">
<pre>
SB_EXPORT bool SbSystemIsDebuggerAttached();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemIsDebuggerAttached-stub">

```
#include "starboard/system.h"

bool SbSystemIsDebuggerAttached() {
  return false;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSystemIsDebuggerAttached-linux">

```
#include "starboard/system.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/log.h"

#include "starboard/shared/posix/file_internal.h"
#include "starboard/shared/posix/handle_eintr.h"

// Adapted from base/debug/debugger_posix.cc
bool SbSystemIsDebuggerAttached() {
  // We can look in /proc/self/status for TracerPid.  We are likely used in
  // crash handling, so we are careful not to use the heap or have side effects.
  // Another option that is common is to try to ptrace yourself, but then we
  // can't detach without forking(), and that's not so great.

  // NOTE: This code MUST be async-signal safe (it's used by in-process stack
  // dumping signal handler). NO malloc or stdio is allowed here.

  int status_fd = open("/proc/self/status", O_RDONLY);
  if (status_fd == -1)
    return false;

  // We assume our line will be in the first 1024 characters and that we can
  // read this much all at once.  In practice this will generally be true.
  // This simplifies and speeds up things considerably.
  char buf[1024];

  ssize_t num_read = HANDLE_EINTR(read(status_fd, buf, sizeof(buf) - 1));
  SB_DCHECK(num_read < sizeof(buf));
  if (HANDLE_EINTR(close(status_fd)) < 0)
    return false;

  if (num_read <= 0)
    return false;

  buf[num_read] = '\0';
  const char tracer[] = "TracerPid:\t";
  char* pid_index = strstr(buf, tracer);
  if (pid_index == NULL)
    return false;

  // Our pid is 0 without a debugger, assume this for any pid starting with 0.
  pid_index += strlen(tracer);
  return pid_index < (buf + num_read) && *pid_index != '0';
}
```

  </div>
</div>

### SbSystemPlatformErrorIsValid

**Description**

Checks whether a `SbSystemPlatformError` is valid.

**Declaration**

```
static SB_C_INLINE bool SbSystemPlatformErrorIsValid(
    SbSystemPlatformError handle) {
  return handle != kSbSystemPlatformErrorInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemPlatformError</code><br>
        <code>handle</code></td>
    <td> </td>
  </tr>
</table>

### SbSystemRaisePlatformError

**Description**

Cobalt calls this function to notify the platform that an error has occurred
in the application that the platform may need to handle. The platform is
expected to then notify the user of the error and to provide a means for
any required interaction, such as by showing a dialog.<br>
The return value is a handle that may be used in a subsequent call to
`SbClearPlatformError`. For example, the handle could be used to
programatically dismiss a dialog that was raised in response to the error.
The lifetime of the object referenced by the handle is until the user reacts
to the error or the error is dismissed by a call to
SbSystemClearPlatformError, whichever happens first. Note that if the
platform cannot respond to the error, then this function should return
`kSbSystemPlatformErrorInvalid`.<br>
This function may be called from any thread, and it is the platform's
responsibility to decide how to handle an error received while a previous
error is still pending. If that platform can only handle one error at a
time, then it may queue the second error or ignore it by returning
`kSbSystemPlatformErrorInvalid`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemRaisePlatformError-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemRaisePlatformError-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemRaisePlatformError-declaration">
<pre>
SB_EXPORT SbSystemPlatformError
SbSystemRaisePlatformError(SbSystemPlatformErrorType type,
                           SbSystemPlatformErrorCallback callback,
                           void* user_data);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemRaisePlatformError-stub">

```
#include "starboard/system.h"

#include "starboard/log.h"

SbSystemPlatformError SbSystemRaisePlatformError(
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data) {
  SB_UNREFERENCED_PARAMETER(callback);
  SB_UNREFERENCED_PARAMETER(user_data);
  std::string message;
  switch (type) {
    case kSbSystemPlatformErrorTypeConnectionError:
      message = "Connection error.";
      break;
#if SB_API_VERSION < 6
    case kSbSystemPlatformErrorTypeUserSignedOut:
      message = "User is not signed in.";
      break;
    case kSbSystemPlatformErrorTypeUserAgeRestricted:
      message = "User is age restricted.";
      break;
#endif
    default:
      message = "<unknown>";
      break;
  }
  SB_DLOG(INFO) << "SbSystemRaisePlatformError: " << message;
  return kSbSystemPlatformErrorInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbSystemPlatformErrorType</code><br>        <code>type</code></td>
    <td>An error type, from the SbSystemPlatformErrorType enum,
that defines the error.</td>
  </tr>
  <tr>
    <td><code>SbSystemPlatformErrorCallback</code><br>        <code>callback</code></td>
    <td>A function that may be called by the platform to let the caller
know that the user has reacted to the error.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>user_data</code></td>
    <td>An opaque pointer that the platform should pass as an argument
to the callback function, if it is called.</td>
  </tr>
</table>

### SbSystemRequestPause

**Description**

Requests that the application move into the Paused state at the next
convenient point. This should roughly correspond to "unfocused application"
in a traditional window manager, where the application may be partially
visible.<br>
This function eventually causes a `kSbEventTypePause` event to be dispatched
to the application. Before the `kSbEventTypePause` event is dispatched, some
work may continue to be done, and unrelated system events may be dispatched.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemRequestPause-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemRequestPause-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemRequestPause-declaration">
<pre>
SB_EXPORT void SbSystemRequestPause();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemRequestPause-stub">

```
#include "starboard/system.h"

void SbSystemRequestPause() {
}
```

  </div>
</div>

### SbSystemRequestStop

**Description**

Requests that the application be terminated gracefully at the next
convenient point. In the meantime, some work may continue to be done, and
unrelated system events may be dispatched. This function eventually causes
a `kSbEventTypeStop` event to be dispatched to the application. When the
process finally terminates, it returns `error_level`, if that has any
meaning on the current platform.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemRequestStop-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemRequestStop-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemRequestStop-declaration">
<pre>
SB_EXPORT void SbSystemRequestStop(int error_level);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemRequestStop-stub">

```
#include "starboard/system.h"

void SbSystemRequestStop(int /*error_level*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>error_level</code></td>
    <td>An integer that serves as the return value for the process
that is eventually terminated as a result of a call to this function.</td>
  </tr>
</table>

### SbSystemRequestSuspend

**Description**

Requests that the application move into the Suspended state at the next
convenient point. This should roughly correspond to "minimization" in a
traditional window manager, where the application is no longer visible.<br>
This function eventually causes a `kSbEventTypeSuspend` event to be
dispatched to the application. Before the `kSbEventTypeSuspend` event is
dispatched, some work may continue to be done, and unrelated system events
may be dispatched.<br>
In the Suspended state, the application will be resident, but probably not
running. The expectation is that an external system event will bring the
application out of the Suspended state.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemRequestSuspend-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemRequestSuspend-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemRequestSuspend-declaration">
<pre>
SB_EXPORT void SbSystemRequestSuspend();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemRequestSuspend-stub">

```
#include "starboard/system.h"

void SbSystemRequestSuspend() {
}
```

  </div>
</div>

### SbSystemRequestUnpause

**Description**

Requests that the application move into the Started state at the next
convenient point. This should roughly correspond to a "focused application"
in a traditional window manager, where the application is fully visible and
the primary receiver of input events.<br>
This function eventually causes a `kSbEventTypeUnpause` event to be
dispatched to the application. Before `kSbEventTypeUnpause` is dispatched,
some work may continue to be done, and unrelated system events may be
dispatched.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemRequestUnpause-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemRequestUnpause-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemRequestUnpause-declaration">
<pre>
SB_EXPORT void SbSystemRequestUnpause();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemRequestUnpause-stub">

```
#include "starboard/system.h"

void SbSystemRequestUnpause() {
}
```

  </div>
</div>

### SbSystemSort

**Description**

Sorts an array of elements `base`, with `element_count` elements of
`element_width` bytes each, using `comparator` as the comparison function.<br>
This function is meant to be a drop-in replacement for `qsort`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemSort-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemSort-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemSort-declaration">
<pre>
SB_EXPORT void SbSystemSort(void* base,
                            size_t element_count,
                            size_t element_width,
                            SbSystemComparator comparator);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemSort-stub">

```
#include "starboard/system.h"

void SbSystemSort(void* /*base*/,
                  size_t /*element_count*/,
                  size_t /*element_width*/,
                  SbSystemComparator /*comparator*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>        <code>base</code></td>
    <td>The array of elements to be sorted.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>element_count</code></td>
    <td>The number of elements in the array.</td>
  </tr>
  <tr>
    <td><code>size_t</code><br>        <code>element_width</code></td>
    <td>The size, in bytes, of each element in the array.</td>
  </tr>
  <tr>
    <td><code>SbSystemComparator</code><br>        <code>comparator</code></td>
    <td>A value that indicates how the array should be sorted.</td>
  </tr>
</table>

### SbSystemSymbolize

**Description**

Looks up `address` as an instruction pointer and places up to
(`buffer_size - 1`) characters of the symbol associated with it in
`out_buffer`, which must not be NULL. `out_buffer` will be NULL-terminated.<br>
The return value indicates whether the function found a reasonable match
for `address`. If the return value is `false`, then `out_buffer` is not
modified.<br>
This function is used in crash signal handlers and, therefore, it must
be async-signal-safe on platforms that support signals.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbSystemSymbolize-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbSystemSymbolize-stub" class="mdl-tabs__tab">stub</a>
    <a href="#SbSystemSymbolize-linux" class="mdl-tabs__tab">linux</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbSystemSymbolize-declaration">
<pre>
SB_EXPORT bool SbSystemSymbolize(const void* address,
                                 char* out_buffer,
                                 int buffer_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbSystemSymbolize-stub">

```
#include "starboard/system.h"

bool SbSystemSymbolize(const void* /*address*/, char* /*out_buffer*/,
                       int /*buffer_size*/) {
  return false;
}
```

  </div>
  <div class="mdl-tabs__panel" id="SbSystemSymbolize-linux">

```
#include "starboard/system.h"

#include "base/third_party/symbolize/symbolize.h"

bool SbSystemSymbolize(const void* address, char* out_buffer, int buffer_size) {
  // I believe this casting-away const in the implementation is better than the
  // alternative of removing const-ness from the address parameter.
  return google::Symbolize(const_cast<void*>(address), out_buffer, buffer_size);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const void*</code><br>
        <code>address</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>char*</code><br>
        <code>out_buffer</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>buffer_size</code></td>
    <td> </td>
  </tr>
</table>

