// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_IFA_H_
#define STARBOARD_EXTENSION_IFA_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionIfaName "dev.cobalt.extension.Ifa"

typedef void (*RequestTrackingAuthorizationCallback)(void* callback_context);

typedef struct StarboardExtensionIfaApi {
  // Name should be the string |kCobaltExtensionIfaName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Advertising ID or IFA, typically a 128-bit UUID
  // Please see https://iabtechlab.com/OTT-IFA for details.
  // Corresponds to 'ifa' field. Note: `ifa_type` field is not provided.
  bool (*GetAdvertisingId)(char* out_value, int value_length);

  // Limit advertising tracking, treated as boolean. Set to nonzero to indicate
  // a true value. Corresponds to 'lmt' field.
  bool (*GetLimitAdTracking)(char* out_value, int value_length);

  // The fields below this point were added in version 2 or later.

  // Returns the the user's authorization status for using IFA in this app.
  // Valid strings that can be returned are:
  //   * NOT_SUPPORTED - if this platform doesn't support this extension
  //   * UNKNOWN - if the system isn't able to determine a status
  //   * NOT_DETERMINED - the user hasn't made a decision yet
  //   * RESTRICTED - the system doesn't allow the user a choice
  //   * AUTHORIZED - the user agreed to tracking
  //   * DENIED - the user declined tracking
  // This is optional and only implemented on some platforms.
  bool (*GetTrackingAuthorizationStatus)(char* out_value, int value_length);

  // Registers the provided callback context and function to allow notification
  // when |RequestTrackingAuthorization| completes.
  // This is optional and only implemented on some platforms.
  void (*RegisterTrackingAuthorizationCallback)(
      void* callback_context,
      RequestTrackingAuthorizationCallback callback);

  // Unregisters any callback context and function set via
  // |RegisterTrackingAuthorizationCallback|.
  // This is optional and only implemented on some platforms.
  void (*UnregisterTrackingAuthorizationCallback)();

  // Asks the OS to request a user prompt to allow IFA in this app.
  // If a callback is registered (via |RegisterTrackingAuthorizationCallback|),
  // it will be called once the request completes.
  // This is optional and only implemented on some platforms.
  void (*RequestTrackingAuthorization)();
} CobaltExtensionIfaApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_IFA_H_
