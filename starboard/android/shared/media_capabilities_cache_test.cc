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

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "starboard/android/shared/mock_media_capabilities_cache.h"
#include "starboard/common/size.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/media/resolutions.h"
#include "starboard/testing/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

using ::testing::ByMove;
using ::testing::Return;
using ::testing::SetArgPointee;

MediaCapabilitiesProvider::VideoCodecCapabilities CreateVp9DecoderCaps(
    bool is_tunnel_sup,
    bool is_secure_sup = true,
    bool is_hdr_capable = false,
    Range width = Range{0, 1920},
    Range height = Range{0, 1080},
    Range bitrate = Range{0, 10'000'000},
    Range fps = Range{0, 30}) {
  MediaCapabilitiesProvider::VideoCodecCapabilities caps;
  caps.push_back(std::make_unique<MockVideoCodecCapability>(
      "OMX.google.vp9.decoder",
      /*is_secure_req=*/false, is_secure_sup,
      /*is_tunnel_req=*/false, is_tunnel_sup,
      /*is_software_decoder=*/false, is_hdr_capable, width, height, bitrate,
      fps));
  return caps;
}

class MediaCapabilitiesCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mock_media_capabilities_provider =
        std::make_unique<MockMediaCapabilitiesProvider>();
    mock_media_capabilities_provider_ = mock_media_capabilities_provider.get();

    cache_ = std::make_unique<MockMediaCapabilitiesCache>(
        std::move(mock_media_capabilities_provider));
    cache_->SetCacheEnabled(true);
  }

  MockMediaCapabilitiesProvider* mock_media_capabilities_provider_;
  std::unique_ptr<MediaCapabilitiesCache> cache_;
};

TEST_F(MediaCapabilitiesCacheTest, IsWidevineSupported_EnabledCache) {
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsWidevineSupported())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_TRUE(cache_->IsWidevineSupported());
}

TEST_F(MediaCapabilitiesCacheTest, IsWidevineSupported_DisabledCache) {
  cache_->SetCacheEnabled(false);
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsWidevineSupported())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_FALSE(cache_->IsWidevineSupported());
}

TEST_F(MediaCapabilitiesCacheTest, IsCbcsSchemeSupported_EnabledCache) {
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsCbcsSchemeSupported())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
}

TEST_F(MediaCapabilitiesCacheTest, IsCbcsSchemeSupported_DisabledCache) {
  cache_->SetCacheEnabled(false);
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsCbcsSchemeSupported())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
  EXPECT_FALSE(cache_->IsCbcsSchemeSupported());
}

TEST_F(MediaCapabilitiesCacheTest,
       IsHDRTransferCharacteristicsSupported_EnabledCache) {
  std::set<SbMediaTransferId> supported_types = {kSbMediaTransferIdSmpteSt2084};
  EXPECT_CALL(*mock_media_capabilities_provider_, GetSupportedHdrTypes())
      .Times(1)
      .WillOnce(Return(supported_types));

  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_FALSE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdAribStdB67));
  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_FALSE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdAribStdB67));
}

TEST_F(MediaCapabilitiesCacheTest,
       IsHDRTransferCharacteristicsSupported_DisabledCache) {
  cache_->SetCacheEnabled(false);
  std::set<SbMediaTransferId> supported_types = {kSbMediaTransferIdSmpteSt2084};
  EXPECT_CALL(*mock_media_capabilities_provider_, GetSupportedHdrTypes())
      .Times(2)
      .WillRepeatedly(Return(supported_types));

  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_FALSE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdAribStdB67));
}

TEST_F(MediaCapabilitiesCacheTest, IsPassthroughSupported_EnabledCache) {
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(kSbMediaAudioCodecAc3))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(kSbMediaAudioCodecEac3))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecEac3));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecEac3));
}

TEST_F(MediaCapabilitiesCacheTest, IsPassthroughSupported_DisabledCache) {
  cache_->SetCacheEnabled(false);
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(kSbMediaAudioCodecAc3))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(kSbMediaAudioCodecEac3))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecEac3));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecEac3));
}

TEST_F(MediaCapabilitiesCacheTest, GetAudioConfiguration_EnabledCache) {
  std::vector<SbMediaAudioConfiguration> configs = {
      {kSbMediaAudioConnectorHdmi, 0, kSbMediaAudioCodingTypeAc3, 2},
      {kSbMediaAudioConnectorHdmi, 0, kSbMediaAudioCodingTypeDolbyDigitalPlus,
       6},
  };
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(0, testing::_))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<1>(configs[0]), Return(true)));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(1, testing::_))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<1>(configs[1]), Return(true)));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(2, testing::_))
      .WillOnce(Return(false));

  SbMediaAudioConfiguration config;
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypeAc3);

  EXPECT_TRUE(cache_->GetAudioConfiguration(1, &config));
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypeDolbyDigitalPlus);

  EXPECT_FALSE(cache_->GetAudioConfiguration(2, &config));
}

TEST_F(MediaCapabilitiesCacheTest, GetAudioConfiguration_DisabledCache) {
  cache_->SetCacheEnabled(false);
  std::vector<SbMediaAudioConfiguration> configs = {
      {kSbMediaAudioConnectorHdmi, 0, kSbMediaAudioCodingTypeAc3, 2},
      {kSbMediaAudioConnectorHdmi, 0, kSbMediaAudioCodingTypeDolbyDigitalPlus,
       6},
  };

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(0, testing::_))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<1>(configs[0]), Return(true)));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(1, testing::_))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<1>(configs[1]), Return(true)));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(2, testing::_))
      .WillOnce(Return(false));

  SbMediaAudioConfiguration config;
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypeAc3);

  EXPECT_TRUE(cache_->GetAudioConfiguration(1, &config));
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypeDolbyDigitalPlus);

  EXPECT_FALSE(cache_->GetAudioConfiguration(2, &config));
}

TEST_F(MediaCapabilitiesCacheTest, ClearCacheClearsAllValues) {
  SbMediaAudioConfiguration config;

  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsWidevineSupported())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsCbcsSchemeSupported())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_media_capabilities_provider_, GetSupportedHdrTypes())
      .Times(2)
      .WillRepeatedly(
          Return(std::set<SbMediaTransferId>{kSbMediaTransferIdSmpteSt2084}));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(testing::_))
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(testing::_, testing::_))
      .Times(4)
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly(Return());

  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_FALSE(cache_->HasAudioDecoderFor("audio/mp4", 192000));

  // Call all cache functions again to ensure that the provider functions
  // are not being called to retrieve the values.
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_FALSE(cache_->HasAudioDecoderFor("audio/mp4", 192000));

  cache_->ClearCache();

  EXPECT_FALSE(cache_->IsWidevineSupported());
  EXPECT_FALSE(cache_->IsCbcsSchemeSupported());
  EXPECT_FALSE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdAribStdB67));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecEac3));
  EXPECT_FALSE(cache_->GetAudioConfiguration(2, &config));
  EXPECT_FALSE(cache_->HasAudioDecoderFor("audio/mp4", 192000));
}

TEST_F(MediaCapabilitiesCacheTest, HasVideoDecoderFor_ResolutionLimits) {
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .WillOnce(testing::Invoke([](auto&, auto& video_caps) {
        video_caps["video/x-vnd.on2.vp9"] = CreateVp9DecoderCaps(
            /*is_tunnel_sup=*/true, /*is_secure_sup=*/true,
            /*is_hdr_capable=*/false, Range{0, 1920}, Range{0, 1080},
            Range{0, 10'000'000}, Range{0, 30});
      }));

  EXPECT_TRUE(cache_->HasVideoDecoderFor("video/x-vnd.on2.vp9",
                                         /*must_support_secure=*/false,
                                         /*must_support_hdr=*/false,
                                         /*must_support_tunnel_mode=*/false,
                                         Resolution::k1080p,
                                         /*bitrate=*/5'000'000,
                                         /*fps=*/30));

  // Width exceeded
  EXPECT_FALSE(cache_->HasVideoDecoderFor("video/x-vnd.on2.vp9",
                                          /*must_support_secure=*/false,
                                          /*must_support_hdr=*/false,
                                          /*must_support_tunnel_mode=*/false,
                                          Size(3840, 1080),
                                          /*bitrate=*/5'000'000,
                                          /*fps=*/30));

  // Height exceeded
  EXPECT_FALSE(cache_->HasVideoDecoderFor("video/x-vnd.on2.vp9",
                                          /*must_support_secure=*/false,
                                          /*must_support_hdr=*/false,
                                          /*must_support_tunnel_mode=*/false,
                                          Size(1920, 2160),
                                          /*bitrate=*/5'000'000,
                                          /*fps=*/30));

  // Bitrate exceeded
  EXPECT_FALSE(cache_->HasVideoDecoderFor("video/x-vnd.on2.vp9",
                                          /*must_support_secure=*/false,
                                          /*must_support_hdr=*/false,
                                          /*must_support_tunnel_mode=*/false,
                                          Resolution::k1080p,
                                          /*bitrate=*/20'000'000,
                                          /*fps=*/30));

  // FPS exceeded
  EXPECT_FALSE(cache_->HasVideoDecoderFor("video/x-vnd.on2.vp9",
                                          /*must_support_secure=*/false,
                                          /*must_support_hdr=*/false,
                                          /*must_support_tunnel_mode=*/false,
                                          Resolution::k1080p,
                                          /*bitrate=*/5'000'000,
                                          /*fps=*/60));
}

TEST_F(MediaCapabilitiesCacheTest, FindVideoDecoder_Overload) {
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .WillOnce(testing::Invoke([](auto&, auto& video_caps) {
        video_caps["video/x-vnd.on2.vp9"] = CreateVp9DecoderCaps(
            /*is_tunnel_sup=*/true, /*is_secure_sup=*/true,
            /*is_hdr_capable=*/true);
      }));

  EXPECT_EQ(cache_->FindVideoDecoder("video/x-vnd.on2.vp9",
                                     /*must_support_secure=*/false,
                                     /*must_support_hdr=*/true,
                                     /*require_software_codec=*/false,
                                     /*must_support_tunnel_mode=*/true),
            "OMX.google.vp9.decoder");
}

TEST_F(MediaCapabilitiesCacheTest, RejectLowPerformanceSoftwareDecoder) {
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .WillOnce(testing::Invoke([](auto&, auto& video_caps) {
        MediaCapabilitiesProvider::VideoCodecCapabilities caps;
        caps.push_back(std::make_unique<MockVideoCodecCapability>(
            "OMX.test.soft.vp9.decoder",
            /*is_secure_req=*/false, /*is_secure_sup=*/false,
            /*is_tunnel_req=*/false, /*is_tunnel_sup=*/false,
            /*is_software_decoder=*/true,
            /*is_hdr_capable=*/false, Range{0, 1280}, Range{0, 720},
            Range{0, 5'000'000}, Range{0, 30}));
        video_caps["video/x-vnd.on2.vp9"] = std::move(caps);
      }));

  // Case 1: Software codec is NOT required.
  // The software decoder should be rejected because it is low performance (does
  // not support 1080p). See b/456473829 for better context.
  EXPECT_EQ(cache_->FindVideoDecoder("video/x-vnd.on2.vp9",
                                     /*must_support_secure=*/false,
                                     /*must_support_hdr=*/false,
                                     /*require_software_codec=*/false,
                                     /*must_support_tunnel_mode=*/false),
            "");

  // Case 2: Software codec IS explicitly required.
  // The software decoder should be successfully returned, even though it is low
  // performance.
  EXPECT_EQ(cache_->FindVideoDecoder("video/x-vnd.on2.vp9",
                                     /*must_support_secure=*/false,
                                     /*must_support_hdr=*/false,
                                     /*require_software_codec=*/true,
                                     /*must_support_tunnel_mode=*/false),
            "OMX.test.soft.vp9.decoder");
}

}  // namespace starboard
