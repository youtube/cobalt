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

void DummySessionUpdateRequestFunc(SbDrmSystem drm_system,
                                   void* context,
                                   int ticket,
                                   const void* session_id,
                                   int session_id_size,
                                   const void* content,
                                   int content_size,
                                   const char* url) {}

void DummySessionUpdatedFunc(SbDrmSystem drm_system,
                             void* context,
                             int ticket,
                             const void* session_id,
                             int session_id_size,
                             bool succeeded) {}

#if SB_HAS(DRM_KEY_STATUSES)
void DummySessionKeyStatusesChangedFunc(SbDrmSystem drm_system,
                                        void* context,
                                        const void* session_id,
                                        int session_id_size,
                                        int number_of_keys,
                                        const SbDrmKeyId* key_ids,
                                        const SbDrmKeyStatus* key_statuses) {}
#endif  // SB_HAS(DRM_KEY_STATUSES)

void DummySessionClosedFunc(SbDrmSystem drm_system,
                            void* context,
                            const void* session_id,
                            int session_id_size) {}

TEST(SbDrmTest, AnySupportedKeySystems) {
  const char* kKeySystems[] = {
      "com.widevine", "com.widevine.alpha", "com.youtube.playready", "fairplay",
  };
  bool any_supported_key_systems = false;
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kKeySystems); ++i) {
    const char* key_system = kKeySystems[i];
#if SB_HAS(DRM_SESSION_CLOSED)
    SbDrmSystem drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, DummySessionUpdateRequestFunc,
        DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
        DummySessionClosedFunc);
#elif SB_HAS(DRM_KEY_STATUSES)
    SbDrmSystem drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, DummySessionUpdateRequestFunc,
        DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc);
#else   // SB_HAS(DRM_KEY_STATUSES)
    SbDrmSystem drm_system = SbDrmCreateSystem(key_system, NULL /* context */,
                                               DummySessionUpdateRequestFunc,
                                               DummySessionUpdatedFunc);
#endif  // SB_HAS(DRM_KEY_STATUSES)
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

#if SB_API_VERSION >= SB_NULL_CALLBACKS_INVALID_RETURN_API_VERSION
TEST(SbDrmTest, NullCallbacks) {
  const char* kKeySystems[] = {
      "com.widevine", "com.widevine.alpha", "com.youtube.playready", "fairplay",
  };
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kKeySystems); ++i) {
    const char* key_system = kKeySystems[i];
#if SB_HAS(DRM_SESSION_CLOSED)
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */,
          NULL /* session_update_request_func */, DummySessionUpdatedFunc,
          DummySessionKeyStatusesChangedFunc,
          NULL /* session_closed_callback */);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          NULL /*session_updated_func */, DummySessionKeyStatusesChangedFunc,
          NULL /* session_closed_callback */);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          DummySessionUpdatedFunc, NULL /* session_key_statuses_changed_func */,
          NULL /* session_closed_callback */);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
#elif SB_HAS(DRM_KEY_STATUSES)
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */,
          NULL /* session_update_request_func */, DummySessionUpdatedFunc,
          DummySessionKeyStatusesChangedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          NULL /* session_updated_func */, DummySessionKeyStatusesChangedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          DummySessionUpdatedFunc,
          NULL /* session_key_statuses_changed_func */);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
#else
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */,
          NULL /* session_update_request_func */, DummySessionUpdatedFunc);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
    }
    {
      SbDrmSystem drm_system = SbDrmCreateSystem(
          key_system, NULL /* context */, DummySessionUpdateRequestFunc,
          NULL /* session_updated_func */);
      EXPECT_FALSE(SbDrmSystemIsValid(drm_system));
      SbDrmDestroySystem(drm_system);
    }
#endif  // SB_HAS(DRM_KEY_STATUSES)
  }
}
#endif  // SB_API_VERSION >= SB_NULL_CALLBACKS_INVALID_RETURN_API_VERSION
}  // namespace
}  // namespace nplb
}  // namespace starboard
