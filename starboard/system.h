// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Starboard System module
//
// Defines APIs that allow client applications to query build and runtime
// properties of the host system.

#ifndef STARBOARD_SYSTEM_H_
#define STARBOARD_SYSTEM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// A type that can represent a system error code across all Starboard platforms.
typedef int SbSystemError;

// Enumerates special paths that the platform can define.
typedef enum SbSystemPathId {
  // Path to where the local content files that ship with the binary are
  // available.
  kSbSystemPathContentDirectory,

  // Path to the directory that can be used as a local file cache, if available.
  kSbSystemPathCacheDirectory,

  // Path to the directory where debug output (e.g. logs, trace output,
  // screenshots) can be written into.
  kSbSystemPathDebugOutputDirectory,

  // Path to the directory containing system font files. Specify this path only
  // on platforms that provide fonts usable by Starboard applications.
  kSbSystemPathFontDirectory,

  // Path to the directory containing system font configuration metadata. This
  // can be the same directory as |kSbSystemPathFontDirectory|. Specify this
  // path only on platforms that provide fonts usable by Starboard applications.
  kSbSystemPathFontConfigurationDirectory,

  // Path to a directory where temporary files can be written.
  kSbSystemPathTempDirectory,

  // Full path to the executable file.
  kSbSystemPathExecutableFile,

  // Path to the directory dedicated to Evergreen permanent file storage.
  // Requires both read and write access. Use this directory only to store
  // updates. See `starboard/doc/evergreen/cobalt_evergreen_overview.md`
  kSbSystemPathStorageDirectory,

  // Path to the directory for permanent files. Used for cookies and
  // localStorage.
  kSbSystemPathFilesDirectory,
} SbSystemPathId;

// System properties that can be queried. Many of these are used to generate the
// User-Agent string.
typedef enum SbSystemPropertyId {
  // The certification scope that identifies a group of devices.
  kSbSystemPropertyCertificationScope,

  // The full model number of the main platform chipset, including any vendor-
  // specific prefixes.
  kSbSystemPropertyChipsetModelNumber,

  // The production firmware version number which the device is currently
  // running.
  kSbSystemPropertyFirmwareVersion,

  // A user-friendly name for the device, which may include personalization
  // (such as "Upstairs Bedroom"). It can be displayed to users during device
  // selection (for example, in-app DIAL).
  kSbSystemPropertyFriendlyName,

  // A deprecated alias for |kSbSystemPropertyBrandName|.
  kSbSystemPropertyManufacturerName,

  // The name of the brand under which the device is being sold.
  kSbSystemPropertyBrandName = kSbSystemPropertyManufacturerName,

  // The final production model number of the device.
  kSbSystemPropertyModelName,

  // The year the device was launched, e.g. "2016".
  kSbSystemPropertyModelYear,

  // The corporate entity responsible for submitting the device to YouTube
  // certification and for the device maintenance/updates.
  kSbSystemPropertySystemIntegratorName,

  // The name of the operating system and platform, suitable for inclusion in a
  // User-Agent, say.
  kSbSystemPropertyPlatformName,

  // The Google Speech API key. Platform manufacturers must register a Google
  // Speech API key for their products. You can enable the Speech APIs and
  // generate a key in the [Google API
  // Console](http://developers.google.com/console).
  kSbSystemPropertySpeechApiKey,

  // A field that, if available, is appended to the User-Agent.
  kSbSystemPropertyUserAgentAuxField,

  // The Advertising ID or Identifier for Advertising (IFA), typically a 128-bit
  // UUID. See [IAB Tech Lab](https://iabtechlab.com/OTT-IFA) for details. This
  // corresponds to the `ifa` field. Note: the `ifa_type` field is not provided.
  kSbSystemPropertyAdvertisingId,

  // Limits advertising tracking, treated as a boolean (non-zero indicates
  // true). Corresponds to the `lmt` field.
  kSbSystemPropertyLimitAdTracking,

  // The device type, such as "TV", "STB", or "OTT". See the YouTube Technical
  // Requirements for a full list of allowed values.
  kSbSystemPropertyDeviceType,
} SbSystemPropertyId;

// Enumeration of device types.
typedef enum SbSystemDeviceType {
  // Blue-ray Disc Player (BDP).
  kSbSystemDeviceTypeBlueRayDiskPlayer,

  // A relatively high-powered TV device used primarily for playing games.
  kSbSystemDeviceTypeGameConsole,

  // Over the top (OTT) devices stream content via the Internet over another
  // type of network, e.g. cable or satellite.
  kSbSystemDeviceTypeOverTheTopBox,

  // Set top boxes (STBs) stream content primarily over cable or satellite. Some
  // STBs can also stream OTT content via the Internet.
  kSbSystemDeviceTypeSetTopBox,

  // A Smart TV is a TV that can directly run applications that stream OTT
  // content via the Internet.
  kSbSystemDeviceTypeTV,

  // Desktop PC.
  kSbSystemDeviceTypeDesktopPC,

  // An Android TV Device.
  kSbSystemDeviceTypeAndroidTV,

  // A wall video projector.
  kSbSystemDeviceTypeVideoProjector,

  // Unknown device.
  kSbSystemDeviceTypeUnknown,
} SbSystemDeviceType;

// Runtime capabilities are boolean platform properties that cannot be
// determined at compile time. These properties can vary between devices but
// remain constant during a single execution. They often dictate specific
// behaviors of other APIs.
typedef enum SbSystemCapabilityId {
  // Whether this system has reversed Enter and Back keys.
  kSbSystemCapabilityReversedEnterAndBack,

  // Indicates whether the system can report GPU memory usage. You can only call
  // `SbSystemGetTotalGPUMemory()` and `SbSystemGetUsedGPUMemory()` if this
  // capability is present.
  kSbSystemCapabilityCanQueryGPUMemoryStats,

  // ATTENTION: Do not add more to this enum. Instead add an "IsSupported"
  // function in the relevant module.
} SbSystemCapabilityId;

// Enumerates possible values for the |type| parameter of
// |SbSystemRaisePlatformError|.
typedef enum SbSystemPlatformErrorType {
  // Sent when Cobalt receives a network connection error or a network
  // disconnection event. If the |response| passed to
  // |SbSystemPlatformErrorCallback| is
  // |kSbSystemPlatformErrorResponsePositive|, retry the request; otherwise,
  // stop the application.
  kSbSystemPlatformErrorTypeConnectionError,
} SbSystemPlatformErrorType;

// Defines possible responses for |SbSystemPlatformErrorCallback|.
typedef enum SbSystemPlatformErrorResponse {
  kSbSystemPlatformErrorResponsePositive,
  kSbSystemPlatformErrorResponseNegative,
  kSbSystemPlatformErrorResponseCancel
} SbSystemPlatformErrorResponse;

// Callback function invoked in response to an error notification from
// |SbSystemRaisePlatformError|. The |response| parameter indicates the user's
// action (for example, if they dismissed a dialog). The |user_data| parameter
// is the opaque pointer passed to |SbSystemRaisePlatformError|.
typedef void (*SbSystemPlatformErrorCallback)(
    SbSystemPlatformErrorResponse response,
    void* user_data);

// Private structure used to represent a raised platform error.
typedef struct SbSystemPlatformErrorPrivate SbSystemPlatformErrorPrivate;

// Called by Cobalt to notify the platform of an application error that may
// require platform handling. The platform should notify the user and provide
// necessary interaction (for example, by showing a dialog).
//
// Returns `true` if the platform handles the error; otherwise, returns `false`.
//
// This function can be called from any thread. The platform must decide how to
// handle concurrent errors. If it can only handle one error at a time, it can
// queue or ignore subsequent errors.
//
// * |type|: The error type.
// * |callback|: A callback function invoked when the user responds to the
//   error.
// * |user_data|: An opaque pointer passed to the callback function.
SB_EXPORT bool SbSystemRaisePlatformError(
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data);

// Pointer to a function that compares two items. The return value uses standard
// `strcmp`-like semantics:
//
// * `< 0` if |a| is less than |b|
// * `0` if the two items are equal
// * `> 1` if |a| is greater than |b|
// * |a|: The first value to compare.
// * |b|: The second value to compare.
typedef int (*SbSystemComparator)(const void* a, const void* b);

// Breaks the program into the debugger if one is attached; otherwise, aborts
// the program.
SB_NORETURN SB_EXPORT void SbSystemBreakIntoDebugger();

// Determines whether the program is running inside or attached to a debugger.
// Returns `false` otherwise.
SB_EXPORT bool SbSystemIsDebuggerAttached();

// Returns the number of processor cores available to this application. If the
// process is sandboxed to a subset of the physical cores, the function returns
// that sandboxed limit.
SB_EXPORT int SbSystemGetNumberOfProcessors();

// Returns the total CPU memory (in bytes) potentially available to this
// application. If the process is sandboxed to a maximum allowable limit, the
// function returns the lesser of the physical and sandbox limits.
SB_EXPORT int64_t SbSystemGetTotalCPUMemory();

// Returns the total physical CPU memory (in bytes) used by this application.
// This value should always be less than or equal to
// `SbSystemGetTotalCPUMemory()`.
SB_EXPORT int64_t SbSystemGetUsedCPUMemory();

// Returns the total GPU memory (in bytes) available to the application. This
// function can only be called if
// `SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)` returns
// `true`.
SB_EXPORT int64_t SbSystemGetTotalGPUMemory();

// Returns the amount of GPU memory (in bytes) used by the application. This
// function can only be called if
// `SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)` returns
// `true`.
SB_EXPORT int64_t SbSystemGetUsedGPUMemory();

// Returns whether the device is disconnected from the network. Disconnection is
// parsed instead of connection because it can typically be determined with more
// certainty.
SB_EXPORT bool SbSystemNetworkIsDisconnected();

// Retrieves the platform-defined system path specified by |path_id| and
// places it as a zero-terminated string into the user-allocated |out_path|
// unless it is longer than |path_length| - 1. This implementation must be
// thread-safe.
//
// This function returns |true| if the path is retrieved successfully. It
// returns |false| under any of the following conditions and, in any such
// case, |out_path| is not changed:
// - |path_id| is invalid for this platform
// - |path_length| is too short for the given result
// - |out_path| is NULL
//
// |path_id|: The system path to be retrieved.
// |out_path|: The platform-defined system path specified by |path_id|.
// |path_length|: The length of the system path.

SB_EXPORT bool SbSystemGetPath(SbSystemPathId path_id,
                               char* out_path,
                               int path_length);

// Retrieves the platform-defined system property specified by |property_id| and
// places its value as a null-terminated string into the user-allocated
// |out_value| (unless the value is longer than |value_length| - 1). This
// implementation must be thread-safe.
//
// This function returns `true` if the property is retrieved successfully. It
// returns `false` under any of the following conditions, leaving |out_value|
// unchanged:
//
// * |property_id| is invalid for this platform
// * |value_length| is too short for the given result
// * |out_value| is NULL
// * |property_id|: The system property to retrieve.
// * |out_value|: The destination buffer for the property value.
// * |value_length|: The length of the destination buffer.
SB_EXPORT bool SbSystemGetProperty(SbSystemPropertyId property_id,
                                   char* out_value,
                                   int value_length);

// Returns whether the platform supports the runtime capability specified by
// |capability_id|. Returns `false` for unknown capabilities. This
// implementation must be thread-safe.
//
// * |capability_id|: The runtime capability to check.
SB_EXPORT bool SbSystemHasCapability(SbSystemCapabilityId capability_id);

// Gets the system's current POSIX-style locale ID. The locale represents the
// location, language, and cultural conventions of the system, which determine
// how text is displayed and how numbers, dates, and currency are formatted.
//
// At its simplest, the locale ID can be a BCP 47 language code, such as
// `en_US`. POSIX also includes the encoding (for example, `en_US.UTF8`). POSIX
// allows basic locales like "C" or "POSIX", but Starboard does not support
// them. While POSIX supports different locale settings for various purposes,
// Starboard exposes only one locale at a time.
//
// RFC 5646 describes BCP 47 language codes:
// https://tools.ietf.org/html/bcp47
//
// For more information than you probably want about POSIX locales, see:
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html
SB_EXPORT const char* SbSystemGetLocaleId();

// A cryptographically secure random number generator that generates 64 random
// bits and returns them as a `uint64_t`. This function does not require manual
// seeding.
SB_EXPORT uint64_t SbSystemGetRandomUInt64();

// A cryptographically secure random number generator that generates
// |buffer_size| random bytes and places them in |out_buffer|. This function
// does not require manual seeding.
//
// * |out_buffer|: The destination buffer for the generated random bytes. Must
//   not be `NULL`.
// * |buffer_size|: The number of random bytes to generate.
SB_EXPORT void SbSystemGetRandomData(void* out_buffer, int buffer_size);

// Gets the last platform-specific error code produced by any Starboard call in
// the current thread for diagnostic purposes. Do not use this error code to
// control program flow; handle function return values directly instead.
SB_EXPORT SbSystemError SbSystemGetLastError();

// Clears the last error set by a Starboard call in the current thread.
SB_EXPORT void SbSystemClearLastError();

// Generates a human-readable string for an error. The function returns the
// total length of the generated string.
//
// * |error|: The error for which to generate a string.
// * |out_string|: The destination buffer for the generated string. This can be
//   `NULL`. The output is always null-terminated.
// * |string_length|: The maximum length of the error string.
SB_EXPORT int SbSystemGetErrorString(SbSystemError error,
                                     char* out_string,
                                     int string_length);

// Places up to |stack_size| instruction pointer addresses of the current
// execution stack into |out_stack|. Returns the number of entries added.
//
// The returned stack frames are in downward order from the calling frame toward
// the thread's entry point. If the buffer is too small, frames near the thread
// entry point are truncated.
//
// This function is used in crash signal handlers and, therefore, it must be
// async-signal-safe on platforms that support signals. The following document
// discusses what it means to be async-signal-safe on POSIX:
// http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03
//
// * |out_stack|: A buffer to store the instruction pointer addresses. Must not
//   be
//   `NULL` and must hold at least |stack_size| entries.
// * |stack_size|: The maximum number of instruction pointer addresses to
//   retrieve.
SB_EXPORT int SbSystemGetStack(void** out_stack, int stack_size);

// Looks up |address| as an instruction pointer and places up to `buffer_size -
// 1` characters of its associated symbol in |out_buffer|. The output is always
// null-terminated. |out_buffer| must not be `NULL`.
//
// The return value indicates whether the function found a reasonable match for
// |address|. If the return value is `false`, then |out_buffer| is not modified.
//
// This function is used in crash signal handlers and, therefore, it must be
// async-signal-safe on platforms that support signals.
SB_EXPORT bool SbSystemSymbolize(const void* address,
                                 char* out_buffer,
                                 int buffer_size);

// Requests that the application transition to the `BLURRED` state. This
// corresponds to an unfocused application in a traditional window manager,
// where it may remain partially visible.
//
// This function triggers a `kSbEventTypeBlur` event. Before the event is
// dispatched, the application may continue working and receive other system
// events.
SB_EXPORT void SbSystemRequestBlur();

// Requests that the application transition to the `STARTED` (focused) state.
// This corresponds to focusing the application, making it fully visible and
// ready to receive input.
//
// This function triggers a `kSbEventTypeFocus` event. Before the event is
// dispatched, the application may continue working and receive other system
// events.
SB_EXPORT void SbSystemRequestFocus();

// Requests that the application transition to the `CONCEALED` state. This
// corresponds to minimizing the application, where it is no longer visible but
// background tasks can continue running.
//
// This function triggers a `kSbEventTypeConceal` event. Before the event is
// dispatched, the application may continue working and receive other system
// events.
//
// In the `CONCEALED` state, the application is invisible but may continue
// running background tasks. An external system event is expected to restore the
// application.
SB_EXPORT void SbSystemRequestConceal();

// Requests that the application transition to the `BLURRED` state. This
// corresponds to revealing the application, making it visible again after being
// concealed.
//
// This function triggers a `kSbEventTypeReveal` event. Before the event is
// dispatched, the application may continue working and receive other system
// events.
SB_EXPORT void SbSystemRequestReveal();

// Requests that the application transition to the `FROZEN` state.
//
// This function triggers a `kSbEventTypeFreeze` event. Before the event is
// dispatched, the application may continue working and receive other system
// events.
//
// In the `FROZEN` state, the application remains in memory but does not run. An
// external system event is expected to resume the application.
SB_EXPORT void SbSystemRequestFreeze();

// Requests that the application terminate gracefully. Before termination, the
// application may continue working and receive other system events. This
// function triggers a `kSbEventTypeStop` event. When the process terminates, it
// returns |error_level| (if supported by the platform).
//
// * |error_level|: An integer that serves as the return value for the process
//   that is eventually terminated as a result of a call to this function.
SB_EXPORT void SbSystemRequestStop(int error_level);

// Hides the splash screen shown while the application loads. This function can
// be called from any thread and must be idempotent.
SB_EXPORT void SbSystemHideSplashScreen();

// Returns `false` if the platform does not support resuming after suspension.
// In this case, Cobalt frees resources retained for resumption, and the
// platform must not send `kSbEventTypeResume` events. The return value must
// remain constant for the lifetime of the application.
SB_EXPORT bool SbSystemSupportsResume();

// Returns a pointer to the constant global structure implementing the extension
// specified by |name|. If the extension is not implemented, the function
// returns `NULL`. The |name| parameter must not be `NULL`.
//
// Extensions implement behaviors specific to a particular application and
// platform combination. They rely on a header file in the application's
// `extension/` subdirectory to define the extension API structure. Because this
// header is used both by the application and the Starboard platform, it must
// not include any application-level headers. It can, however, depend on
// Starboard headers.
//
// The API structure must begin with two required fields: a `const char* name`
// (storing the extension name) and a `uint32_t version` (storing the extension
// version). Remaining fields can be C types (including custom structures) or
// function pointers.
//
// The application queries for the extension by name using
// |SbSystemGetExtension|, and the platform returns a pointer to a singleton
// structure instance. This singleton structure must remain constant after
// initialization. When incrementing the extension version, you can append
// fields to the structure, but you must never remove existing fields (mark them
// as deprecated instead).
SB_EXPORT const void* SbSystemGetExtension(const char* name);

// Computes a HMAC-SHA256 digest of |message| using the application's
// certification secret, and writes it to |digest|. The |message| and |digest|
// pointers must not be `NULL`.
//
// The |digest_size_in_bytes| parameter must be at least 32. Returns `false` if
// an error occurs or if the function is not implemented; in these cases, the
// contents of |digest| are undefined.
SB_EXPORT bool SbSystemSignWithCertificationSecretKey(
    const uint8_t* message,
    size_t message_size_in_bytes,
    uint8_t* digest,
    size_t digest_size_in_bytes);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SYSTEM_H_
