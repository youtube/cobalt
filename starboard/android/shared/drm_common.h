// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_DRM_COMMON_H_
#define STARBOARD_ANDROID_SHARED_DRM_COMMON_H_

#include <jni.h>

#include "starboard/common/log.h"
#include "starboard/drm.h"

namespace starboard {
namespace android {
namespace shared {

// They are defined in the same order as in their Java counterparts.  Their
// values should be kept in consistent with their Java counterparts defined in
// android.media.MediaDrm.KeyStatus.
enum class MediaDrmKeyStatus : jint {
  kExpired = 1,
  kInternalError = 4,
  kOutputNotAllowed = 2,
  kPending = 3,
  kUsable = 0,
};

// They must have the same values as defined in MediaDrm.KeyRequest.
enum class RequestType : jint {
  kInitial = 0,
  kRenewal = 1,
  kRelease = 2,
};

inline SbDrmSessionRequestType ToSbDrmSessionRequestType(
    RequestType request_type) {
  switch (request_type) {
    case RequestType::kInitial:
      return kSbDrmSessionRequestTypeLicenseRequest;
    case RequestType::kRenewal:
      return kSbDrmSessionRequestTypeLicenseRenewal;
    case RequestType::kRelease:
      return kSbDrmSessionRequestTypeLicenseRelease;
    default:
      SB_NOTREACHED() << "Unknown key request type: "
                      << static_cast<jint>(request_type);
      return kSbDrmSessionRequestTypeLicenseRequest;
  }
}

// Converts a Java MediaDrm.KeyStatus status code to the equivalent
// SbDrmKeyStatus. The |status_code| is obtained via JNI from a Java KeyStatus
// object's getStatusCode() method.
inline SbDrmKeyStatus ToSbDrmKeyStatus(jint status_code) {
  switch (static_cast<MediaDrmKeyStatus>(status_code)) {
    case MediaDrmKeyStatus::kExpired:
      return kSbDrmKeyStatusExpired;
    case MediaDrmKeyStatus::kInternalError:
      return kSbDrmKeyStatusError;
    case MediaDrmKeyStatus::kOutputNotAllowed:
      return kSbDrmKeyStatusRestricted;
    case MediaDrmKeyStatus::kPending:
      return kSbDrmKeyStatusPending;
    case MediaDrmKeyStatus::kUsable:
      return kSbDrmKeyStatusUsable;
    default:
      SB_NOTREACHED() << "Unknown status=" << status_code;
      return kSbDrmKeyStatusError;
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_DRM_COMMON_H_
