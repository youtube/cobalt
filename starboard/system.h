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

// A broad set of APIs that allow the client application to query build and
// runtime properties of the enclosing system.

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

  // Path to the directory containing the root of the source tree.
  kSbSystemPathSourceDirectory,

  // Path to a directory where temporary files can be written.
  kSbSystemPathTempDirectory,

  // Path to a directory where test results can be written.
  kSbSystemPathTestOutputDirectory,

  // Full path to the executable file.
  kSbSystemPathExecutableFile,
} SbSystemPathId;

// System properties that can be queried for.
typedef enum SbSystemPropertyId {
  // The full model number of the main platform chipset, including any
  // vendor-specific prefixes.
  kSbSystemPropertyChipsetModelNumber,

  // The production firmware version number which the device is currently
  // running.
  kSbSystemPropertyFirmwareVersion,

  // A friendly name for this actual device. It may include user-personalization
  // like "Upstairs Bedroom." It may be displayed to users as part of some kind
  // of device selection.
  kSbSystemPropertyFriendlyName,

  // The name of the device manufacturer.
  kSbSystemPropertyManufacturerName,

  // The name of the device model.
  kSbSystemPropertyModelName,

  // The name of the network operator that owns the target device.
  kSbSystemPropertyNetworkOperatorName,

  // The name of this platform, suitable for inclusion in a User-Agent, say.
  kSbSystemPropertyPlatformName,

  // A universally-unique ID for the current user.
  kSbSystemPropertyPlatformUuid,
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
// determined at compile-time, and may vary from device to device, but will not
// change over the course of a single execution. They often specify particular
// behavior of other APIs within the bounds of their operating range.
typedef enum SbSystemCapabilityId {
  // Whether this system has reversed Enter and Back keys.
  kSbSystemCapabilityReversedEnterAndBack,
} SbSystemCapabilityId;

// Pointer to a function to compare two items, returning less than zero, zero,
// or greater than zero depending on whether |a| is less than |b|, equal to |b|,
// or greater than |b|, respectively (standard *cmp semantics).
typedef int (*SbSystemComparator)(const void* a, const void* b);

// Breaks the current program into the debugger, if a debugger is
// attached. Aborts the program otherwise.
SB_EXPORT void SbSystemBreakIntoDebugger();

// Attempts to determine if the current program is running inside or attached to
// a debugger.
SB_EXPORT bool SbSystemIsDebuggerAttached();

// Returns the number of processor cores available to this application. If the
// process is sandboxed to a subset of the physical cores, it will return that
// sandboxed limit.
SB_EXPORT int SbSystemGetNumberOfProcessors();

// Gets the total memory potentially available to this application. If the
// process is sandboxed to a maximum allowable limit, it will return the lesser
// of the physical and sandbox limits.
SB_EXPORT int64_t SbSystemGetTotalMemory();

// Returns the type of the device.
SB_EXPORT SbSystemDeviceType SbSystemGetDeviceType();

// Returns whether the device is a TV device.
SB_EXPORT bool SbSystemIsTvDeviceType(SbSystemDeviceType device_type);

// Returns the device's current network connection type.
SB_EXPORT SbSystemConnectionType SbSystemGetConnectionType();

// Gets the platform-defined system path specified by |path_id|, placing it as a
// zero-terminated string into the user-allocated |out_path|, unless it is
// longer than |path_length| - 1. Returns false if |path_id| is invalid for this
// platform, or if |path_length| is too short for the given result, or if
// |out_path| is NULL. On failure, |out_path| will not be changed.
// Implementation must be thread-safe.
SB_EXPORT bool SbSystemGetPath(SbSystemPathId path_id,
                               char* out_path,
                               int path_length);

// Gets the platform-defined system property specified by |property_id|, placing
// it as a zero-terminated string into the user-allocated |out_value|, unless it
// is longer than |value_length| - 1. Returns false if |property_id| is invalid
// for this platform, or if |value_length| is too short for the given result, or
// if |out_value| is NULL. On failure, |out_value| will not be changed.
// Implementation must be thread-safe.
SB_EXPORT bool SbSystemGetProperty(SbSystemPropertyId property_id,
                                   char* out_value,
                                   int value_length);

// Returns whether the platform has the runtime capability specified by
// |capability_id|. Returns false for any unknown capabilities. Implementation
// must be thread-safe.
SB_EXPORT bool SbSystemHasCapability(SbSystemCapabilityId capability_id);

// Gets the system's current POSIX-style Locale ID. The locale represents the
// location, language, and cultural conventions that the system wants to use,
// which affects which text is displayed to the user, and how display numbers,
// dates, currency, and the like are formatted.
//
// At its simplest, it can just be a BCP 47 language code, like "en_US". These
// days, POSIX also wants to include the encoding, like "en_US.UTF8". POSIX
// additionally allows a couple very bare-bones locales, like "C" or "POSIX",
// but they are not supported here. POSIX also supports different locale
// settings for a few different purposes, but Starboard only exposes a single
// locale at a time.
//
// RFC 5646 describing BCP 47 language codes:
// https://tools.ietf.org/html/bcp47
//
// For more information than you want on POSIX locales:
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap07.html
SB_EXPORT const char* SbSystemGetLocaleId();

// Gets sixty-four random bits returned as an uint64_t. This is expected to be a
// cryptographically secure random number generator, and doesn't require manual
// seeding.
SB_EXPORT uint64_t SbSystemGetRandomUInt64();

// Produces an arbitrary non-negative number, |buffer_size|, of random bytes
// which must not be negative, placing the result in |out_buffer|, which must
// not be null. This is expected to be a cryptographically secure random number
// generator, and doesn't require manual seeding.
SB_EXPORT void SbSystemGetRandomData(void* out_buffer, int buffer_size);

// Gets the last platform-specific error code produced by any Starboard call in
// the current thread for diagnostic purposes. Semantic reactions to Starboard
// function call results should be modeled explicitly.
SB_EXPORT SbSystemError SbSystemGetLastError();

// Clears the last error set by a Starboard call in the current thread.
SB_EXPORT void SbSystemClearLastError();

// Writes a human-readable string for |error|, up to |string_length| bytes, into
// the provided |out_string|. Returns the total desired length of the string.
// |out_string| may be null, in which case just the total desired length of the
// string is returned. |out_string| is always terminated with a null byte.
SB_EXPORT int SbSystemGetErrorString(SbSystemError error,
                                     char* out_string,
                                     int string_length);

// Places up to |stack_size| instruction pointer addresses of the current
// execution stack into |out_stack|, returning the number of entries
// populated. |out_stack| must point to a non-NULL array of void * of at least
// |stack_size| entries. The returned stack frames will be in "downward" order
// from the calling frame towards the entry point of the thread. So, if all the
// stack frames do not fit, the ones truncated will be the less interesting ones
// towards the thread entry point. This function must be async-signal-safe on
// platforms that support signals, as it will be used in crash signal handlers.
//
// See the following about what it means to be async-signal-safe on POSIX:
// http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html#tag_02_04_03
SB_EXPORT int SbSystemGetStack(void** out_stack, int stack_size);

// Looks up |address| as an instruction pointer and places up to |buffer_size| -
// 1 characters of the symbol associated with it in |out_buffer|, which must not
// be NULL. |out_buffer| will be NULL-terminated. Returns whether it found a
// reasonable match for |address|. If the return value is false, |out_buffer|
// will not be modified. This function must be async-signal-safe on platforms
// that support signals, as it will be used in crash signal handlers.
SB_EXPORT bool SbSystemSymbolize(const void* address,
                                 char* out_buffer,
                                 int buffer_size);

// Requests that the application be terminated gracefully at the next convenient
// point. Some work may continue to be done, and unrelated system events
// dispatched, in the meantime. This will eventually result in a
// kSbEventTypeStop event being dispatched to the application.  When the process
// finally terminates, it will return |error_level|, if that has any meaning on
// the current platform.
SB_EXPORT void SbSystemRequestStop(int error_level);

// Binary searches a sorted table |base| of |element_count| objects, each
// element |element_width| bytes in size for an element that |comparator|
// compares equal to |key|. Meant to be a drop-in replacement for bsearch.
SB_EXPORT void* SbSystemBinarySearch(const void* key,
                                     const void* base,
                                     size_t element_count,
                                     size_t element_width,
                                     SbSystemComparator comparator);

// Sorts an array of elements |base|, with |element_count| elements of
// |element_width| bytes each, using |comparator| as the comparison function.
// Meant to be a drop-in replacement for qsort.
SB_EXPORT void SbSystemSort(void* base,
                            size_t element_count,
                            size_t element_width,
                            SbSystemComparator comparator);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SYSTEM_H_
