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

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr char kMime[] = "video/mp4; codecs=\"avc1.640028\"; width=1920";

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

TEST_F(MimeSupportabilityCacheTest, BitrateNameIsCaseInsensitive) {
  cache_->CacheMimeSupportability(
      (std::string(kMime) + "; bitrate=2000; height=1080").c_str(),
      kSupportabilitySupported);

  auto result = cache_->GetMimeSupportability(
      (std::string(kMime) + "; height=1080; BITRATE=1000").c_str());

  EXPECT_EQ(result.supportability, kSupportabilitySupported);
  ASSERT_TRUE(result.mime_info);
  EXPECT_EQ(result.mime_info->video_info().bitrate, 1000);
}

TEST_F(MimeSupportabilityCacheTest, InvalidBitrate) {
  auto result = cache_->GetMimeSupportability(
      (std::string(kMime) + "; Bitrate=abc").c_str());

  EXPECT_EQ(result.supportability, kSupportabilityNotSupported);
  EXPECT_FALSE(result.mime_info);
}

TEST_F(MimeSupportabilityCacheTest, SimilarParamNameIsNotBitrate) {
  auto result = cache_->GetMimeSupportability(
      (std::string(kMime) + "; mybitrate=1000").c_str());

  ASSERT_TRUE(result.mime_info);
  EXPECT_EQ(result.mime_info->video_info().bitrate, 0);
}

}  // namespace
}  // namespace starboard
