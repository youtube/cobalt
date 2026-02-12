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

#include "starboard/android/shared/media/media_capabilities_cache.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::testing::ByMove;
using ::testing::Return;
using ::testing::SetArgPointee;

class MockMediaCapabilitiesProvider : public MediaCapabilitiesProvider {
 public:
  MOCK_METHOD(bool, GetIsWidevineSupported, (), (override));
  MOCK_METHOD(bool, GetIsCbcsSchemeSupported, (), (override));
  MOCK_METHOD(std::set<SbMediaTransferId>,
              GetSupportedHdrTypes,
              (),
              (override));
  MOCK_METHOD(bool,
              GetIsPassthroughSupported,
              (SbMediaAudioCodec codec),
              (override));
  MOCK_METHOD(bool,
              GetAudioConfiguration,
              (int index, SbMediaAudioConfiguration* configuration),
              (override));
  MOCK_METHOD(
      void,
      GetCodecCapabilities,
      ((std::map<std::string, AudioCodecCapabilities>)&audio_codec_capabilities,
       (std::map<std::string,
                 VideoCodecCapabilities>)&video_codec_capabilities),
      (override));
};

class MediaCapabilitiesCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mock_media_capabilities_provider =
        std::make_unique<MockMediaCapabilitiesProvider>();
    mock_media_capabilities_provider_ = mock_media_capabilities_provider.get();

    cache_ = MediaCapabilitiesCache::CreateForTest(
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
}  // namespace
}  // namespace starboard
