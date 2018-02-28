// Copyright 2015 Google Inc. All Rights Reserved.
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
// Defines a broad set of APIs that allow the client application to query
// build and runtime properties of the enclosing system.

#ifndef STARBOARD_SYSTEM_H_
#define STARBOARD_SYSTEM_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// A type that can represent a system error code across all Starboard platforms.
typedef int SbSystemError;

// Enumeration of special paths that the platform can define.
typedef enum SbSystemPathId {
  // Path to where the local content files that ship with the binary are
  // available.
  kSbSystemPathContentDirectory,

  // Path to the directory that can be used as a local file cache, if
  // available.
  kSbSystemPathCacheDirectory,

  // Path to the directory where debug output (e.g. logs, trace output,
  // screenshots) can be written into.
  kSbSystemPathDebugOutputDirectory,

  // Path to a directory where system font files can be found. Should only be
  // specified on platforms that provide fonts usable by Starboard applications.
  kSbSystemPathFontDirectory,

  // Path to a directory where system font configuration metadata can be
  // found. May be the same directory as |kSbSystemPathFontDirectory|, but not
  // necessarily. Should only be specified on platforms that provide fonts
  // usable by Starboard applications.
  kSbSystemPathFontConfigurationDirectory,

#if SB_API_VERSION < SB_PATH_SOURCE_DIR_REMOVED_API_VERSION
  // Deprecated and unused. Tests looking for static data should instead look
  // in the 'test' subdirectory of kSbSystemPathContentDirectory.
  kSbSystemPathSourceDirectory,
#endif  // SB_API_VERSION < SB_PATH_SOURCE_DIR_REMOVED_API_VERSION

  // Path to a directory where temporary files can be written.
  kSbSystemPathTempDirectory,

  // Path to a directory where test results can be written.
  kSbSystemPathTestOutputDirectory,

  // Full path to the executable file.
  kSbSystemPathExecutableFile,
} SbSystemPathId;

// System properties that can be queried for. Many of these are used in
// User-Agent string generation.
typedef enum SbSystemPropertyId {
  // The full model number of the main platform chipset, including any
  // vendor-specific prefixes.
  kSbSystemPropertyChipsetModelNumber,

  // The production firmware version number which the device is currently
  // running.
  kSbSystemPropertyFirmwareVersion,

  // A friendly name for this actual device. It may include user-personalization
  // like "Upstairs Bedroom." It may be displayed to users as part of some kind
  // of device selection (e.g. in-app DIAL).
  kSbSystemPropertyFriendlyName,

  // A deprecated alias for |kSbSystemPropertyBrandName|.
  kSbSystemPropertyManufacturerName,

  // The name of the brand under which the device is being sold.
  kSbSystemPropertyBrandName = kSbSystemPropertyManufacturerName,

  // The final production model number of the device.
  kSbSystemPropertyModelName,

  // The year the device was launched, e.g. "2016".
  kSbSystemPropertyModelYear,

  // The name of the network operator that owns the target device, if
  // applicable.
  kSbSystemPropertyNetworkOperatorName,

  // The name of the operating system and platform, suitable for inclusion in a
  // User-Agent, say.
  kSbSystemPropertyPlatformName,

  // A universally-unique ID for the current user.
  kSbSystemPropertyPlatformUuid,

  // The Google Speech API key. The platform manufacturer is responsible
  // for registering a Google Speech API key for their products. In the API
  // Console (http://developers.google.com/console), you can enable the
  // Speech APIs and generate a Speech API key.
  kSbSystemPropertySpeechApiKey,

#if SB_API_VERSION >= 5
  // A field that, if available, is appended to the user agent
  kSbSystemPropertyUserAgentAuxField,
#endif  // SB_API_VERSION >= 5
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

  // Set top boxes (STBs) stream content primarily over cable or satellite.
  // Some STBs can also stream OTT content via the Internet.
  kSbSystemDeviceTypeSetTopBox,

  // A Smart TV is a TV that can directly run applications that stream OTT
  // content via the Internet.
  kSbSystemDeviceTypeTV,

  // Desktop PC.
  kSbSystemDeviceTypeDesktopPC,

  // An Android TV Device.
  kSbSystemDeviceTypeAndroidTV,

  // Unknown device.
  kSbSystemDeviceTypeUnknown,
} SbSystemDeviceType;

// Enumeration of network connection types.
typedef enum SbSystemConnectionType {
  // The system is on a wired connection.
  kSbSystemConnectionTypeWired,

  // The system is on a wireless connection.
  kSbSystemConnectionTypeWireless,

  // The system connection type is unknown.
  kSbSystemConnectionTypeUnknown,
} SbSystemConnectionType;

// Runtime capabilities are boolean properties of a platform that can't be
// determined at compile-time. They may vary from device to device, but they
// will not change over the course of a single execution. They often specify
// particular behavior of other APIs within the bounds of their operating range.
typedef enum SbSystemCapabilityId {
  // Whether this system has reversed Enter and Back keys.
  kSbSystemCapabilityReversedEnterAndBack,

  // Whether this system has the ability to report on GPU memory usage.
  // If (and only if) a system has this capcability will
  // SbSystemGetTotalGPUMemory() and SbSystemGetUsedGPUMemory() be valid to
  // call.
  kSbSystemCapabilityCanQueryGPUMemoryStats,
} SbSystemCapabilityId;

// Enumeration of possible values for the |type| parameter passed to  the
// |SbSystemRaisePlatformError| function.
typedef enum SbSystemPlatformErrorType {
  // Cobalt received a network connection error, or a network disconnection
  // event. If the |response| passed to |SbSystemPlatformErrorCallback| is
  // |kSbSystemPlatformErrorResponsePositive| then the request should be
  // retried, otherwise the app should be stopped.
  kSbSystemPlatformErrorTypeConnectionError,

#if SB_API_VERSION < 6
  // The current user is not signed in.
  kSbSystemPlatformErrorTypeUserSignedOut,

  // The current user does not meet the age requirements to use the app.
  kSbSystemPlatformErrorTypeUserAgeRestricted
#endif
} SbSystemPlatformErrorType;

// Possible responses for |SbSystemPlatformErrorCallback|.
typedef enum SbSystemPlatformErrorResponse {
  kSbSystemPlatformErrorResponsePositive,
  kSbSystemPlatformErrorResponseNegative,
  kSbSystemPlatformErrorResponseCancel
} SbSystemPlatformErrorResponse;

// Type of callback function that may be called in response to an error
// notification from |SbSystemRaisePlatformError|. |response| is a code to
// indicate the user's response, e.g. if the platform raised a dialog to notify
// the user of the error. |user_data| is the opaque pointer that was passed to
// the call to |SbSystemRaisePlatformError|.
typedef void (*SbSystemPlatformErrorCallback)(
    SbSystemPlatformErrorResponse response,
    void* user_data);

// Private structure used to represent a raised platform error.
typedef struct SbSystemPlatformErrorPrivate SbSystemPlatformErrorPrivate;

// Opaque handle returned by |SbSystemRaisePlatformError| that can be passed
// to |SbSystemClearPlatformError|.
typedef SbSystemPlatformErrorPrivate* SbSystemPlatformError;

// Well-defined value for an invalid |SbSystemPlatformError|.
#define kSbSystemPlatformErrorInvalid (SbSystemPlatformError) NULL

// Checks whether a |SbSystemPlatformError| is valid.
static SB_C_INLINE bool SbSystemPlatformErrorIsValid(
    SbSystemPlatformError handle) {
  return handle != kSbSystemPlatformErrorInvalid;
}

// Cobalt calls this function to notify the platform that an error has occurred
// in the application that the platform may need to handle. The platform is
// expected to then notify the user of the error and to provide a means for
// any required interaction, such as by showing a dialog.
//
// The return value is a handle that may be used in a subsequent call to
// |SbClearPlatformError|. For example, the handle could be used to
// programatically dismiss a dialog that was raised in response to the error.
// The lifetime of the object referenced by the handle is until the user reacts
// to the error or the error is dismissed by a call to
// SbSystemClearPlatformError, whichever happens first. Note that if the
// platform cannot respond to the error, then this function should return
// |kSbSystemPlatformErrorInvalid|.
//
// This function may be called from any thread, and it is the platform's
// responsibility to decide how to handle an error received while a previous
// error is still pending. If that platform can only handle one error at a
// time, then it may queue the second error or ignore it by returning
// |kSbSystemPlatformErrorInvalid|.
//
// |type|: An error type, from the SbSystemPlatformErrorType enum,
//    that defines the error.
// |callback|: A function that may be called by the platform to let the caller
//   know that the user has reacted to the error.
// |user_data|: An opaque pointer that the platform should pass as an argument
//   to the callback function, if it is called.
SB_EXPORT SbSystemPlatformError
SbSystemRaisePlatformError(SbSystemPlatformErrorType type,
                           SbSystemPlatformErrorCallback callback,
                           void* user_data);

// Clears a platform error that was previously raised by a call to
// |SbSystemRaisePlatformError|. The platform may use this, for example,
// to close a dialog that was opened in response to the error.
//
// |handle|: The platform error to be cleared.
SB_EXPORT void SbSystemClearPlatformError(SbSystemPlatformError handle);

// Pointer to a function to compare two items. The return value uses standard
// |*cmp| semantics:
// - |< 0| if |a| is less than |b|
// - |0| if the two items are equal
// - |> 1| if |a| is greater than |b|
//
// |a|: The first value to compare.
// |b|: The second value to compare.
typedef int (*SbSystemComparator)(const void* a, const void* b);

// Breaks the current program into the debugger, if a debugger is attached.
// If a debugger is not attached, this function aborts the program.
SB_NORETURN SB_EXPORT void SbSystemBreakIntoDebugger();

// Attempts to determine whether the current program is running inside or
// attached to a debugger. The function returns |false| if neither of those
// cases is true.
SB_EXPORT bool SbSystemIsDebuggerAttached();

// Returns the number of processor cores available to this application. If the
// process is sandboxed to a subset of the physical cores, the function returns
// that sandboxed limit.
SB_EXPORT int SbSystemGetNumberOfProcessors();

// Returns the total CPU memory (in bytes) potentially available to this
// application. If the process is sandboxed to a maximum allowable limit,
// the function returns the lesser of the physical and sandbox limits.
SB_EXPORT int64_t SbSystemGetTotalCPUMemory();

// Returns the total physical CPU memory (in bytes) used by this application.
// This value should always be less than (or, in particularly exciting
// situations, equal to) SbSystemGetTotalCPUMemory().
SB_EXPORT int64_t SbSystemGetUsedCPUMemory();

// Returns the total GPU memory (in bytes) available for use by this
// application. This function may only be called the return value for calls to
// SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats) is |true|.
SB_EXPORT int64_t SbSystemGetTotalGPUMemory();

// Returns the current amount of GPU memory (in bytes) that is currently being
// used by this application. This function may only be called if the return
// value for calls to
// SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats) is |true|.
SB_EXPORT int64_t SbSystemGetUsedGPUMemory();

// Returns the type of the device.
SB_EXPORT SbSystemDeviceType SbSystemGetDeviceType();

// Returns the device's current network connection type.
SB_EXPORT SbSystemConnectionType SbSystemGetConnectionType();

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
// places its value as a zero-terminated string into the user-allocated
// |out_value| unless it is longer than |value_length| - 1. This implementation
// must be thread-safe.
//
// This function returns |true| if the property is retrieved successfully. It
// returns |false| under any of the following conditions and, in any such
// case, |out_value| is not changed:
// - |property_id| is invalid for this platform
// - |value_length| is too short for the given result
// - |out_value| is NULL
//
// |property_id|: The system path to be retrieved.
// |out_value|: The platform-defined system property specified by |property_id|.
// |value_length|: The length of the system property.
SB_EXPORT bool SbSystemGetProperty(SbSystemPropertyId property_id,
                                   char* out_value,
                                   int value_length);

// Returns whether the platform has the runtime capability specified by
// |capability_id|. Returns false for any unknown capabilities. This
// implementation must be thread-safe.
//
// |capability_id|: The runtime capability to check.
SB_EXPORT bool SbSystemHasCapability(SbSystemCapabilityId capability_id);

// Gets the system's current POSIX-style Locale ID. The locale represents the
// location, language, and cultural conventions that the system wants to use,
// which affects which text is displayed to the user as well as how displayed
// numbers, dates, currency, and similar values are formatted.
//
// At its simplest, the locale ID can just be a BCP 47 language code, like
// |en_US|. Currently, POSIX also wants to include the encoding as in
// |en_US.UTF8|. POSIX also allows a couple very bare-bones locales, like "C"
// or "POSIX", but they are not supported here. POSIX also supports different
// locale settings for a few different purposes, but Starboard only exposes one
// locale at a time.
//
// RFC 5646 describes BCP 47 language codes:
// https://tools.ietf.org/html/bcp47
//
// For more information than you probably want about POSIX locales, see:
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html
SB_EXPORT const char* SbSystemGetLocaleId();

// A cryptographically secure random number generator that gets 64 random bits
// and returns them as an |uint64_t|. This function does not require manual
// seeding.
SB_EXPORT uint64_t SbSystemGetRandomUInt64();

// A cryptographically secure random number generator that produces an
// arbitrary, non-negative number of |buffer_size| random, non-negative bytes.
// The generated number is placed in |out_buffer|. This function does not
// require manual seeding.
//
// |out_buffer|: A pointer for the generated random number. This value must
//   not be null.
// |buffer_size|: The size of the random number, in bytes.
SB_EXPORT void SbSystemGetRandomData(void* out_buffer, int buffer_size);

// Gets the last platform-specific error code produced by any Starboard call in
// the current thread for diagnostic purposes. Semantic reactions to Starboard
// function call results should be modeled explicitly.
SB_EXPORT SbSystemError SbSystemGetLastError();

// Clears the last error set by a Starboard call in the current thread.
SB_EXPORT void SbSystemClearLastError();

// Generates a human-readable string for an error. The return value specifies
// the total desired length of the string.
//
// |error|: The error for which a human-readable string is generated.
// |out_string|: The generated string. This value may be null, and it is
//   always terminated with a null byte.
// |string_length|: The maximum length of the error string.
SB_EXPORT int SbSystemGetErrorString(SbSystemError error,
                                     char* out_string,
                                     int string_length);

// Places up to |stack_size| instruction pointer addresses of the current
// execution stack into |out_stack|. The return value specifies the number
// of entries added.
//
// The returned stack frames are in "downward" order from the calling frame
// toward the entry point of the thread. So, if all the stack frames do not
// fit, the ones truncated will be the less interesting ones toward the thread
// entry point.
//
// This function is used in crash signal handlers and, therefore, it must
// be async-signal-safe on platforms that support signals. The following
// document discusses what it means to be async-signal-safe on POSIX:
// http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03
//
// |out_stack|: A non-NULL array of |void *| of at least |stack_size| entries.
// |stack_size|: The maximum number of instruction pointer addresses to be
//   placed into |out_stack| from the current execution stack.
SB_EXPORT int SbSystemGetStack(void** out_stack, int stack_size);

// Looks up |address| as an instruction pointer and places up to
// (|buffer_size - 1|) characters of the symbol associated with it in
// |out_buffer|, which must not be NULL. |out_buffer| will be NULL-terminated.
//
// The return value indicates whether the function found a reasonable match
// for |address|. If the return value is |false|, then |out_buffer| is not
// modified.
//
// This function is used in crash signal handlers and, therefore, it must
// be async-signal-safe on platforms that support signals.
SB_EXPORT bool SbSystemSymbolize(const void* address,
                                 char* out_buffer,
                                 int buffer_size);

// Requests that the application move into the Paused state at the next
// convenient point. This should roughly correspond to "unfocused application"
// in a traditional window manager, where the application may be partially
// visible.
//
// This function eventually causes a |kSbEventTypePause| event to be dispatched
// to the application. Before the |kSbEventTypePause| event is dispatched, some
// work may continue to be done, and unrelated system events may be dispatched.
SB_EXPORT void SbSystemRequestPause();

// Requests that the application move into the Started state at the next
// convenient point. This should roughly correspond to a "focused application"
// in a traditional window manager, where the application is fully visible and
// the primary receiver of input events.
//
// This function eventually causes a |kSbEventTypeUnpause| event to be
// dispatched to the application. Before |kSbEventTypeUnpause| is dispatched,
// some work may continue to be done, and unrelated system events may be
// dispatched.
SB_EXPORT void SbSystemRequestUnpause();

// Requests that the application move into the Suspended state at the next
// convenient point. This should roughly correspond to "minimization" in a
// traditional window manager, where the application is no longer visible.
//
// This function eventually causes a |kSbEventTypeSuspend| event to be
// dispatched to the application. Before the |kSbEventTypeSuspend| event is
// dispatched, some work may continue to be done, and unrelated system events
// may be dispatched.
//
// In the Suspended state, the application will be resident, but probably not
// running. The expectation is that an external system event will bring the
// application out of the Suspended state.
SB_EXPORT void SbSystemRequestSuspend();

// Requests that the application be terminated gracefully at the next
// convenient point. In the meantime, some work may continue to be done, and
// unrelated system events may be dispatched. This function eventually causes
// a |kSbEventTypeStop| event to be dispatched to the application. When the
// process finally terminates, it returns |error_level|, if that has any
// meaning on the current platform.
//
// |error_level|: An integer that serves as the return value for the process
//   that is eventually terminated as a result of a call to this function.
SB_EXPORT void SbSystemRequestStop(int error_level);

// Binary searches a sorted table |base| of |element_count| objects, each
// element |element_width| bytes in size for an element that |comparator|
// compares equal to |key|.
//
// This function is meant to be a drop-in replacement for |bsearch|.
//
// |key|: The key to search for in the table.
// |base|: The sorted table of elements to be searched.
// |element_count|: The number of elements in the table.
// |element_width|: The size, in bytes, of each element in the table.
// |comparator|: A value that indicates how the element in the table should
//   compare to the specified |key|.
SB_EXPORT void* SbSystemBinarySearch(const void* key,
                                     const void* base,
                                     size_t element_count,
                                     size_t element_width,
                                     SbSystemComparator comparator);

// Sorts an array of elements |base|, with |element_count| elements of
// |element_width| bytes each, using |comparator| as the comparison function.
//
// This function is meant to be a drop-in replacement for |qsort|.
//
// |base|: The array of elements to be sorted.
// |element_count|: The number of elements in the array.
// |element_width|: The size, in bytes, of each element in the array.
// |comparator|: A value that indicates how the array should be sorted.
SB_EXPORT void SbSystemSort(void* base,
                            size_t element_count,
                            size_t element_width,
                            SbSystemComparator comparator);

// Hides the system splash screen on systems that support a splash screen that
// is displayed while the application is loading. This function may be called
// from any thread and must be idempotent.
SB_EXPORT void SbSystemHideSplashScreen();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SYSTEM_H_
