// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/drm.h"

#include "starboard/common/log.h"
#include "starboard/drm.h"

namespace starboard {

const char* GetSbDrmSessionRequestTypeName(
    SbDrmSessionRequestType request_type) {
  switch (request_type) {
    case kSbDrmSessionRequestTypeLicenseRequest:
      return "license-request";
    case kSbDrmSessionRequestTypeLicenseRenewal:
      return "license-renewal";
    case kSbDrmSessionRequestTypeLicenseRelease:
      return "license-release";
    case kSbDrmSessionRequestTypeIndividualizationRequest:
      return "individualization-request";
    default:
      SB_NOTREACHED() << "Unexpected SbDrmSessionRequestType="
                      << static_cast<int>(request_type);
      return "unexpected";
  }
}

const char* GetSbDrmStatusName(SbDrmStatus status) {
  switch (status) {
    case kSbDrmStatusSuccess:
      return "success";
    case kSbDrmStatusTypeError:
      return "type-error";
    case kSbDrmStatusNotSupportedError:
      return "not-supported-error";
    case kSbDrmStatusInvalidStateError:
      return "invalid-state-error";
    case kSbDrmStatusQuotaExceededError:
      return "quota-exceeded-error";
    case kSbDrmStatusUnknownError:
      return "unknown-error";
    default:
      SB_NOTREACHED() << "Unexpected SbDrmStatus" << static_cast<int>(status);
      return "unexpected";
  }
}
}  // namespace starboard

std::ostream& operator<<(std::ostream& os,
                         SbDrmSessionRequestType request_type) {
  return os << starboard::GetSbDrmSessionRequestTypeName(request_type);
}

std::ostream& operator<<(std::ostream& os, SbDrmStatus status) {
  return os << starboard::GetSbDrmStatusName(status);
}
