// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/starboard/sbmedia_interface.h"

#include <cstring>
#include <string>
#include <vector>

#include "media/base/mime_util.h"
#include "media/base/mime_util_internal.h"
#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace media {
namespace {

// A mock implementation of SbMediaInterface for unit testing the media
// pipeline's interaction with the Starboard media layer. This class is
// thread-safe.
class MockSbMediaInterface : public SbMediaInterface {
 public:
  MockSbMediaInterface() = default;
  ~MockSbMediaInterface() override = default;

  MOCK_METHOD(SbMediaSupportType,
              CanPlayMimeAndKeySystem,
              (const char* mime, const char* key_system),
              (const, override));
  MOCK_METHOD(bool,
              CanChangeType,
              (const char* current_mime, const char* new_mime),
              (const, override));
  MOCK_METHOD(int, GetAudioOutputCount, (), (const, override));
  MOCK_METHOD(bool,
              GetAudioConfiguration,
              (int output_index, SbMediaAudioConfiguration* out_configuration),
              (const, override));
  MOCK_METHOD(int, GetBufferAllocationUnit, (), (const, override));
  MOCK_METHOD(int, GetAudioBufferBudget, (), (const, override));
  MOCK_METHOD(int64_t,
              GetBufferGarbageCollectionDurationThreshold,
              (),
              (const, override));
  MOCK_METHOD(int, GetInitialBufferCapacity, (), (const, override));
  MOCK_METHOD(bool, IsBufferPoolAllocateOnDemand, (), (const, override));
  MOCK_METHOD(int,
              GetVideoBufferBudget,
              (SbMediaVideoCodec codec,
               int resolution_width,
               int resolution_height,
               int bits_per_pixel),
              (const, override));
};

// Test fixture for testing SbMediaInterface and its integration with MimeUtil.
// This class is thread-affine to the main test thread.
class SbMediaInterfaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Ensure clean state before each test.
    SetSbMediaInterfaceForTesting(nullptr);
  }

  void TearDown() override {
    // Restore default interface after each test.
    SetSbMediaInterfaceForTesting(nullptr);
  }
};

TEST_F(SbMediaInterfaceTest, DefaultInterfaceReturnedWhenNotOverridden) {
  SbMediaInterface* media_interface = GetSbMediaInterface();
  ASSERT_NE(media_interface, nullptr);
}

TEST_F(SbMediaInterfaceTest, SetAndResetTestingInterface) {
  SbMediaInterface* default_interface = GetSbMediaInterface();
  ASSERT_NE(default_interface, nullptr);

  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);
  EXPECT_EQ(GetSbMediaInterface(), &mock_interface);

  SetSbMediaInterfaceForTesting(nullptr);
  EXPECT_EQ(GetSbMediaInterface(), default_interface);
}

TEST_F(SbMediaInterfaceTest, MockCanPlayMimeAndKeySystem) {
  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);

  const char kMime[] =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus";
  const char kKeySystem[] = "com.widevine.alpha";

  EXPECT_CALL(mock_interface,
              CanPlayMimeAndKeySystem(StrEq(kMime), StrEq(kKeySystem)))
      .WillOnce(Return(kSbMediaSupportTypeProbably));

  EXPECT_EQ(GetSbMediaInterface()->CanPlayMimeAndKeySystem(kMime, kKeySystem),
            kSbMediaSupportTypeProbably);
}

TEST_F(SbMediaInterfaceTest, MockCanChangeType) {
  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);

  const char kCurrentMime[] = "video/mp4; codecs=\"avc1.64002a\"";
  const char kNewMime[] = "video/webm; codecs=\"vp9\"";

  EXPECT_CALL(mock_interface,
              CanChangeType(StrEq(kCurrentMime), StrEq(kNewMime)))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_TRUE(GetSbMediaInterface()->CanChangeType(kCurrentMime, kNewMime));
  EXPECT_FALSE(GetSbMediaInterface()->CanChangeType(kCurrentMime, kNewMime));
}

TEST_F(SbMediaInterfaceTest, MockAudioOutputAndConfiguration) {
  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);

  EXPECT_CALL(mock_interface, GetAudioOutputCount()).WillOnce(Return(2));
  EXPECT_EQ(GetSbMediaInterface()->GetAudioOutputCount(), 2);

  SbMediaAudioConfiguration expected_config = {};
  expected_config.number_of_channels = 6;
  expected_config.latency = 10000;
  expected_config.coding_type = kSbMediaAudioCodingTypePcm;
  expected_config.connector = kSbMediaAudioConnectorHdmi;

  EXPECT_CALL(mock_interface, GetAudioConfiguration(0, _))
      .WillOnce(DoAll(SetArgPointee<1>(expected_config), Return(true)));

  SbMediaAudioConfiguration actual_config = {};
  EXPECT_TRUE(GetSbMediaInterface()->GetAudioConfiguration(0, &actual_config));
  EXPECT_EQ(actual_config.number_of_channels, 6);
  EXPECT_EQ(actual_config.latency, 10000);
  EXPECT_EQ(actual_config.coding_type, kSbMediaAudioCodingTypePcm);
  EXPECT_EQ(actual_config.connector, kSbMediaAudioConnectorHdmi);
}

TEST_F(SbMediaInterfaceTest, MockBufferParametersAndBudgets) {
  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);

  EXPECT_CALL(mock_interface, GetBufferAllocationUnit())
      .WillOnce(Return(65536));
  EXPECT_EQ(GetSbMediaInterface()->GetBufferAllocationUnit(), 65536);

  EXPECT_CALL(mock_interface, GetAudioBufferBudget())
      .WillOnce(Return(5 * 1024 * 1024));
  EXPECT_EQ(GetSbMediaInterface()->GetAudioBufferBudget(), 5 * 1024 * 1024);

  EXPECT_CALL(mock_interface, GetBufferGarbageCollectionDurationThreshold())
      .WillOnce(Return(30000000LL));
  EXPECT_EQ(
      GetSbMediaInterface()->GetBufferGarbageCollectionDurationThreshold(),
      30000000LL);

  EXPECT_CALL(mock_interface, GetInitialBufferCapacity())
      .WillOnce(Return(1024 * 1024));
  EXPECT_EQ(GetSbMediaInterface()->GetInitialBufferCapacity(), 1024 * 1024);

  EXPECT_CALL(mock_interface, IsBufferPoolAllocateOnDemand())
      .WillOnce(Return(true));
  EXPECT_TRUE(GetSbMediaInterface()->IsBufferPoolAllocateOnDemand());

  EXPECT_CALL(mock_interface,
              GetVideoBufferBudget(kSbMediaVideoCodecH264, 3840, 2160, 8))
      .WillOnce(Return(100 * 1024 * 1024));
  EXPECT_EQ(GetSbMediaInterface()->GetVideoBufferBudget(kSbMediaVideoCodecH264,
                                                        3840, 2160, 8),
            100 * 1024 * 1024);
}

TEST_F(SbMediaInterfaceTest,
       MimeUtilIsSupportedMediaMimeTypePreservesAttributes) {
  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);

  internal::MimeUtil mime_util;

  const std::string kCustomMime =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus";

  EXPECT_CALL(mock_interface,
              CanPlayMimeAndKeySystem(StrEq(kCustomMime.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeProbably))
      .WillOnce(Return(kSbMediaSupportTypeMaybe))
      .WillOnce(Return(kSbMediaSupportTypeNotSupported));

  EXPECT_TRUE(mime_util.IsSupportedMediaMimeType(kCustomMime));
  EXPECT_TRUE(mime_util.IsSupportedMediaMimeType(kCustomMime));
  EXPECT_FALSE(mime_util.IsSupportedMediaMimeType(kCustomMime));
}

TEST_F(SbMediaInterfaceTest, MimeUtilIsSupportedMediaFormatSupportTypeMapping) {
  MockSbMediaInterface mock_interface;
  SetSbMediaInterfaceForTesting(&mock_interface);

  internal::MimeUtil mime_util;

  const std::string kCustomMime =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true";
  const std::vector<std::string> kCodecs = {"avc1.64002a"};

  EXPECT_CALL(mock_interface,
              CanPlayMimeAndKeySystem(StrEq(kCustomMime.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeProbably))
      .WillOnce(Return(kSbMediaSupportTypeMaybe))
      .WillOnce(Return(kSbMediaSupportTypeNotSupported));

  EXPECT_EQ(mime_util.IsSupportedMediaFormat(kCustomMime, kCodecs,
                                             /*is_encrypted=*/false),
            SupportsType::kSupported);
  EXPECT_EQ(mime_util.IsSupportedMediaFormat(kCustomMime, kCodecs,
                                             /*is_encrypted=*/false),
            SupportsType::kMaybeSupported);
  EXPECT_EQ(mime_util.IsSupportedMediaFormat(kCustomMime, kCodecs,
                                             /*is_encrypted=*/false),
            SupportsType::kNotSupported);
}

}  // namespace
}  // namespace media
