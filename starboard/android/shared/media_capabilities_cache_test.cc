// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/media_capabilities_cache.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

using ::testing::ByMove;
using ::testing::Return;

// Mock DrmCapabilitiesProvider
class MockDrmCapabilitiesProvider : public DrmCapabilitiesProvider {
 public:
  MOCK_METHOD(bool, IsWidevineSupported, (), (override));
  MOCK_METHOD(bool, IsCbcsSchemeSupported, (), (override));
};

// Mock HdrCapabilitiesProvider
class MockHdrCapabilitiesProvider : public HdrCapabilitiesProvider {
 public:
  MOCK_METHOD(std::set<SbMediaTransferId>,
              GetSupportedHdrTransferIds,
              (),
              (override));
};

// Mock PassthroughCapabilitiesProvider
class MockPassthroughCapabilitiesProvider
    : public PassthroughCapabilitiesProvider {
 public:
  MOCK_METHOD(bool,
              IsPassthroughSupported,
              (SbMediaAudioCodec codec),
              (override));
};

// Mock AudioConfigurationProvider
class MockAudioConfigurationProvider : public AudioConfigurationProvider {
 public:
  MOCK_METHOD(std::vector<SbMediaAudioConfiguration>,
              GetAudioConfigurations,
              (),
              (override));
};

// Custom action to return an empty map of unique_ptrs
ACTION(ReturnEmptyAudioCodecCapabilities) {
  return std::map<std::string,
                  std::vector<std::unique_ptr<AudioCodecCapability>>>();
}

ACTION(ReturnEmptyVideoCodecCapabilities) {
  return std::map<std::string,
                  std::vector<std::unique_ptr<VideoCodecCapability>>>();
}

// Mock CodecCapabilitiesProvider
class MockCodecCapabilitiesProvider : public CodecCapabilitiesProvider {
 public:
  MOCK_METHOD((std::map<std::string,
                        std::vector<std::unique_ptr<AudioCodecCapability>>>),
              GetAudioCodecCapabilities,
              (),
              (override));
  MOCK_METHOD((std::map<std::string,
                        std::vector<std::unique_ptr<VideoCodecCapability>>>),
              GetVideoCodecCapabilities,
              (),
              (override));
};

class MediaCapabilitiesCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mock_drm_provider = std::make_unique<MockDrmCapabilitiesProvider>();
    mock_drm_provider_ = mock_drm_provider.get();
    auto mock_hdr_provider = std::make_unique<MockHdrCapabilitiesProvider>();
    mock_hdr_provider_ = mock_hdr_provider.get();
    auto mock_passthrough_provider =
        std::make_unique<MockPassthroughCapabilitiesProvider>();
    mock_passthrough_provider_ = mock_passthrough_provider.get();
    auto mock_audio_config_provider =
        std::make_unique<MockAudioConfigurationProvider>();
    mock_audio_config_provider_ = mock_audio_config_provider.get();
    auto mock_codec_provider =
        std::make_unique<MockCodecCapabilitiesProvider>();
    mock_codec_provider_ = mock_codec_provider.get();

    // Default mock behaviors
    ON_CALL(*mock_drm_provider_, IsWidevineSupported())
        .WillByDefault(Return(true));
    ON_CALL(*mock_drm_provider_, IsCbcsSchemeSupported())
        .WillByDefault(Return(true));
    ON_CALL(*mock_hdr_provider_, GetSupportedHdrTransferIds())
        .WillByDefault(
            Return(std::set<SbMediaTransferId>{kSbMediaTransferIdSmpteSt2084}));
    ON_CALL(*mock_passthrough_provider_, IsPassthroughSupported(::testing::_))
        .WillByDefault(Return(false));
    ON_CALL(*mock_audio_config_provider_, GetAudioConfigurations())
        .WillByDefault(Return(std::vector<SbMediaAudioConfiguration>{}));
    ON_CALL(*mock_codec_provider_, GetAudioCodecCapabilities())
        .WillByDefault(ReturnEmptyAudioCodecCapabilities());
    ON_CALL(*mock_codec_provider_, GetVideoCodecCapabilities())
        .WillByDefault(ReturnEmptyVideoCodecCapabilities());

    cache_ = MediaCapabilitiesCache::CreateForTest(
        std::move(mock_drm_provider), std::move(mock_hdr_provider),
        std::move(mock_passthrough_provider),
        std::move(mock_audio_config_provider), std::move(mock_codec_provider));
    cache_->SetCacheEnabled(true);
  }

  MockDrmCapabilitiesProvider* mock_drm_provider_;
  MockHdrCapabilitiesProvider* mock_hdr_provider_;
  MockPassthroughCapabilitiesProvider* mock_passthrough_provider_;
  MockAudioConfigurationProvider* mock_audio_config_provider_;
  MockCodecCapabilitiesProvider* mock_codec_provider_;
  std::unique_ptr<MediaCapabilitiesCache> cache_;
};

TEST_F(MediaCapabilitiesCacheTest, IsWidevineSupported_EnabledCache) {
  EXPECT_CALL(*mock_drm_provider_, IsWidevineSupported()).Times(1);
  EXPECT_TRUE(cache_->IsWidevineSupported());
  // Second call should hit cache, no further calls to provider
  EXPECT_TRUE(cache_->IsWidevineSupported());
}

TEST_F(MediaCapabilitiesCacheTest, IsWidevineSupported_DisabledCache) {
  cache_->SetCacheEnabled(false);
  EXPECT_CALL(*mock_drm_provider_, IsWidevineSupported()).Times(2);
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_TRUE(cache_->IsWidevineSupported());
}

TEST_F(MediaCapabilitiesCacheTest, IsCbcsSchemeSupported) {
  EXPECT_CALL(*mock_drm_provider_, IsCbcsSchemeSupported()).Times(1);
  EXPECT_TRUE(cache_->IsCbcsSchemeSupported());
}

TEST_F(MediaCapabilitiesCacheTest, IsHDRTransferCharacteristicsSupported) {
  EXPECT_CALL(*mock_hdr_provider_, GetSupportedHdrTransferIds()).Times(1);
  EXPECT_TRUE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdSmpteSt2084));
  EXPECT_FALSE(cache_->IsHDRTransferCharacteristicsSupported(
      kSbMediaTransferIdAribStdB67));
}

TEST_F(MediaCapabilitiesCacheTest, IsPassthroughSupported) {
  EXPECT_CALL(*mock_passthrough_provider_,
              IsPassthroughSupported(kSbMediaAudioCodecAc3))
      .WillOnce(Return(true));
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));
  // Second call should hit internal cache, not provider
  EXPECT_TRUE(cache_->IsPassthroughSupported(kSbMediaAudioCodecAc3));

  EXPECT_CALL(*mock_passthrough_provider_,
              IsPassthroughSupported(kSbMediaAudioCodecEac3))
      .WillOnce(Return(false));
  EXPECT_FALSE(cache_->IsPassthroughSupported(kSbMediaAudioCodecEac3));
}

TEST_F(MediaCapabilitiesCacheTest, GetAudioConfiguration) {
  std::vector<SbMediaAudioConfiguration> configs = {
      {kSbMediaAudioConnectorUnknown, 0, kSbMediaAudioCodingTypeAc3, 2},
      {kSbMediaAudioConnectorUnknown, 0,
       kSbMediaAudioCodingTypeDolbyDigitalPlus, 6},
  };
  EXPECT_CALL(*mock_audio_config_provider_, GetAudioConfigurations())
      .WillOnce(Return(configs));

  SbMediaAudioConfiguration config;
  EXPECT_TRUE(cache_->GetAudioConfiguration(0, &config));
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypeAc3);
  EXPECT_EQ(config.number_of_channels, 2);

  EXPECT_TRUE(cache_->GetAudioConfiguration(1, &config));
  EXPECT_EQ(config.coding_type, kSbMediaAudioCodingTypeDolbyDigitalPlus);
  EXPECT_EQ(config.number_of_channels, 6);

  EXPECT_FALSE(cache_->GetAudioConfiguration(2, &config));
}

TEST_F(MediaCapabilitiesCacheTest, ClearCache) {
  EXPECT_CALL(*mock_drm_provider_, IsWidevineSupported()).Times(2);
  EXPECT_TRUE(cache_->IsWidevineSupported());
  cache_->ClearCache();
  EXPECT_TRUE(cache_->IsWidevineSupported());
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
