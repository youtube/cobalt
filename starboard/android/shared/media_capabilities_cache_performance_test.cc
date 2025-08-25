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
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"

#include <gtest/gtest.h>
#include <chrono>

#include "starboard/common/log.h"

namespace starboard {
namespace {

// 200 milliseconds is chosen for the time limit on uncached calls, mainly due
// to the amount of time it potentially could take for a MediaCapabilitiesCache
// query on a cold start. Once the cache is warmed up, uncached queries are much
// faster.
//
// Cached queries are extremely fast, and usually are timed to take 0
// milliseconds for a call. We set the cache time limit to 5 ms, to allow some
// leeway if the function call is slowed down due to other external means.
const int64_t kUncacheTimeLimitMs = 200;
const int64_t kCacheTimeLimitMs = 5;

class MediaCapabilitiesCachePerformanceTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    JNIEnv* env = base::android::AttachCurrentThread();
    StarboardBridge::InitializeForTesting(env);
  }

  void SetUp() override {
    instance_ = MediaCapabilitiesCache::GetInstance();
    instance_->ClearCache();
    MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
  }

  MediaCapabilitiesCache* instance_ = nullptr;
};

template <typename Func>
void TimeFunctionCall(const std::string& test_name, Func func) {
  auto measure_duration_ms = [&]() {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
  };

  const int64_t uncached_duration = measure_duration_ms();
  SB_LOG(INFO) << test_name << " - 1st call (uncached): " << uncached_duration
               << " milliseconds.";
  EXPECT_LT(uncached_duration, kUncacheTimeLimitMs);

  const int64_t cached_duration = measure_duration_ms();
  SB_LOG(INFO) << test_name << " - 2nd call (cached): " << cached_duration
               << " milliseconds.";
  EXPECT_LT(cached_duration, kCacheTimeLimitMs);
}

TEST_F(MediaCapabilitiesCachePerformanceTest, IsWidevineSupported) {
  TimeFunctionCall("IsWidevineSupported",
                   [&]() { instance_->IsWidevineSupported(); });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, IsCbcsSchemeSupported) {
  TimeFunctionCall("IsCbcsSchemeSupported",
                   [&]() { instance_->IsCbcsSchemeSupported(); });
}

TEST_F(MediaCapabilitiesCachePerformanceTest,
       IsHDRTransferCharacteristicsSupported) {
  const SbMediaTransferId smpte_st_2084 = kSbMediaTransferIdSmpteSt2084;
  TimeFunctionCall(
      "IsHDRTransferCharacteristicsSupported (SmpteSt2084)", [&]() {
        instance_->IsHDRTransferCharacteristicsSupported(smpte_st_2084);
      });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, IsPassthroughSupported) {
  const SbMediaAudioCodec ac3_codec = kSbMediaAudioCodecAc3;
  TimeFunctionCall("IsPassthroughSupported (AC3)",
                   [&]() { instance_->IsPassthroughSupported(ac3_codec); });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, GetAudioConfiguration) {
  const int index = 0;
  SbMediaAudioConfiguration configuration;
  TimeFunctionCall("GetAudioConfiguration", [&]() {
    instance_->GetAudioConfiguration(index, &configuration);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, HasAudioDecoderFor) {
  const std::string mime_type = "audio/mp4; codecs=\"mp4a.40.2\"";
  const int bitrate = 192000;
  TimeFunctionCall("HasAudioDecoderFor", [&]() {
    instance_->HasAudioDecoderFor(mime_type, bitrate);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, HasVideoDecoderFor) {
  const std::string mime_type = "video/avc; codecs=\"avc1.4d401f\"";
  const bool must_support_secure = false;
  const bool must_support_hdr = false;
  const bool must_support_tunnel_mode = false;
  const int frame_width = 1920;
  const int frame_height = 1080;
  const int bitrate = 8000000;
  const int fps = 30;
  TimeFunctionCall("HasVideoDecoderFor", [&]() {
    instance_->HasVideoDecoderFor(mime_type, must_support_secure,
                                  must_support_hdr, must_support_tunnel_mode,
                                  frame_width, frame_height, bitrate, fps);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, FindAudioDecoder) {
  const std::string mime_type = "audio/eac-3";
  const int bitrate = 640000;
  TimeFunctionCall("FindAudioDecoder",
                   [&]() { instance_->FindAudioDecoder(mime_type, bitrate); });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, FindVideoDecoder) {
  const std::string mime_type = "video/x-vnd.on2.vp9";
  const bool must_support_secure = false;
  const bool must_support_hdr = false;
  const bool require_software_codec = false;
  const bool must_support_tunnel_mode = false;
  const int frame_width = 1920;
  const int frame_height = 1080;
  const int bitrate = 8000000;
  const int fps = 30;
  TimeFunctionCall("FindVideoDecoder", [&]() {
    instance_->FindVideoDecoder(mime_type, must_support_secure,
                                must_support_hdr, require_software_codec,
                                must_support_tunnel_mode, frame_width,
                                frame_height, bitrate, fps);
  });
}

}  // namespace
}  // namespace starboard
