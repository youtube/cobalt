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
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

// You can see the log output by running a unit test
// $ autoninja -C out/linux-x64x11_devel starboard_unittests_wrapper &
// $ out/linux-x64x11_devel/starboard_unittests_wrapper \
//   --gtest_filter=DrmTest.LogSbDrmSessionRequestType
TEST(DrmTest, LogSbDrmSessionRequestType) {
#define LOG_SB_DRM_SESSION_REQUEST_TYPE(type) \
  SB_LOG(INFO) << #type << "=" << type;

  LOG_SB_DRM_SESSION_REQUEST_TYPE(kSbDrmSessionRequestTypeLicenseRequest);
  LOG_SB_DRM_SESSION_REQUEST_TYPE(kSbDrmSessionRequestTypeLicenseRenewal);
  LOG_SB_DRM_SESSION_REQUEST_TYPE(kSbDrmSessionRequestTypeLicenseRelease);
  LOG_SB_DRM_SESSION_REQUEST_TYPE(
      kSbDrmSessionRequestTypeIndividualizationRequest);
}

// You can see the log output by running a unit test
// $ autoninja -C out/linux-x64x11_devel starboard_unittests_wrapper &
// $ out/linux-x64x11_devel/starboard_unittests_wrapper \
//   --gtest_filter=DrmTest.LogSbDrmStatus
TEST(DrmTest, LogSbDrmStatus) {
#define LOG_SB_DRM_STATUS(status) SB_LOG(INFO) << #status << "=" << status;

  LOG_SB_DRM_STATUS(kSbDrmStatusSuccess);
  LOG_SB_DRM_STATUS(kSbDrmStatusTypeError);
  LOG_SB_DRM_STATUS(kSbDrmStatusNotSupportedError);
  LOG_SB_DRM_STATUS(kSbDrmStatusInvalidStateError);
  LOG_SB_DRM_STATUS(kSbDrmStatusQuotaExceededError);
  LOG_SB_DRM_STATUS(kSbDrmStatusUnknownError);
}

}  // namespace starboard
