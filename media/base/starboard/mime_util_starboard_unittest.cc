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

#include <string>
#include <vector>

#include "media/base/mime_util.h"
#include "media/base/mime_util_internal.h"
#include "media/base/starboard/sbmedia_interface.h"
#include "starboard/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using ::testing::AnyOf;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::StrEq;

// A mock implementation of SbMediaInterface for unit testing the media
// pipeline's interaction with the Starboard media layer. This class is
// typically owned by the test fixture or instantiated as a local variable
// within a test, and is thread-safe.
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

// Test fixture for MimeUtil's Starboard code paths. When USE_STARBOARD_MEDIA
// is enabled, MimeUtil delegates MIME support queries to Starboard via
// SbMediaInterface instead of using upstream Chromium's codec parsing and
// |media_format_map_| lookup. The fixture installs a mock interface so those
// delegating code paths can be observed directly.
//
// This class is owned and managed by the gtest framework, with a lifetime
// spanning a single test case execution. It is thread-affine to the main
// test thread.
class MimeUtilStarboardTest : public ::testing::Test {
 protected:
  void SetUp() override { SetSbMediaInterfaceForTesting(&mock_interface_); }

  void TearDown() override { SetSbMediaInterfaceForTesting(nullptr); }

  MockSbMediaInterface mock_interface_;
};

TEST_F(MimeUtilStarboardTest, IsSupportedMediaMimeTypeForwardsRawMime) {
  internal::MimeUtil mime_util;

  const std::string kMimeProbably =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus; test_id=1";
  const std::string kMimeMaybe =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus; test_id=2";
  const std::string kMimeNotSupported =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus; test_id=3";

  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kMimeProbably.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeProbably));
  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kMimeMaybe.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeMaybe));
  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kMimeNotSupported.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeNotSupported));

  EXPECT_TRUE(mime_util.IsSupportedMediaMimeType(kMimeProbably));
  EXPECT_TRUE(mime_util.IsSupportedMediaMimeType(kMimeMaybe));
  EXPECT_FALSE(mime_util.IsSupportedMediaMimeType(kMimeNotSupported));
}

TEST_F(MimeUtilStarboardTest, IsSupportedMediaFormatMapsSupportType) {
  internal::MimeUtil mime_util;

  const std::string kMimeProbably =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true; test_id=1";
  const std::string kMimeMaybe =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true; test_id=2";
  const std::string kMimeNotSupported =
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "tunnelmode=true; test_id=3";
  const std::vector<std::string> kCodecs = {"avc1.64002a"};

  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kMimeProbably.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeProbably));
  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kMimeMaybe.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeMaybe));
  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kMimeNotSupported.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeNotSupported));

  EXPECT_EQ(mime_util.IsSupportedMediaFormat(kMimeProbably, kCodecs,
                                             /*is_encrypted=*/false),
            SupportsType::kSupported);
  EXPECT_EQ(mime_util.IsSupportedMediaFormat(kMimeMaybe, kCodecs,
                                             /*is_encrypted=*/false),
            SupportsType::kMaybeSupported);
  EXPECT_EQ(mime_util.IsSupportedMediaFormat(kMimeNotSupported, kCodecs,
                                             /*is_encrypted=*/false),
            SupportsType::kNotSupported);
}

// An executable inventory of every custom MIME parameter Cobalt is expected to
// forward to Starboard untouched. The MIME string is kept byte-for-byte
// identical to the one in
// CustomMimeTypeBrowserTest.MediaSourceIsTypeSupported_ForwardsRawCustomAttributes
// so that this serves as a fast unit-level backstop for that browser test.
TEST_F(MimeUtilStarboardTest,
       IsSupportedMediaMimeTypeForwardsAllCustomParameters) {
  internal::MimeUtil mime_util;

  const std::string kAllParamsMime =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "framerate=60; bitrate=20000000; hdr=hdr10plus; eotf=smpte2084; "
      "color_primaries=bt2020; matrix=bt2020nc; tunnelmode=true; "
      "softwaredecoder=false; disablecache=true; "
      "disabledynamicprerollframecount=true; enableflushduringseek=true; "
      "encryptionscheme=cenc;";

  EXPECT_CALL(mock_interface_,
              CanPlayMimeAndKeySystem(StrEq(kAllParamsMime.c_str()),
                                      AnyOf(IsNull(), StrEq(""))))
      .WillOnce(Return(kSbMediaSupportTypeProbably));

  EXPECT_TRUE(mime_util.IsSupportedMediaMimeType(kAllParamsMime));
}

}  // namespace
}  // namespace media
