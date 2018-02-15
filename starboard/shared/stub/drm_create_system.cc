// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/drm.h"

#if SB_HAS(DRM_SESSION_CLOSED)

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback,
    SbDrmSessionClosedFunc session_closed_callback) {
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(key_system);
  SB_UNREFERENCED_PARAMETER(update_request_callback);
  SB_UNREFERENCED_PARAMETER(session_updated_callback);
  SB_UNREFERENCED_PARAMETER(key_statuses_changed_callback);
  SB_UNREFERENCED_PARAMETER(session_closed_callback);
  return kSbDrmSystemInvalid;
}

#elif SB_HAS(DRM_KEY_STATUSES)

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback) {
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(key_system);
  SB_UNREFERENCED_PARAMETER(update_request_callback);
  SB_UNREFERENCED_PARAMETER(session_updated_callback);
  SB_UNREFERENCED_PARAMETER(key_statuses_changed_callback);
  return kSbDrmSystemInvalid;
}

#else  // SB_API_VERSION >= 6

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback) {
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(key_system);
  SB_UNREFERENCED_PARAMETER(update_request_callback);
  SB_UNREFERENCED_PARAMETER(session_updated_callback);
  return kSbDrmSystemInvalid;
}

#endif  // SB_API_VERSION >= 6
