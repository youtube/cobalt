---
layout: doc
title: "Starboard Module Reference: system.h"
---

Defines a broad set of APIs that allow the client application to query build and
runtime properties of the enclosing system.

## Enums ##

### SbSystemCapabilityId ###

Runtime capabilities are boolean properties of a platform that can't be
determined at compile-time. They may vary from device to device, but they will
not change over the course of a single execution. They often specify particular
behavior of other APIs within the bounds of their operating range.

#### Values ####

*   `kSbSystemCapabilityReversedEnterAndBack`

    Whether this system has reversed Enter and Back keys.
*   `kSbSystemCapabilityCanQueryGPUMemoryStats`

    Whether this system has the ability to report on GPU memory usage. If (and
    only if) a system has this capability will SbSystemGetTotalGPUMemory() and
    SbSystemGetUsedGPUMemory() be valid to call.

### SbSystemConnectionType ###

Enumeration of network connection types.

#### Values ####

*   `kSbSystemConnectionTypeWired`

    The system is on a wired connection.
*   `kSbSystemConnectionTypeWireless`

    The system is on a wireless connection.
*   `kSbSystemConnectionTypeUnknown`

    The system connection type is unknown.

### SbSystemDeviceType ###

Enumeration of device types.

#### Values ####

*   `kSbSystemDeviceTypeBlueRayDiskPlayer`

    Blue-ray Disc Player (BDP).
*   `kSbSystemDeviceTypeGameConsole`

    A relatively high-powered TV device used primarily for playing games.
*   `kSbSystemDeviceTypeOverTheTopBox`

    Over the top (OTT) devices stream content via the Internet over another type
    of network, e.g. cable or satellite.
*   `kSbSystemDeviceTypeSetTopBox`

    Set top boxes (STBs) stream content primarily over cable or satellite. Some
    STBs can also stream OTT content via the Internet.
*   `kSbSystemDeviceTypeTV`

    A Smart TV is a TV that can directly run applications that stream OTT
    content via the Internet.
*   `kSbSystemDeviceTypeDesktopPC`

    Desktop PC.
*   `kSbSystemDeviceTypeAndroidTV`

    An Android TV Device.
*   `kSbSystemDeviceTypeUnknown`

    Unknown device.

### SbSystemPathId ###

Enumeration of special paths that the platform can define.

#### Values ####

*   `kSbSystemPathContentDirectory`

    Path to where the local content files that ship with the binary are
    available.
*   `kSbSystemPathCacheDirectory`

    Path to the directory that can be used as a local file cache, if available.
*   `kSbSystemPathDebugOutputDirectory`

    Path to the directory where debug output (e.g. logs, trace output,
    screenshots) can be written into.
*   `kSbSystemPathFontDirectory`

    Path to a directory where system font files can be found. Should only be
    specified on platforms that provide fonts usable by Starboard applications.
*   `kSbSystemPathFontConfigurationDirectory`

    Path to a directory where system font configuration metadata can be found.
    May be the same directory as `kSbSystemPathFontDirectory`, but not
    necessarily. Should only be specified on platforms that provide fonts usable
    by Starboard applications.
*   `kSbSystemPathTempDirectory`

    Path to a directory where temporary files can be written.
*   `kSbSystemPathTestOutputDirectory`

    Path to a directory where test results can be written.
*   `kSbSystemPathExecutableFile`

    Full path to the executable file.
*   `kSbSystemPathStorageDirectory`

    Path to a directory for permanent file storage. Both read and write access
    is required. This is where an app may store its persistent settings. The
    location should be user agnostic if possible.

### SbSystemPlatformErrorResponse ###

Possible responses for `SbSystemPlatformErrorCallback`.

#### Values ####

*   `kSbSystemPlatformErrorResponsePositive`
*   `kSbSystemPlatformErrorResponseNegative`
*   `kSbSystemPlatformErrorResponseCancel`

### SbSystemPlatformErrorType ###

Enumeration of possible values for the `type` parameter passed to the
`SbSystemRaisePlatformError` function.

#### Values ####

*   `kSbSystemPlatformErrorTypeConnectionError`

    Cobalt received a network connection error, or a network disconnection
    event. If the `response` passed to `SbSystemPlatformErrorCallback` is
    `kSbSystemPlatformErrorResponsePositive` then the request should be retried,
    otherwise the app should be stopped.

### SbSystemPropertyId ###

System properties that can be queried for. Many of these are used in User-Agent
string generation.

#### Values ####

*   `kSbSystemPropertyCertificationScope`

    The certification scope that identifies a group of devices.
*   `kSbSystemPropertyChipsetModelNumber`

    The full model number of the main platform chipset, including any vendor-
    specific prefixes.
*   `kSbSystemPropertyFirmwareVersion`

    The production firmware version number which the device is currently
    running.
*   `kSbSystemPropertyFriendlyName`

    A friendly name for this actual device. It may include user-personalization
    like "Upstairs Bedroom." It may be displayed to users as part of some kind
    of device selection (e.g. in-app DIAL).
*   `kSbSystemPropertyManufacturerName`

    A deprecated alias for `kSbSystemPropertyBrandName`.
*   `kSbSystemPropertyBrandName`

    The name of the brand under which the device is being sold.
*   `kSbSystemPropertyModelName`

    The final production model number of the device.
*   `kSbSystemPropertyModelYear`

    The year the device was launched, e.g. "2016".
*   `kSbSystemPropertySystemIntegratorName`

    The corporate entity responsible for submitting the device to YouTube
    certification and for the device maintenance/updates.
*   `kSbSystemPropertyPlatformName`

    The name of the operating system and platform, suitable for inclusion in a
    User-Agent, say.
*   `kSbSystemPropertySpeechApiKey`

    The Google Speech API key. The platform manufacturer is responsible for
    registering a Google Speech API key for their products. In the API Console (
    [http://developers.google.com/console](http://developers.google.com/console)
    ), you can enable the Speech APIs and generate a Speech API key.
*   `kSbSystemPropertyUserAgentAuxField`

    A field that, if available, is appended to the user agent

## Typedefs ##

### SbSystemComparator ###

Pointer to a function to compare two items. The return value uses standard
`*cmp` semantics:

*   `< 0` if `a` is less than `b`

*   `0` if the two items are equal

*   `> 1` if `a` is greater than `b`

`a`: The first value to compare. `b`: The second value to compare.

#### Definition ####

```
typedef int(* SbSystemComparator) (const void *a, const void *b)
```

### SbSystemError ###

A type that can represent a system error code across all Starboard platforms.

#### Definition ####

```
typedef int SbSystemError
```

### SbSystemPlatformErrorCallback ###

Type of callback function that may be called in response to an error
notification from `SbSystemRaisePlatformError`. `response` is a code to indicate
the user's response, e.g. if the platform raised a dialog to notify the user of
the error. `user_data` is the opaque pointer that was passed to the call to
`SbSystemRaisePlatformError`.

#### Definition ####

```
typedef void(* SbSystemPlatformErrorCallback) (SbSystemPlatformErrorResponse response, void *user_data)
```

## Functions ##

### SbSystemBreakIntoDebugger ###

Breaks the current program into the debugger, if a debugger is attached. If a
debugger is not attached, this function aborts the program.

#### Declaration ####

```
SB_NORETURN void SbSystemBreakIntoDebugger()
```

### SbSystemClearLastError ###

Clears the last error set by a Starboard call in the current thread.

#### Declaration ####

```
void SbSystemClearLastError()
```

### SbSystemGetConnectionType ###

Returns the device's current network connection type.

#### Declaration ####

```
SbSystemConnectionType SbSystemGetConnectionType()
```

### SbSystemGetDeviceType ###

Returns the type of the device.

#### Declaration ####

```
SbSystemDeviceType SbSystemGetDeviceType()
```

### SbSystemGetErrorString ###

Generates a human-readable string for an error. The return value specifies the
total desired length of the string.

`error`: The error for which a human-readable string is generated. `out_string`:
The generated string. This value may be null, and it is always terminated with a
null byte. `string_length`: The maximum length of the error string.

#### Declaration ####

```
int SbSystemGetErrorString(SbSystemError error, char *out_string, int string_length)
```

### SbSystemGetExtension ###

Returns pointer to a constant global struct implementing the extension named
`name`, if it is implemented. Otherwise return NULL.

Extensions are used to implement behavior which is specific to the combination
of application & platform. An extension relies on a header file in the
"extension" subdirectory of an app, which is used by both the application and
the Starboard platform to define an extension API struct. Since the header is
used both above and below Starboard, it cannot include any files from above
Starboard. It may depend on Starboard headers. That API struct has only 2
required fields which must be first: a const char* `name`, storing the extension
name, and a uint32_t `version` storing the version number of the extension. All
other fields may be C types (including custom structs) or function pointers. The
application will query for the function by name using SbSystemGetExtension, and
the platform returns a pointer to the singleton instance of the extension
struct. The singleton struct should be constant after initialization, since the
application may only get the extension once, meaning updated values would be
ignored. As the version of extensions are incremented, fields may be added to
the end of the struct, but never removed (only deprecated).

#### Declaration ####

```
const void* SbSystemGetExtension(const char *name)
```

### SbSystemGetLastError ###

Gets the last platform-specific error code produced by any Starboard call in the
current thread for diagnostic purposes. Semantic reactions to Starboard function
call results should be modeled explicitly.

#### Declaration ####

```
SbSystemError SbSystemGetLastError()
```

### SbSystemGetLocaleId ###

Gets the system's current POSIX-style Locale ID. The locale represents the
location, language, and cultural conventions that the system wants to use, which
affects which text is displayed to the user as well as how displayed numbers,
dates, currency, and similar values are formatted.

At its simplest, the locale ID can just be a BCP 47 language code, like `en_US`.
Currently, POSIX also wants to include the encoding as in `en_US.UTF8`. POSIX
also allows a couple very bare-bones locales, like "C" or "POSIX", but they are
not supported here. POSIX also supports different locale settings for a few
different purposes, but Starboard only exposes one locale at a time.

RFC 5646 describes BCP 47 language codes: [https://tools.ietf.org/html/bcp47](https://tools.ietf.org/html/bcp47)

For more information than you probably want about POSIX locales, see: [http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html)

#### Declaration ####

```
const char* SbSystemGetLocaleId()
```

### SbSystemGetNumberOfProcessors ###

Returns the number of processor cores available to this application. If the
process is sandboxed to a subset of the physical cores, the function returns
that sandboxed limit.

#### Declaration ####

```
int SbSystemGetNumberOfProcessors()
```

### SbSystemGetProperty ###

Retrieves the platform-defined system property specified by `property_id` and
places its value as a zero-terminated string into the user-allocated `out_value`
unless it is longer than `value_length` - 1. This implementation must be thread-
safe.

This function returns `true` if the property is retrieved successfully. It
returns `false` under any of the following conditions and, in any such case,
`out_value` is not changed:

*   `property_id` is invalid for this platform

*   `value_length` is too short for the given result

*   `out_value` is NULL

`property_id`: The system path to be retrieved. `out_value`: The platform-
defined system property specified by `property_id`. `value_length`: The length
of the system property.

#### Declaration ####

```
bool SbSystemGetProperty(SbSystemPropertyId property_id, char *out_value, int value_length)
```

### SbSystemGetRandomData ###

A cryptographically secure random number generator that produces an arbitrary,
non-negative number of `buffer_size` random, non-negative bytes. The generated
number is placed in `out_buffer`. This function does not require manual seeding.

`out_buffer`: A pointer for the generated random number. This value must not be
null. `buffer_size`: The size of the random number, in bytes.

#### Declaration ####

```
void SbSystemGetRandomData(void *out_buffer, int buffer_size)
```

### SbSystemGetRandomUInt64 ###

A cryptographically secure random number generator that gets 64 random bits and
returns them as an `uint64_t`. This function does not require manual seeding.

#### Declaration ####

```
uint64_t SbSystemGetRandomUInt64()
```

### SbSystemGetStack ###

Places up to `stack_size` instruction pointer addresses of the current execution
stack into `out_stack`. The return value specifies the number of entries added.

The returned stack frames are in "downward" order from the calling frame toward
the entry point of the thread. So, if all the stack frames do not fit, the ones
truncated will be the less interesting ones toward the thread entry point.

This function is used in crash signal handlers and, therefore, it must be async-
signal-safe on platforms that support signals. The following document discusses
what it means to be async-signal-safe on POSIX: [http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03](http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03)

`out_stack`: A non-NULL array of `void *` of at least `stack_size` entries.
`stack_size`: The maximum number of instruction pointer addresses to be placed
into `out_stack` from the current execution stack.

#### Declaration ####

```
int SbSystemGetStack(void **out_stack, int stack_size)
```

### SbSystemGetTotalCPUMemory ###

Returns the total CPU memory (in bytes) potentially available to this
application. If the process is sandboxed to a maximum allowable limit, the
function returns the lesser of the physical and sandbox limits.

#### Declaration ####

```
int64_t SbSystemGetTotalCPUMemory()
```

### SbSystemGetTotalGPUMemory ###

Returns the total GPU memory (in bytes) available for use by this application.
This function may only be called the return value for calls to
SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats) is `true`.

#### Declaration ####

```
int64_t SbSystemGetTotalGPUMemory()
```

### SbSystemGetUsedCPUMemory ###

Returns the total physical CPU memory (in bytes) used by this application. This
value should always be less than (or, in particularly exciting situations, equal
to) SbSystemGetTotalCPUMemory().

#### Declaration ####

```
int64_t SbSystemGetUsedCPUMemory()
```

### SbSystemGetUsedGPUMemory ###

Returns the current amount of GPU memory (in bytes) that is currently being used
by this application. This function may only be called if the return value for
calls to SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats) is
`true`.

#### Declaration ####

```
int64_t SbSystemGetUsedGPUMemory()
```

### SbSystemHasCapability ###

Returns whether the platform has the runtime capability specified by
`capability_id`. Returns false for any unknown capabilities. This implementation
must be thread-safe.

`capability_id`: The runtime capability to check.

#### Declaration ####

```
bool SbSystemHasCapability(SbSystemCapabilityId capability_id)
```

### SbSystemHideSplashScreen ###

Hides the system splash screen on systems that support a splash screen that is
displayed while the application is loading. This function may be called from any
thread and must be idempotent.

#### Declaration ####

```
void SbSystemHideSplashScreen()
```

### SbSystemIsDebuggerAttached ###

Attempts to determine whether the current program is running inside or attached
to a debugger. The function returns `false` if neither of those cases is true.

#### Declaration ####

```
bool SbSystemIsDebuggerAttached()
```

### SbSystemNetworkIsDisconnected ###

Returns if the device is disconnected from network. "Disconnected" is chosen
over connected because disconnection can be determined with more certainty than
connection usually.

#### Declaration ####

```
bool SbSystemNetworkIsDisconnected()
```

### SbSystemRaisePlatformError ###

Cobalt calls this function to notify the platform that an error has occurred in
the application that the platform may need to handle. The platform is expected
to then notify the user of the error and to provide a means for any required
interaction, such as by showing a dialog.

The return value is a boolean. If the platform cannot respond to the error, then
this function should return `false`, otherwise it should return `true`.

This function may be called from any thread, and it is the platform's
responsibility to decide how to handle an error received while a previous error
is still pending. If that platform can only handle one error at a time, then it
may queue the second error or ignore it by returning
`kSbSystemPlatformErrorInvalid`.

`type`: An error type, from the SbSystemPlatformErrorType enum, that defines the
error. `callback`: A function that may be called by the platform to let the
caller know that the user has reacted to the error. `user_data`: An opaque
pointer that the platform should pass as an argument to the callback function,
if it is called.

#### Declaration ####

```
bool SbSystemRaisePlatformError(SbSystemPlatformErrorType type, SbSystemPlatformErrorCallback callback, void *user_data)
```

### SbSystemRequestBlur ###

Requests that the application move into the Blurred state at the next convenient
point. This should roughly correspond to "unfocused application" in a
traditional window manager, where the application may be partially visible.

This function eventually causes a `kSbEventTypeBlur` event to be dispatched to
the application. Before the `kSbEventTypeBlur` event is dispatched, some work
may continue to be done, and unrelated system events may be dispatched.

#### Declaration ####

```
void SbSystemRequestBlur()
```

### SbSystemRequestConceal ###

Requests that the application move into the Concealed state at the next
convenient point. This should roughly correspond to "minimization" in a
traditional window manager, where the application is no longer visible. However,
the background tasks can still be running.

This function eventually causes a `kSbEventTypeConceal` event to be dispatched
to the application. Before the `kSbEventTypeConceal` event is dispatched, some
work may continue to be done, and unrelated system events may be dispatched.

In the Concealed state, the application will be invisible, but probably still be
running background tasks. The expectation is that an external system event will
bring the application out of the Concealed state.

#### Declaration ####

```
void SbSystemRequestConceal()
```

### SbSystemRequestFocus ###

Requests that the application move into the Started state at the next convenient
point. This should roughly correspond to a "focused application" in a
traditional window manager, where the application is fully visible and the
primary receiver of input events.

This function eventually causes a `kSbEventTypeFocus` event to be dispatched to
the application. Before `kSbEventTypeFocus` is dispatched, some work may
continue to be done, and unrelated system events may be dispatched.

#### Declaration ####

```
void SbSystemRequestFocus()
```

### SbSystemRequestFreeze ###

Requests that the application move into the Frozen state at the next convenient
point.

This function eventually causes a `kSbEventTypeFreeze` event to be dispatched to
the application. Before the `kSbEventTypeSuspend` event is dispatched, some work
may continue to be done, and unrelated system events may be dispatched.

In the Frozen state, the application will be resident, but probably not running.
The expectation is that an external system event will bring the application out
of the Frozen state.

#### Declaration ####

```
void SbSystemRequestFreeze()
```

### SbSystemRequestReveal ###

Requests that the application move into the Blurred state at the next convenient
point. This should roughly correspond to a "focused application" in a
traditional window manager, where the application is fully visible and the
primary receiver of input events.

This function eventually causes a `kSbEventTypeReveal` event to be dispatched to
the application. Before the `kSbEventTypeReveal` event is dispatched, some work
may continue to be done, and unrelated system events may be dispatched.

#### Declaration ####

```
void SbSystemRequestReveal()
```

### SbSystemRequestStop ###

Requests that the application be terminated gracefully at the next convenient
point. In the meantime, some work may continue to be done, and unrelated system
events may be dispatched. This function eventually causes a `kSbEventTypeStop`
event to be dispatched to the application. When the process finally terminates,
it returns `error_level`, if that has any meaning on the current platform.

`error_level`: An integer that serves as the return value for the process that
is eventually terminated as a result of a call to this function.

#### Declaration ####

```
void SbSystemRequestStop(int error_level)
```

### SbSystemSignWithCertificationSecretKey ###

Computes a HMAC-SHA256 digest of `message` into `digest` using the application's
certification secret.

This function may be implemented as an alternative to implementing
SbSystemGetProperty(kSbSystemPropertyBase64EncodedCertificationSecret), however
both should not be implemented.

The output will be written into `digest`. `digest_size_in_bytes` must be 32 (or
greater), since 32-bytes will be written into it. Returns false in the case of
an error, or if it is not implemented. In this case the contents of `digest`
will be undefined.

#### Declaration ####

```
bool SbSystemSignWithCertificationSecretKey(const uint8_t *message, size_t message_size_in_bytes, uint8_t *digest, size_t digest_size_in_bytes)
```

### SbSystemSupportsResume ###

Returns false if the platform doesn't need resume after suspend support. In such
case Cobalt will free up the resource it retains for resume after suspend. Note
that if this function returns false, the Starboard implementation cannot send
kSbEventTypeResume to the event handler. The return value of this function
cannot change over the life time of the application.

#### Declaration ####

```
bool SbSystemSupportsResume()
```

### SbSystemSymbolize ###

Looks up `address` as an instruction pointer and places up to (`buffer_size -
1`) characters of the symbol associated with it in `out_buffer`, which must not
be NULL. `out_buffer` will be NULL-terminated.

The return value indicates whether the function found a reasonable match for
`address`. If the return value is `false`, then `out_buffer` is not modified.

This function is used in crash signal handlers and, therefore, it must be async-
signal-safe on platforms that support signals.

#### Declaration ####

```
bool SbSystemSymbolize(const void *address, char *out_buffer, int buffer_size)
```

