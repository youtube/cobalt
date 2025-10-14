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

#include "starboard/android/shared/drm_session_id_mapper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(DrmSessionIdMapperTest, InitialState) {
  DrmSessionIdMapper mapper;
  EXPECT_EQ(mapper.GetEmeSessionId("test_id"), "test_id");
  EXPECT_EQ(mapper.GetMediaDrmSessionId("test_id"), "test_id");
}

TEST(DrmSessionIdMapperTest, GetBridgeEmeSessionId) {
  DrmSessionIdMapper mapper;
  std::string eme_session_id = mapper.GetBridgeEmeSessionId();
  EXPECT_FALSE(eme_session_id.empty());
  EXPECT_NE(eme_session_id, "test_id");

  // The media drm session id should be empty initially.
  EXPECT_TRUE(mapper.GetMediaDrmSessionId(eme_session_id).empty());
}

TEST(DrmSessionIdMapperTest, GetEmeSessionId) {
  DrmSessionIdMapper mapper;
  std::string eme_session_id = mapper.GetBridgeEmeSessionId();

  const std::string media_drm_session_id = "media_drm_session_id";
  if (mapper.IsMediaDrmSessionIdForProvisioningRequired()) {
    mapper.RegisterMediaDrmSessionIdForProvisioning(media_drm_session_id);
  }

  EXPECT_EQ(mapper.GetEmeSessionId(media_drm_session_id), eme_session_id);
}

TEST(DrmSessionIdMapperTest, GetMediaDrmSessionId) {
  DrmSessionIdMapper mapper;
  std::string eme_session_id = mapper.GetBridgeEmeSessionId();

  const std::string media_drm_session_id = "media_drm_session_id";
  if (mapper.IsMediaDrmSessionIdForProvisioningRequired()) {
    mapper.RegisterMediaDrmSessionIdForProvisioning(media_drm_session_id);
  }

  EXPECT_EQ(mapper.GetMediaDrmSessionId(eme_session_id), media_drm_session_id);
}

TEST(DrmSessionIdMapperTest, GetBridgeEmeSessionIdIsConsistent) {
  DrmSessionIdMapper mapper;
  std::string eme_session_id1 = mapper.GetBridgeEmeSessionId();
  std::string eme_session_id2 = mapper.GetBridgeEmeSessionId();
  EXPECT_EQ(eme_session_id1, eme_session_id2);
}

TEST(DrmSessionIdMapperTest, IsMediaDrmSessionIdForProvisioningRequired) {
  DrmSessionIdMapper mapper;
  EXPECT_FALSE(mapper.IsMediaDrmSessionIdForProvisioningRequired());

  std::string eme_id = mapper.GetBridgeEmeSessionId();
  EXPECT_TRUE(mapper.IsMediaDrmSessionIdForProvisioningRequired());

  mapper.RegisterMediaDrmSessionIdForProvisioning("media_id_1");
  EXPECT_FALSE(mapper.IsMediaDrmSessionIdForProvisioningRequired());
}

TEST(DrmSessionIdMapperTest, RegisterMediaDrmSessionIdForProvisioning) {
  DrmSessionIdMapper mapper;
  std::string eme_id = mapper.GetBridgeEmeSessionId();
  EXPECT_TRUE(mapper.GetMediaDrmSessionId(eme_id).empty());

  mapper.RegisterMediaDrmSessionIdForProvisioning("media_id_1");
  EXPECT_EQ(mapper.GetMediaDrmSessionId(eme_id), "media_id_1");
}

}  // namespace
}  // namespace starboard
