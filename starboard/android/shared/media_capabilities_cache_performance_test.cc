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
#include <mutex>

#include "starboard/common/log.h"

namespace starboard {
namespace {

// 200 milliseconds is chosen for the time limit on uncached calls, mainly due
// to the amount of time it potentially could take for a MediaCapabilitiesCache
// query on an empty cache.
//
// Cached queries are extremely fast, and usually are timed to take 0
// milliseconds for a call. We set the cache time limit to 5 ms, to allow some
// leeway if the function call is slowed down due to other external means.
constexpr int64_t kUncacheTimeLimitMs = 200;
constexpr int64_t kCacheTimeLimitMs = 5;

// Since these tests rely on the same resource, it is possible that one test
// could mess up the status of one test if ran in parallel. To make sure this
// doesn't happen, we use a mutex to lock each test when ran.
std::mutex media_cache_test_mutex;

class MediaCapabilitiesCachePerformanceTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    JNIEnv* env = base::android::AttachCurrentThread();
    StarboardBridge::InitializeForTesting(env);

    // Make an initial call to MediaCapabilitiesCache, as the first ever call to
    // the cache can be extremely slow (up to 700 ms). This behavior is
    // something that we don't want influencing the tests.
    MediaCapabilitiesCache::GetInstance()->IsWidevineSupported();
  }

  void SetUp() override {
    // Lock the test so that other tests can't start while this one is running.
    media_cache_test_mutex.lock();
    instance_ = MediaCapabilitiesCache::GetInstance();
    instance_->ClearCache();
    MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
  }

  void TearDown() override { media_cache_test_mutex.unlock(); }

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

  const int64_t uncached_duration_ms = measure_duration_ms();
  SB_LOG(INFO) << test_name
               << " - 1st call (uncached): " << uncached_duration_ms
               << " milliseconds.";
  EXPECT_LT(uncached_duration_ms, kUncacheTimeLimitMs);

  const int64_t cached_duration_ms = measure_duration_ms();
  SB_LOG(INFO) << test_name << " - 2nd call (cached): " << cached_duration_ms
               << " milliseconds.";
  EXPECT_LT(cached_duration_ms, kCacheTimeLimitMs);
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
  TimeFunctionCall("IsHDRTransferCharacteristicsSupported (SmpteSt2084)",
                   [&]() {
                     instance_->IsHDRTransferCharacteristicsSupported(
                         /*transfer_id=*/kSbMediaTransferIdSmpteSt2084);
                   });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, IsPassthroughSupported) {
  TimeFunctionCall("IsPassthroughSupported (AC3)", [&]() {
    instance_->IsPassthroughSupported(/*codec=*/kSbMediaAudioCodecAc3);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, GetAudioConfiguration) {
  SbMediaAudioConfiguration configuration;
  TimeFunctionCall("GetAudioConfiguration", [&]() {
    instance_->GetAudioConfiguration(/*index=*/0, &configuration);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, HasAudioDecoderFor) {
  TimeFunctionCall("HasAudioDecoderFor", [&]() {
    instance_->HasAudioDecoderFor(
        /*mime_type=*/"audio/mp4; codecs=\"mp4a.40.2\"",
        /*bitrate=*/192'000);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, HasVideoDecoderFor) {
  TimeFunctionCall("HasVideoDecoderFor", [&]() {
    instance_->HasVideoDecoderFor(
        /*mime_type=*/"video/avc; codecs=\"avc1.4d401f\"",
        /*must_support_secure=*/false,
        /*must_support_hdr=*/false,
        /*must_support_tunnel_mode=*/false,
        /*frame_width=*/1'920,
        /*frame_height=*/1'080,
        /*bitrate=*/8'000'000,
        /*fps=*/30);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, FindAudioDecoder) {
  TimeFunctionCall("FindAudioDecoder", [&]() {
    instance_->FindAudioDecoder(/*mime_type=*/"audio/eac-3",
                                /*bitrate=*/640'000);
  });
}

TEST_F(MediaCapabilitiesCachePerformanceTest, FindVideoDecoder) {
  TimeFunctionCall("FindVideoDecoder", [&]() {
    instance_->FindVideoDecoder(
        /*mime_type=*/"video/x-vnd.on2.vp9",
        /*must_support_secure=*/false,
        /*must_support_hdr=*/false,
        /*require_software_codec=*/false,
        /*must_support_tunnel_mode=*/false,
        /*frame_width=*/1'920,
        /*frame_height=*/1'080,
        /*bitrate=*/8'000'000,
        /*fps=*/30);
  });
}

}  // namespace
}  // namespace starboard
