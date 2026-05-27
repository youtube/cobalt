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
#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

using ::testing::ByMove;
using ::testing::Return;
using ::testing::SetArgPointee;

using AudioCodecCapabilities =
    MediaCapabilitiesProvider::AudioCodecCapabilities;
using VideoCodecCapabilities =
    MediaCapabilitiesProvider::VideoCodecCapabilities;

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

TEST_F(MediaCapabilitiesCacheTest, FindAudioDecoderEnabledCacheFound) {
  std::map<std::string, AudioCodecCapabilities> audio_codec_capabilities;
  audio_codec_capabilities["audio/mp4a-latm"].push_back(
      std::make_unique<MockAudioCodecCapability>(
          "decoder_aac", false, false, false, false, Range(0, 256000)));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::map<std::string, AudioCodecCapabilities>& audio_caps,
              std::map<std::string, VideoCodecCapabilities>& video_caps) {
            audio_caps = std::move(audio_codec_capabilities);
          }));

  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 192000), "decoder_aac");
  // Call again to test cache.
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 192000), "decoder_aac");
}

TEST_F(MediaCapabilitiesCacheTest, FindAudioDecoderEnabledCacheNotFound) {
  std::map<std::string, AudioCodecCapabilities> audio_codec_capabilities;
  audio_codec_capabilities["audio/mp4a-latm"].push_back(
      std::make_unique<MockAudioCodecCapability>(
          "decoder_aac", false, false, false, false, Range(0, 256000)));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::map<std::string, AudioCodecCapabilities>& audio_caps,
              std::map<std::string, VideoCodecCapabilities>& video_caps) {
            audio_caps = std::move(audio_codec_capabilities);
          }));

  // Mime type not found.
  EXPECT_EQ(cache_->FindAudioDecoder("audio/opus", 192000), "");
  // Bitrate not supported.
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 300000), "");
}

TEST_F(MediaCapabilitiesCacheTest, FindAudioDecoderDisabledCache) {
  cache_->SetCacheEnabled(false);
  EXPECT_CALL(*mock_media_capabilities_provider_,
              FindAudioDecoder("audio/mp4a-latm", 192000))
      .Times(2)
      .WillOnce(Return("decoder_aac"))
      .WillOnce(Return(""));

  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 192000), "decoder_aac");
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 192000), "");
}

TEST_F(MediaCapabilitiesCacheTest,
       FindAudioDecoderEnabledCacheDecoderSelection) {
  std::map<std::string, AudioCodecCapabilities> audio_codec_capabilities;
  // A high-bitrate decoder.
  audio_codec_capabilities["audio/mp4a-latm"].push_back(
      std::make_unique<MockAudioCodecCapability>(
          "decoder_aac_hq", false, false, false, false, Range(256000, 512000)));
  // A low-bitrate decoder.
  audio_codec_capabilities["audio/mp4a-latm"].push_back(
      std::make_unique<MockAudioCodecCapability>(
          "decoder_aac_lq", false, false, false, false, Range(0, 256000)));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::map<std::string, AudioCodecCapabilities>& audio_caps,
              std::map<std::string, VideoCodecCapabilities>& video_caps) {
            audio_caps = std::move(audio_codec_capabilities);
          }));

  // Low bitrate can only be handled by the second decoder.
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 192000),
            "decoder_aac_lq");

  // High bitrate can only be handled by the first decoder.
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 320000),
            "decoder_aac_hq");

  // Unsupported bitrate is not handled by any decoder.
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4a-latm", 600000), "");
}

TEST_F(MediaCapabilitiesCacheTest, FindVideoDecoderEnabledCacheFound) {
  std::map<std::string, VideoCodecCapabilities> video_codec_capabilities;
  video_codec_capabilities["video/avc"].push_back(
      std::make_unique<MockVideoCodecCapability>(
          "decoder_h264",      // name
          false,               // is_secure_req
          true,                // is_secure_sup
          false,               // is_tunnel_req
          true,                // is_tunnel_sup
          false,               // is_software_decoder
          true,                // is_hdr_capable
          Range(0, 1920),      // supported_widths
          Range(0, 1080),      // supported_heights
          Range(0, 20000000),  // supported_bitrates
          Range(0, 30)));      // supported_frame_rates
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::map<std::string, AudioCodecCapabilities>& audio_caps,
              std::map<std::string, VideoCodecCapabilities>& video_caps) {
            video_caps = std::move(video_codec_capabilities);
          }));

  // Simple case.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1280, 720, 1000000, 30),
            "decoder_h264");
  // Call again to test cache.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1280, 720, 1000000, 30),
            "decoder_h264");
  // Secure required.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", true, false, false, false,
                                     1280, 720, 1000000, 30),
            "decoder_h264.secure");
  // HDR required.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, true, false, false,
                                     1280, 720, 1000000, 30),
            "decoder_h264");
  // Tunnel mode required.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, true,
                                     1280, 720, 1000000, 30),
            "decoder_h264");
}

TEST_F(MediaCapabilitiesCacheTest, FindVideoDecoderEnabledCacheNotFound) {
  std::map<std::string, VideoCodecCapabilities> video_codec_capabilities;
  video_codec_capabilities["video/avc"].push_back(
      std::make_unique<MockVideoCodecCapability>(
          "decoder_h264",      // name
          false,               // is_secure_req
          false,               // is_secure_sup
          false,               // is_tunnel_req
          false,               // is_tunnel_sup
          false,               // is_software_decoder
          false,               // is_hdr_capable
          Range(0, 1920),      // supported_widths
          Range(0, 1080),      // supported_heights
          Range(0, 20000000),  // supported_bitrates
          Range(0, 30)));      // supported_frame_rates
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::map<std::string, AudioCodecCapabilities>& audio_caps,
              std::map<std::string, VideoCodecCapabilities>& video_caps) {
            video_caps = std::move(video_codec_capabilities);
          }));

  // Mime type not found.
  EXPECT_EQ(cache_->FindVideoDecoder("video/vp9", false, false, false, false,
                                     1280, 720, 1000000, 30),
            "");
  // Secure required but not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", true, false, false, false,
                                     1280, 720, 1000000, 30),
            "");
  // HDR required but not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, true, false, false,
                                     1280, 720, 1000000, 30),
            "");
  // Tunnel mode required but not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, true,
                                     1280, 720, 1000000, 30),
            "");
  // Software decoder required but not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, true, false,
                                     1280, 720, 1000000, 30),
            "");
  // Resolution not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     3840, 2160, 1000000, 30),
            "");
  // Frame rate not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1920, 1080, 1000000, 60),
            "");
  // Bitrate not supported.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1920, 1080, 30000000, 30),
            "");
}

TEST_F(MediaCapabilitiesCacheTest,
       FindVideoDecoderEnabledCacheDecoderSelection) {
  std::map<std::string, VideoCodecCapabilities> video_codec_capabilities;
  // A hardware decoder that supports up to 1080p.
  video_codec_capabilities["video/avc"].push_back(
      std::make_unique<MockVideoCodecCapability>(
          "decoder_h264_hw", false, true, false, true, false, true,
          Range(0, 1920), Range(0, 1080), Range(0, 20000000), Range(0, 30)));
  // A software decoder that supports up to 720p.
  video_codec_capabilities["video/avc"].push_back(
      std::make_unique<MockVideoCodecCapability>(
          "decoder_h264_sw", false, false, false, false, true, false,
          Range(0, 1280), Range(0, 720), Range(0, 10000000), Range(0, 30)));

  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::map<std::string, AudioCodecCapabilities>& audio_caps,
              std::map<std::string, VideoCodecCapabilities>& video_caps) {
            video_caps = std::move(video_codec_capabilities);
          }));

  // 720p request, should select the first decoder that can handle it.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1280, 720, 5000000, 30),
            "decoder_h264_hw");

  // 1080p request, only the first decoder can handle it.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1920, 1080, 15000000, 30),
            "decoder_h264_hw");

  // 720p request, but require software decoder.
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, true, false,
                                     1280, 720, 5000000, 30),
            "decoder_h264_sw");
}

TEST_F(MediaCapabilitiesCacheTest, FindVideoDecoderDisabledCache) {
  cache_->SetCacheEnabled(false);
  EXPECT_CALL(*mock_media_capabilities_provider_,
              FindVideoDecoder("video/avc", true, true, false, true, 1920, 1080,
                               10000000, 30))
      .Times(2)
      .WillOnce(Return("decoder_h264.secure"))
      .WillOnce(Return(""));

  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", true, true, false, true, 1920,
                                     1080, 10000000, 30),
            "decoder_h264.secure");
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", true, true, false, true, 1920,
                                     1080, 10000000, 30),
            "");
}

TEST_F(MediaCapabilitiesCacheTest, ClearCacheClearsAllValues) {
  SbMediaAudioConfiguration config;

  // Set expectations for the first call to the provider, which will happen
  // when the first cacheable value is requested.
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsWidevineSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsCbcsSchemeSupported())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_capabilities_provider_, GetSupportedHdrTypes())
      .WillOnce(
          Return(std::set<SbMediaTransferId>{kSbMediaTransferIdSmpteSt2084}));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](std::map<std::string, AudioCodecCapabilities>& audio_caps,
             std::map<std::string, VideoCodecCapabilities>& video_caps) {
            audio_caps["audio/mp4"].push_back(
                std::make_unique<MockAudioCodecCapability>(
                    "audio_decoder", false, false, false, false,
                    Range(0, 200000)));
            video_caps["video/avc"].push_back(
                std::make_unique<MockVideoCodecCapability>(
                    "video_decoder", false, true, false, true, false, true,
                    Range(0, 1920), Range(0, 1080), Range(0, 20000000),
                    Range(0, 30)));
          }));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(0, testing::_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(1, testing::_))
      .WillOnce(Return(false));

  // This is called separately, not as part of the initial cache population.
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(kSbMediaAudioCodecAc3))
      .WillOnce(Return(true));

  // Call all cacheable functions and verify they return the initial values.
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_FALSE(cache_->GetAudioConfiguration(1, &config));
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4", 192000), "audio_decoder");
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1280, 720, 1000000, 30),
            "video_decoder");

  // Call again to ensure we're hitting the cache (no more calls to provider).
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_FALSE(cache_->GetAudioConfiguration(1, &config));
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4", 192000), "audio_decoder");
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1280, 720, 1000000, 30),
            "video_decoder");

  cache_->ClearCache();

  // Set expectations for calls after the cache is cleared.
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsWidevineSupported())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_media_capabilities_provider_, GetIsCbcsSchemeSupported())
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_media_capabilities_provider_, GetSupportedHdrTypes())
      .WillOnce(Return(std::set<SbMediaTransferId>()));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetCodecCapabilities(testing::_, testing::_));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetAudioConfiguration(0, testing::_))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_media_capabilities_provider_,
              GetIsPassthroughSupported(kSbMediaAudioCodecAc3))
      .WillOnce(Return(false));

  // Verify that the cache is empty and new values are fetched.
  EXPECT_FALSE(cache_->IsWidevineSupported());
  EXPECT_FALSE(cache_->IsCbcsSchemeSupported());
  EXPECT_FALSE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  EXPECT_FALSE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_EQ(cache_->FindAudioDecoder("audio/mp4", 192000), "");
  EXPECT_EQ(cache_->FindVideoDecoder("video/avc", false, false, false, false,
                                     1280, 720, 1000000, 30),
            "");
}

}  // namespace starboard
