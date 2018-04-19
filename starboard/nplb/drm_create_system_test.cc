// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <vector>

#include "starboard/drm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbDrmTest, AnySupportedKeySystems) {
  const char* key_systems[] = {
      "com.widevine", "com.widevine.alpha", "com.youtube.playready",
  };
  bool any_supported_key_systems = false;
  for (int i = 0; i < SB_ARRAY_SIZE_INT(key_systems); ++i) {
    const char* key_system = key_systems[i];
#if SB_HAS(DRM_SESSION_CLOSED)
    SbDrmSystem drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, NULL /* update_request_callback */,
        NULL /* session_updated_callback */,
        NULL /* key_statuses_changed_callback*/,
        NULL /* session_closed_callback */);
#elif SB_HAS(DRM_KEY_STATUSES)
    SbDrmSystem drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, NULL /* update_request_callback */,
        NULL /* session_updated_callback */,
        NULL /* key_statuses_changed_callback*/);
#else   // SB_HAS(DRM_KEY_STATUSES)
    SbDrmSystem drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, NULL /* update_request_callback */,
        NULL /* session_updated_callback */);
#endif  // SB_HAS(DRM_KEY_STATUSES)
    if (SbDrmSystemIsValid(drm_system)) {
      SB_DLOG(INFO) << "Drm system with key system " << key_system
                    << " is valid.";
    } else {
      SB_DLOG(INFO) << "Drm system with key system " << key_system
                    << " is NOT valid.";
    }
    any_supported_key_systems |= SbDrmSystemIsValid(drm_system);
  }
  EXPECT_TRUE(any_supported_key_systems) << " no DRM key systems supported";
}
}  // namespace
}  // namespace nplb
}  // namespace starboard
