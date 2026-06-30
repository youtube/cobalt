Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `system.h`

Defines APIs that allow client applications to query build and runtime
properties of the host system.

## Enums

### SbSystemCapabilityId

Runtime capabilities are boolean platform properties that cannot be determined
at compile time. These properties can vary between devices but remain constant
during a single execution. They often dictate specific behaviors of other APIs.

#### Values

*   `kSbSystemCapabilityReversedEnterAndBack`

    Whether this system has reversed Enter and Back keys.
*   `kSbSystemCapabilityCanQueryGPUMemoryStats`

    Indicates whether the system can report GPU memory usage. You can only call
    `SbSystemGetTotalGPUMemory()` and `SbSystemGetUsedGPUMemory()` if this
    capability is present.

### SbSystemDeviceType

Enumeration of device types.

#### Values

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
*   `kSbSystemDeviceTypeVideoProjector`

    A wall video projector.
*   `kSbSystemDeviceTypeUnknown`

    Unknown device.

### SbSystemPathId

Enumerates special paths that the platform can define.

#### Values

*   `kSbSystemPathContentDirectory`

    Path to where the local content files that ship with the binary are
    available.
*   `kSbSystemPathCacheDirectory`

    Path to the directory that can be used as a local file cache, if available.
*   `kSbSystemPathDebugOutputDirectory`

    Path to the directory where debug output (e.g. logs, trace output,
    screenshots) can be written into.
*   `kSbSystemPathFontDirectory`

    Path to the directory containing system font files. Specify this path only
    on platforms that provide fonts usable by Starboard applications.
*   `kSbSystemPathFontConfigurationDirectory`

    Path to the directory containing system font configuration metadata. This
    can be the same directory as `kSbSystemPathFontDirectory`. Specify this path
    only on platforms that provide fonts usable by Starboard applications.
*   `kSbSystemPathTempDirectory`

    Path to a directory where temporary files can be written.
*   `kSbSystemPathExecutableFile`

    Full path to the executable file.
*   `kSbSystemPathStorageDirectory`

    Path to the directory dedicated to Evergreen permanent file storage.
    Requires both read and write access. Use this directory only to store
    updates. See `starboard/doc/evergreen/cobalt_evergreen_overview.md`
*   `kSbSystemPathFilesDirectory`

    Path to the directory for permanent files. Used for cookies and
    localStorage.

### SbSystemPlatformErrorResponse

Defines possible responses for `SbSystemPlatformErrorCallback`.

#### Values

*   `kSbSystemPlatformErrorResponsePositive`
*   `kSbSystemPlatformErrorResponseNegative`
*   `kSbSystemPlatformErrorResponseCancel`

### SbSystemPlatformErrorType

Enumerates possible values for the `type` parameter of
`SbSystemRaisePlatformError`.

#### Values

*   `kSbSystemPlatformErrorTypeConnectionError`

    Sent when Cobalt receives a network connection error or a network
    disconnection event. If the `response` passed to
    `SbSystemPlatformErrorCallback` is `kSbSystemPlatformErrorResponsePositive`,
    retry the request; otherwise, stop the application.

### SbSystemPropertyId

System properties that can be queried. Many of these are used to generate the
User-Agent string.

#### Values

*   `kSbSystemPropertyCertificationScope`

    The certification scope that identifies a group of devices.
*   `kSbSystemPropertyChipsetModelNumber`

    The full model number of the main platform chipset, including any vendor-
    specific prefixes.
*   `kSbSystemPropertyFirmwareVersion`

    The production firmware version number which the device is currently
    running.
*   `kSbSystemPropertyFriendlyName`

    A user-friendly name for the device, which may include personalization (such
    as "Upstairs Bedroom"). It can be displayed to users during device selection
    (for example, in-app DIAL).
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

    The Google Speech API key. Platform manufacturers must register a Google
    Speech API key for their products. You can enable the Speech APIs and
    generate a key in the [Google API Console](http://developers.google.com/console).
*   `kSbSystemPropertyUserAgentAuxField`

    A field that, if available, is appended to the User-Agent.
*   `kSbSystemPropertyAdvertisingId`

    The Advertising ID or Identifier for Advertising (IFA), typically a 128-bit
    UUID. See [IAB Tech Lab](https://iabtechlab.com/OTT-IFA) for details. This
    corresponds to the `ifa` field. Note: the `ifa_type` field is not provided.
*   `kSbSystemPropertyLimitAdTracking`

    Limits advertising tracking, treated as a boolean (non-zero indicates true).
    Corresponds to the `lmt` field.
*   `kSbSystemPropertyDeviceType`

    The device type, such as "TV", "STB", or "OTT". See the YouTube Technical
    Requirements for a full list of allowed values.

## Typedefs

### SbSystemComparator

Pointer to a function that compares two items. The return value uses standard
`strcmp` -like semantics:

*   `< 0` if `a` is less than `b`

*   `0` if the two items are equal

*   `> 1` if `a` is greater than `b`

*   `a`: The first value to compare.

*   `b`: The second value to compare.

#### Definition

```
typedef int(* SbSystemComparator) (const void *a, const void *b)
```

### SbSystemError

A type that can represent a system error code across all Starboard platforms.

#### Definition

```
typedef int SbSystemError
```

### SbSystemPlatformErrorCallback

Callback function invoked in response to an error notification from
`SbSystemRaisePlatformError`. The `response` parameter indicates the user's
action (for example, if they dismissed a dialog). The `user_data` parameter is
the opaque pointer passed to `SbSystemRaisePlatformError`.

#### Definition

```
typedef void(* SbSystemPlatformErrorCallback) (SbSystemPlatformErrorResponse response, void *user_data)
```

## Functions

### SbSystemBreakIntoDebugger

Breaks the program into the debugger if one is attached; otherwise, aborts the
program.

#### Declaration

```
SB_NORETURN void SbSystemBreakIntoDebugger()
```

### SbSystemClearLastError

Clears the last error set by a Starboard call in the current thread.

#### Declaration

```
void SbSystemClearLastError()
```

### SbSystemGetErrorString

Generates a human-readable string for an error. The function returns the total
length of the generated string.

*   `error`: The error for which to generate a string.

*   `out_string`: The destination buffer for the generated string. This can be
    `NULL`. The output is always null-terminated.

*   `string_length`: The maximum length of the error string.

#### Declaration

```
int SbSystemGetErrorString(SbSystemError error, char *out_string, int string_length)
```

### SbSystemGetExtension

Returns a pointer to the constant global structure implementing the extension
specified by `name`. If the extension is not implemented, the function returns
`NULL`. The `name` parameter must not be `NULL`.

Extensions implement behaviors specific to a particular application and platform
combination. They rely on a header file in the application's `extension/`
subdirectory to define the extension API structure. Because this header is used
both by the application and the Starboard platform, it must not include any
application-level headers. It can, however, depend on Starboard headers.

The API structure must begin with two required fields: a `const char* name`
(storing the extension name) and a `uint32_t version` (storing the extension
version). Remaining fields can be C types (including custom structures) or
function pointers.

The application queries for the extension by name using `SbSystemGetExtension`,
and the platform returns a pointer to a singleton structure instance. This
singleton structure must remain constant after initialization. When incrementing
the extension version, you can append fields to the structure, but you must
never remove existing fields (mark them as deprecated instead).

#### Declaration

```
const void * SbSystemGetExtension(const char *name)
```

### SbSystemGetLastError

Gets the last platform-specific error code produced by any Starboard call in the
current thread for diagnostic purposes. Do not use this error code to control
program flow; handle function return values directly instead.

#### Declaration

```
SbSystemError SbSystemGetLastError()
```

### SbSystemGetLocaleId

Gets the system's current POSIX-style locale ID. The locale represents the
location, language, and cultural conventions of the system, which determine how
text is displayed and how numbers, dates, and currency are formatted.

At its simplest, the locale ID can be a BCP 47 language code, such as `en_US`.
POSIX also includes the encoding (for example, `en_US.UTF8`). POSIX allows basic
locales like "C" or "POSIX", but Starboard does not support them. While POSIX
supports different locale settings for various purposes, Starboard exposes only
one locale at a time.

RFC 5646 describes BCP 47 language codes: [https://tools.ietf.org/html/bcp47](https://tools.ietf.org/html/bcp47)

For more information than you probably want about POSIX locales, see: [http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html)

#### Declaration

```
const char * SbSystemGetLocaleId()
```

### SbSystemGetNumberOfProcessors

Returns the number of processor cores available to this application. If the
process is sandboxed to a subset of the physical cores, the function returns
that sandboxed limit.

#### Declaration

```
int SbSystemGetNumberOfProcessors()
```

### SbSystemGetProperty

Retrieves the platform-defined system property specified by `property_id` and
places its value as a null-terminated string into the user-allocated `out_value`
(unless the value is longer than `value_length` - 1). This implementation must
be thread-safe.

This function returns `true` if the property is retrieved successfully. It
returns `false` under any of the following conditions, leaving `out_value`
unchanged:

*   `property_id` is invalid for this platform

*   `value_length` is too short for the given result

*   `out_value` is NULL

*   `property_id`: The system property to retrieve.

*   `out_value`: The destination buffer for the property value.

*   `value_length`: The length of the destination buffer.

#### Declaration

```
bool SbSystemGetProperty(SbSystemPropertyId property_id, char *out_value, int value_length)
```

### SbSystemGetRandomData

A cryptographically secure random number generator that generates `buffer_size`
random bytes and places them in `out_buffer`. This function does not require
manual seeding.

*   `out_buffer`: The destination buffer for the generated random bytes. Must
    not be `NULL`.

*   `buffer_size`: The number of random bytes to generate.

#### Declaration

```
void SbSystemGetRandomData(void *out_buffer, int buffer_size)
```

### SbSystemGetRandomUInt64

A cryptographically secure random number generator that generates 64 random bits
and returns them as a `uint64_t`. This function does not require manual seeding.

#### Declaration

```
uint64_t SbSystemGetRandomUInt64()
```

### SbSystemGetStack

Places up to `stack_size` instruction pointer addresses of the current execution
stack into `out_stack`. Returns the number of entries added.

The returned stack frames are in downward order from the calling frame toward
the thread's entry point. If the buffer is too small, frames near the thread
entry point are truncated.

This function is used in crash signal handlers and, therefore, it must be
async-signal-safe on platforms that support signals. The following document
discusses what it means to be async-signal-safe on POSIX: [http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03](http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03)

*   `out_stack`: A buffer to store the instruction pointer addresses. Must not
    be `NULL` and must hold at least `stack_size` entries.

*   `stack_size`: The maximum number of instruction pointer addresses to
    retrieve.

#### Declaration

```
int SbSystemGetStack(void **out_stack, int stack_size)
```

### SbSystemGetTotalCPUMemory

Returns the total CPU memory (in bytes) potentially available to this
application. If the process is sandboxed to a maximum allowable limit, the
function returns the lesser of the physical and sandbox limits.

#### Declaration

```
int64_t SbSystemGetTotalCPUMemory()
```

### SbSystemGetTotalGPUMemory

Returns the total GPU memory (in bytes) available to the application. This
function can only be called if
`SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)` returns
`true`.

#### Declaration

```
int64_t SbSystemGetTotalGPUMemory()
```

### SbSystemGetUsedCPUMemory

Returns the total physical CPU memory (in bytes) used by this application. This
value should always be less than or equal to `SbSystemGetTotalCPUMemory()`.

#### Declaration

```
int64_t SbSystemGetUsedCPUMemory()
```

### SbSystemGetUsedGPUMemory

Returns the amount of GPU memory (in bytes) used by the application. This
function can only be called if
`SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)` returns
`true`.

#### Declaration

```
int64_t SbSystemGetUsedGPUMemory()
```

### SbSystemHasCapability

Returns whether the platform supports the runtime capability specified by
`capability_id`. Returns `false` for unknown capabilities. This implementation
must be thread-safe.

*   `capability_id`: The runtime capability to check.

#### Declaration

```
bool SbSystemHasCapability(SbSystemCapabilityId capability_id)
```

### SbSystemHideSplashScreen

Hides the splash screen shown while the application loads. This function can be
called from any thread and must be idempotent.

#### Declaration

```
void SbSystemHideSplashScreen()
```

### SbSystemIsDebuggerAttached

Determines whether the program is running inside or attached to a debugger.
Returns `false` otherwise.

#### Declaration

```
bool SbSystemIsDebuggerAttached()
```

### SbSystemNetworkIsDisconnected

Returns whether the device is disconnected from the network. Disconnection is
parsed instead of connection because it can typically be determined with more
certainty.

#### Declaration

```
bool SbSystemNetworkIsDisconnected()
```

### SbSystemRaisePlatformError

Called by Cobalt to notify the platform of an application error that may require
platform handling. The platform should notify the user and provide necessary
interaction (for example, by showing a dialog).

Returns `true` if the platform handles the error; otherwise, returns `false`.

This function can be called from any thread. The platform must decide how to
handle concurrent errors. If it can only handle one error at a time, it can
queue or ignore subsequent errors.

*   `type`: The error type.

*   `callback`: A callback function invoked when the user responds to the error.

*   `user_data`: An opaque pointer passed to the callback function.

#### Declaration

```
bool SbSystemRaisePlatformError(SbSystemPlatformErrorType type, SbSystemPlatformErrorCallback callback, void *user_data)
```

### SbSystemRequestBlur

Requests that the application transition to the `BLURRED` state. This
corresponds to an unfocused application in a traditional window manager, where
it may remain partially visible.

This function triggers a `kSbEventTypeBlur` event. Before the event is
dispatched, the application may continue working and receive other system
events.

#### Declaration

```
void SbSystemRequestBlur()
```

### SbSystemRequestConceal

Requests that the application transition to the `CONCEALED` state. This
corresponds to minimizing the application, where it is no longer visible but
background tasks can continue running.

This function triggers a `kSbEventTypeConceal` event. Before the event is
dispatched, the application may continue working and receive other system
events.

In the `CONCEALED` state, the application is invisible but may continue running
background tasks. An external system event is expected to restore the
application.

#### Declaration

```
void SbSystemRequestConceal()
```

### SbSystemRequestFocus

Requests that the application transition to the `STARTED` (focused) state. This
corresponds to focusing the application, making it fully visible and ready to
receive input.

This function triggers a `kSbEventTypeFocus` event. Before the event is
dispatched, the application may continue working and receive other system
events.

#### Declaration

```
void SbSystemRequestFocus()
```

### SbSystemRequestFreeze

Requests that the application transition to the `FROZEN` state.

This function triggers a `kSbEventTypeFreeze` event. Before the event is
dispatched, the application may continue working and receive other system
events.

In the `FROZEN` state, the application remains in memory but does not run. An
external system event is expected to resume the application.

#### Declaration

```
void SbSystemRequestFreeze()
```

### SbSystemRequestReveal

Requests that the application transition to the `BLURRED` state. This
corresponds to revealing the application, making it visible again after being
concealed.

This function triggers a `kSbEventTypeReveal` event. Before the event is
dispatched, the application may continue working and receive other system
events.

#### Declaration

```
void SbSystemRequestReveal()
```

### SbSystemRequestStop

Requests that the application terminate gracefully. Before termination, the
application may continue working and receive other system events. This function
triggers a `kSbEventTypeStop` event. When the process terminates, it returns
`error_level` (if supported by the platform).

*   `error_level`: An integer that serves as the return value for the process
    that is eventually terminated as a result of a call to this function.

#### Declaration

```
void SbSystemRequestStop(int error_level)
```

### SbSystemSignWithCertificationSecretKey

Computes a HMAC-SHA256 digest of `message` using the application's certification
secret, and writes it to `digest`. The `message` and `digest` pointers must not
be `NULL`.

The `digest_size_in_bytes` parameter must be at least 32. Returns `false` if an
error occurs or if the function is not implemented; in these cases, the contents
of `digest` are undefined.

#### Declaration

```
bool SbSystemSignWithCertificationSecretKey(const uint8_t *message, size_t message_size_in_bytes, uint8_t *digest, size_t digest_size_in_bytes)
```

### SbSystemSupportsResume

Returns `false` if the platform does not support resuming after suspension. In
this case, Cobalt frees resources retained for resumption, and the platform must
not send `kSbEventTypeResume` events. The return value must remain constant for
the lifetime of the application.

#### Declaration

```
bool SbSystemSupportsResume()
```

### SbSystemSymbolize

Looks up `address` as an instruction pointer and places up to `buffer_size - 1`
characters of its associated symbol in `out_buffer`. The output is always
null-terminated. `out_buffer` must not be `NULL`.

The return value indicates whether the function found a reasonable match for
`address`. If the return value is `false`, then `out_buffer` is not modified.

This function is used in crash signal handlers and, therefore, it must be
async-signal-safe on platforms that support signals.

#### Declaration

```
bool SbSystemSymbolize(const void *address, char *out_buffer, int buffer_size)
```
