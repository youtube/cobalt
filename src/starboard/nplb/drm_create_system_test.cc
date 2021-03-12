// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/nplb/drm_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbDrmTest, AnySupportedKeySystems) {
  bool any_supported_key_systems = false;
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kKeySystems); ++i) {
    const char* key_system = kKeySystems[i];
    SbDrmSystem drm_system = CreateDummyDrmSystem(key_system);
    if (SbDrmSystemIsValid(drm_system)) {
      SB_DLOG(INFO) << "Drm system with key system " << key_system
                    << " is valid.";
    } else {
      SB_DLOG(INFO) << "Drm system with key system " << key_system
                    << " is NOT valid.";
    }
    any_supported_key_systems |= SbDrmSystemIsValid(drm_system);
    SbDrmDestroySystem(drm_system);
  }
  EXPECT_TRUE(any_supported_key_systems) << " no DRM key systems supported";
}

TEST(SbDrmTest, NullCallbacks) {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kKeySystems); ++i) {
    const char* key_system = kKeySystems[i];
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */,
          NULL /* session_update_request_func */, DummySessionUpdatedFunc,
          DummySessionKeyStatusesChangedFunc, DummyServerCertificateUpdatedFunc,
          DummySessionClosedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          NULL /*session_updated_func */, DummySessionKeyStatusesChangedFunc,
          DummyServerCertificateUpdatedFunc, DummySessionClosedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          DummySessionUpdatedFunc, NULL /* session_key_statuses_changed_func */,
          DummyServerCertificateUpdatedFunc, DummySessionClosedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
          NULL /* server_certificate_updated_func */, DummySessionClosedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
          DummyServerCertificateUpdatedFunc, NULL /* session_closed_func */);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
  }
}

TEST(SbDrmTest, MultiDrm) {
  const int kMaxPlayersPerKeySystem = 16;
  std::vector<SbDrmSystem> created_drm_systems;
  int number_of_drm_systems = 0;
  for (int i = 0; i < kMaxPlayersPerKeySystem; ++i) {
    for (int j = 0; j < SB_ARRAY_SIZE_INT(kKeySystems); ++j) {
      const char* key_system = kKeySystems[j];
      created_drm_systems.push_back(CreateDummyDrmSystem(key_system));
      if (!SbDrmSystemIsValid(created_drm_systems.back())) {
        created_drm_systems.pop_back();
      }
    }
    if (created_drm_systems.size() == number_of_drm_systems) {
      break;
    }
    number_of_drm_systems = created_drm_systems.size();
  }
  SB_DLOG(INFO) << "Created " << number_of_drm_systems
                << " DRM systems in total.";
  for (auto drm_system : created_drm_systems) {
    SbDrmDestroySystem(drm_system);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
