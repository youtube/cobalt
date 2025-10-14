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

#include "starboard/shared/starboard/media/drm_session_id_mapper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(DrmSessionIdMapperTest, InitialState) {
  DrmSessionIdMapper mapper;
  EXPECT_EQ(mapper.GetCdmSessionId("test_id"), "test_id");
  EXPECT_EQ(mapper.GetMediaDrmSessionId("test_id"), "test_id");
}

TEST(DrmSessionIdMapperTest, GetBridgeCdmSessionId) {
  DrmSessionIdMapper mapper;
  std::string cdm_session_id = mapper.GetBridgeCdmSessionId();
  EXPECT_FALSE(cdm_session_id.empty());
  EXPECT_NE(cdm_session_id, "test_id");

  // The media drm session id should be empty initially.
  EXPECT_TRUE(mapper.GetMediaDrmSessionId(cdm_session_id).empty());
}

TEST(DrmSessionIdMapperTest, GetCdmSessionId) {
  DrmSessionIdMapper mapper;
  std::string cdm_session_id = mapper.GetBridgeCdmSessionId();

  const std::string media_drm_session_id = "media_drm_session_id";
  if (mapper.IsMediaDrmSessionIdForProvisioningRequired()) {
    mapper.RegisterMediaDrmSessionIdForProvisioning(media_drm_session_id);
  }

  EXPECT_EQ(mapper.GetCdmSessionId(media_drm_session_id), cdm_session_id);
}

TEST(DrmSessionIdMapperTest, GetMediaDrmSessionId) {
  DrmSessionIdMapper mapper;
  std::string cdm_session_id = mapper.GetBridgeCdmSessionId();

  const std::string media_drm_session_id = "media_drm_session_id";
  if (mapper.IsMediaDrmSessionIdForProvisioningRequired()) {
    mapper.RegisterMediaDrmSessionIdForProvisioning(media_drm_session_id);
  }

  EXPECT_EQ(mapper.GetMediaDrmSessionId(cdm_session_id), media_drm_session_id);
}

TEST(DrmSessionIdMapperTest, GetBridgeCdmSessionIdIsConsistent) {
  DrmSessionIdMapper mapper;
  std::string cdm_session_id1 = mapper.GetBridgeCdmSessionId();
  std::string cdm_session_id2 = mapper.GetBridgeCdmSessionId();
  EXPECT_EQ(cdm_session_id1, cdm_session_id2);
}

TEST(DrmSessionIdMapperTest, IsMediaDrmSessionIdForProvisioningRequired) {
  DrmSessionIdMapper mapper;
  EXPECT_FALSE(mapper.IsMediaDrmSessionIdForProvisioningRequired());

  std::string cdm_id = mapper.GetBridgeCdmSessionId();
  EXPECT_TRUE(mapper.IsMediaDrmSessionIdForProvisioningRequired());

  mapper.RegisterMediaDrmSessionIdForProvisioning("media_id_1");
  EXPECT_FALSE(mapper.IsMediaDrmSessionIdForProvisioningRequired());
}

TEST(DrmSessionIdMapperTest, RegisterMediaDrmSessionIdForProvisioning) {
  DrmSessionIdMapper mapper;
  std::string cdm_id = mapper.GetBridgeCdmSessionId();
  EXPECT_TRUE(mapper.GetMediaDrmSessionId(cdm_id).empty());

  mapper.RegisterMediaDrmSessionIdForProvisioning("media_id_1");
  EXPECT_EQ(mapper.GetMediaDrmSessionId(cdm_id), "media_id_1");
}

}  // namespace
}  // namespace starboard
