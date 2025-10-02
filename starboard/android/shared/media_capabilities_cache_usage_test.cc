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

#include "starboard/android/shared/media_capabilities_cache.h"

#include "starboard/shared/starboard/media/media_support_internal.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace starboard {
namespace {

class MockMediaCapabilitiesCache final : public MediaCapabilitiesCache {
 public:
  MockMediaCapabilitiesCache() {}
  ~MockMediaCapabilitiesCache() {}

  MOCK_METHOD(bool, IsWidevineSupported, (), (override));
  MOCK_METHOD(bool, IsCbcsSchemeSupported, (), (override));
  MOCK_METHOD(bool,
              IsHDRTransferCharacteristicsSupported,
              (SbMediaTransferId transfer_id),
              (override));
  MOCK_METHOD(bool,
              IsPassthroughSupported,
              (SbMediaAudioCodec codec),
              (override));
  MOCK_METHOD(bool,
              GetAudioConfiguration,
              (int index, SbMediaAudioConfiguration* configuration),
              (override));
  MOCK_METHOD(bool,
              HasAudioDecoderFor,
              (const std::string& mime_type, int bitrate),
              (override));
  MOCK_METHOD(bool,
              HasVideoDecoderFor,
              (const std::string& mime_type,
               bool must_support_secure,
               bool must_support_hdr,
               bool must_support_tunnel_mode,
               int frame_width,
               int frame_height,
               int bitrate,
               int fps),
              (override));
};

using ::testing::_;
using ::testing::Return;

MediaCapabilitiesCache* original_cache_ = nullptr;

class MediaCapabilitiesCacheTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    original_cache_ = MediaCapabilitiesCache::GetInstance();
  }

  void SetUp() {
    MediaCapabilitiesCache::SetInstanceForTesting(
        new MockMediaCapabilitiesCache());
    mock_cache_ = static_cast<MockMediaCapabilitiesCache*>(
        MediaCapabilitiesCache::GetInstance());
  }

  void TearDown() {
    MediaCapabilitiesCache::SetInstanceForTesting(nullptr);
    delete mock_cache_;
  }

  // Once the test suite finishes, manually set a new real instance of
  // MediaCapabilitiesCache, to ensure that other tests that make use of this
  // cache work properly.
  static void TearDownTestSuite() {
    MediaCapabilitiesCache::SetInstanceForTesting(original_cache_);
  }

  MockMediaCapabilitiesCache* mock_cache_ = nullptr;
};

TEST_F(MediaCapabilitiesCacheTest,
       SbMediaGetAudioConfigurationCallsGetAudioConfiguration) {
  int test_output_index = 1;
  SbMediaAudioConfiguration test_config{kSbMediaAudioConnectorUnknown, 0,
                                        kSbMediaAudioCodingTypeNone, 0};
  EXPECT_CALL(*mock_cache_,
              GetAudioConfiguration(test_output_index, &test_config))
      .Times(1);
  SbMediaGetAudioConfiguration(test_output_index, &test_config);
}

TEST_F(MediaCapabilitiesCacheTest,
       MediaIsAudioSupportedCallsHasAudioDecoderForAndIsPassthroughSupported) {
  SbMediaAudioCodec test_audio_codec = kSbMediaAudioCodecAc3;
  const MimeType* test_mime_type = nullptr;
  int64_t test_bitrate = 0;

  EXPECT_CALL(*mock_cache_, HasAudioDecoderFor(_, _))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_cache_, IsPassthroughSupported(_)).Times(1);

  MediaIsAudioSupported(test_audio_codec, test_mime_type, test_bitrate);
}

TEST_F(MediaCapabilitiesCacheTest,
       MediaIsSupportedCallsIsWideVineSupportedAndIsCbcsSchemeSupported) {
  const char* test_key_system = "com.widevine; encryptionscheme=cbcs";

  EXPECT_CALL(*mock_cache_, IsWidevineSupported())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_cache_, IsCbcsSchemeSupported()).Times(1);
  MediaIsSupported(kSbMediaVideoCodecNone, kSbMediaAudioCodecNone,
                   test_key_system);
}

TEST_F(
    MediaCapabilitiesCacheTest,
    MediaIsVideoSupportedCallsIsHDRTransferCharacteristicsSupportedAndHasVideoDecoderFor) {
  // Initialize all test Values
  SbMediaVideoCodec test_video_codec = kSbMediaVideoCodecVp9;
  MimeType* test_mime_type = nullptr;
  int test_profile = 0;
  int test_level = 0;
  int test_bit_depth = 0;
  SbMediaPrimaryId test_primary_id = kSbMediaPrimaryIdReserved0;
  SbMediaTransferId test_transfer_id = kSbMediaTransferIdReserved0;
  SbMediaMatrixId test_matrix_id = kSbMediaMatrixIdRgb;
  int test_frame_width = 0;
  int test_frame_height = 0;
  int64_t test_bitrate = 0;
  int test_fps = 0;
  bool test_decode_to_texture_required = false;

  EXPECT_CALL(*mock_cache_, IsHDRTransferCharacteristicsSupported(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_cache_, HasVideoDecoderFor(_, _, _, _, _, _, _, _))
      .Times(1);

  MediaIsVideoSupported(test_video_codec, test_mime_type, test_profile,
                        test_level, test_bit_depth, test_primary_id,
                        test_transfer_id, test_matrix_id, test_frame_width,
                        test_frame_height, test_bitrate, test_fps,
                        test_decode_to_texture_required);
}

}  // namespace
}  // namespace starboard
