// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/extension/ifa.h"

#import <AppTrackingTransparency/ATTrackingManager.h>

#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/system.h"
#include "starboard/tvos/shared/ifa.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace {
bool CopyStringAndTestIfSuccess(char* out_value,
                                int value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length) {
    return false;
  }
  starboard::strlcpy(out_value, from_value, value_length);
  return true;
}

bool GetAdvertisingId(char* out_value, int value_length) {
  // Just rely on the public Starboard API here.
  return SbSystemGetProperty(kSbSystemPropertyAdvertisingId, out_value,
                             value_length);
}

bool GetLimitAdTracking(char* out_value, int value_length) {
  // Just rely on the public Starboard API here.
  return SbSystemGetProperty(kSbSystemPropertyLimitAdTracking, out_value,
                             value_length);
}

bool GetTrackingAuthorizationStatus(char* out_value, int value_length) {
  if (@available(tvOS 14.0, *)) {
    ATTrackingManagerAuthorizationStatus status =
        [ATTrackingManager trackingAuthorizationStatus];
    if (status == ATTrackingManagerAuthorizationStatusNotDetermined) {
      return CopyStringAndTestIfSuccess(out_value, value_length,
                                        "NOT_DETERMINED");
    } else if (status == ATTrackingManagerAuthorizationStatusRestricted) {
      return CopyStringAndTestIfSuccess(out_value, value_length, "RESTRICTED");
    } else if (status == ATTrackingManagerAuthorizationStatusAuthorized) {
      return CopyStringAndTestIfSuccess(out_value, value_length, "AUTHORIZED");
    } else if (status == ATTrackingManagerAuthorizationStatusDenied) {
      return CopyStringAndTestIfSuccess(out_value, value_length, "DENIED");
    }
  }
  return CopyStringAndTestIfSuccess(out_value, value_length, "UNKNOWN");
}

starboard::Mutex g_tracking_authorization_callback_mutex;
void* g_tracking_authorization_callback_context = nullptr;
RequestTrackingAuthorizationCallback g_tracking_authorization_callback =
    nullptr;

void RegisterTrackingAuthorizationCallback(
    void* callback_context,
    RequestTrackingAuthorizationCallback callback) {
  ScopedLock lock(g_tracking_authorization_callback_mutex);
  g_tracking_authorization_callback_context = callback_context;
  g_tracking_authorization_callback = callback;
}

void UnregisterTrackingAuthorizationCallback() {
  ScopedLock lock(g_tracking_authorization_callback_mutex);
  g_tracking_authorization_callback_context = nullptr;
  g_tracking_authorization_callback = nullptr;
}

void RequestTrackingAuthorization() {
  if (@available(tvOS 14.0, *)) {
    [ATTrackingManager requestTrackingAuthorizationWithCompletionHandler:^(
                           ATTrackingManagerAuthorizationStatus status) {
      // Invoke the callback if set
      ScopedLock lock(g_tracking_authorization_callback_mutex);
      if (g_tracking_authorization_callback_context &&
          g_tracking_authorization_callback) {
        g_tracking_authorization_callback(
            g_tracking_authorization_callback_context);
      }
    }];
  }
}

const StarboardExtensionIfaApi kIfaApi = {
    kStarboardExtensionIfaName,
    2,  // API version that's implemented.
    &GetAdvertisingId,
    &GetLimitAdTracking,
    &GetTrackingAuthorizationStatus,
    &RegisterTrackingAuthorizationCallback,
    &UnregisterTrackingAuthorizationCallback,
    &RequestTrackingAuthorization};
}  // namespace

const void* GetIfaApi() {
  return &kIfaApi;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
