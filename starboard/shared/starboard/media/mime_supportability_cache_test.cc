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

#include "starboard/shared/starboard/media/mime_supportability_cache.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr char kMime[] = "video/mp4; codecs=\"avc1.640028\"; width=1920";

std::string WithParams(const std::string& params) {
  return std::string(kMime) + params;
}

class MimeSupportabilityCacheTest : public ::testing::Test {
 protected:
  // MimeSupportabilityCache is a singleton, so restore its state after tests.
  void SetUp() override {
    was_enabled_ = cache_->IsEnabled();
    cache_->SetCacheEnabled(true);
    cache_->ClearCachedMimeSupportabilities();
  }
  void TearDown() override {
    cache_->ClearCachedMimeSupportabilities();
    cache_->SetCacheEnabled(was_enabled_);
  }

  MimeSupportabilityCache* cache_ = MimeSupportabilityCache::GetInstance();
  bool was_enabled_ = false;
};

TEST_F(MimeSupportabilityCacheTest, SharesEntryAcrossBitrates) {
  cache_->CacheMimeSupportability(WithParams("; bitrate=2000").c_str(),
                                  kSupportabilitySupported);
  cache_->CacheMimeSupportability(WithParams("; bitrate=4000").c_str(),
                                  kSupportabilityNotSupported);

  EXPECT_EQ(cache_->GetMimeSupportability(WithParams("; bitrate=1000").c_str())
                .supportability,
            kSupportabilitySupported);
  EXPECT_EQ(cache_->GetMimeSupportability(WithParams("; bitrate=3000").c_str())
                .supportability,
            kSupportabilityUnknown);
  EXPECT_EQ(cache_->GetMimeSupportability(WithParams("; bitrate=5000").c_str())
                .supportability,
            kSupportabilityNotSupported);
}

TEST_F(MimeSupportabilityCacheTest, ReturnsRequestedBitrate) {
  cache_->CacheMimeSupportability(WithParams("; bitrate=2000").c_str(),
                                  kSupportabilitySupported);

  auto result =
      cache_->GetMimeSupportability(WithParams("; bitrate=1000").c_str());

  ASSERT_TRUE(result.mime_info);
  EXPECT_EQ(result.mime_info->video_info().bitrate, 1000);
}

TEST_F(MimeSupportabilityCacheTest, BitrateNameIsCaseInsensitive) {
  cache_->CacheMimeSupportability(
      WithParams("; bitrate=2000; height=1080").c_str(),
      kSupportabilitySupported);

  auto result = cache_->GetMimeSupportability(
      WithParams("; height=1080; BITRATE=1000").c_str());

  EXPECT_EQ(result.supportability, kSupportabilitySupported);
  ASSERT_TRUE(result.mime_info);
  EXPECT_EQ(result.mime_info->video_info().bitrate, 1000);
}

TEST_F(MimeSupportabilityCacheTest, KeyIgnoresWhitespaceAndQuoting) {
  cache_->CacheMimeSupportability(
      "video/mp4;codecs=avc1.640028;width=1280;bitrate=2000",
      kSupportabilitySupported);

  auto result = cache_->GetMimeSupportability(
      "video/mp4; codecs=\"avc1.640028\" ; width=1280 ; bitrate=1000");

  EXPECT_EQ(result.supportability, kSupportabilitySupported);
}

TEST_F(MimeSupportabilityCacheTest, InvalidBitrate) {
  auto result =
      cache_->GetMimeSupportability(WithParams("; Bitrate=abc").c_str());

  EXPECT_EQ(result.supportability, kSupportabilityNotSupported);
  EXPECT_FALSE(result.mime_info);
}

TEST_F(MimeSupportabilityCacheTest, NegativeBitrate) {
  auto result =
      cache_->GetMimeSupportability(WithParams("; bitrate=-1").c_str());

  EXPECT_EQ(result.supportability, kSupportabilityNotSupported);
  EXPECT_FALSE(result.mime_info);
}

TEST_F(MimeSupportabilityCacheTest, InvalidMime) {
  auto result = cache_->GetMimeSupportability("not a mime; bitrate=1000");

  EXPECT_EQ(result.supportability, kSupportabilityNotSupported);
  EXPECT_FALSE(result.mime_info);
}

TEST_F(MimeSupportabilityCacheTest, SimilarParamNameIsNotBitrate) {
  auto result =
      cache_->GetMimeSupportability(WithParams("; mybitrate=1000").c_str());

  ASSERT_TRUE(result.mime_info);
  EXPECT_EQ(result.mime_info->video_info().bitrate, 0);
}

}  // namespace
}  // namespace starboard
