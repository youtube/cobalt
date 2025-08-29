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

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// #include "cobalt/android/jni_headers/MediaCodecUtil_jni.h"

// Note: This test suite requires a test environment where the real JNI function
// implementations can be replaced by the mock implementations provided below.

namespace starboard {
namespace android {
namespace shared {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

// --- Mock JNI Environment ---
// We create a mock class to control the behavior of the JNI functions.
class MockJni {
 public:
  MOCK_METHOD(base::android::ScopedJavaLocalRef<jobjectArray>,
              GetAllCodecCapabilityInfos,
              (JNIEnv*));
  MOCK_METHOD(bool, IsWidevineSupported, (JNIEnv*));
};

// Global mock object instance.
MockJni* g_mock_jni = nullptr;

// --- JNI Function Fakes ---
// These are the fake implementations of the JNI functions that the class under
// test will call. They delegate to our global mock object.

extern "C" {
// Fake implementation for MediaCodecUtil.java
base::android::ScopedJavaLocalRef<jobjectArray>
JNI_MediaCodecUtil_GetAllCodecCapabilityInfos(JNIEnv* env) {
  SB_CHECK(g_mock_jni);
  return g_mock_jni->GetAllCodecCapabilityInfos(env);
}

// Fake implementation for MediaDrmBridge.java
bool JNI_MediaDrmBridge_IsWidevineSupported(JNIEnv* env) {
  SB_CHECK(g_mock_jni);
  return g_mock_jni->IsWidevineSupported(env);
}

// Provide stubs for other unneeded JNI functions to allow compilation.
bool JNI_MediaDrmBridge_IsCbcsSupported(JNIEnv* env) {
  return false;
}
base::android::ScopedJavaLocalRef<jintArray>
JNI_StarboardBridge_GetSupportedHdrTypes(JNIEnv* env) {
  return base::android::ScopedJavaLocalRef<jintArray>();
}
}  // extern "C"

// --- Test Fixture ---
class MediaCapabilitiesCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create and install the global mock object before each test.
    g_mock_jni = new MockJni();
    // Ensure the cache is cleared before each test to prevent state leakage.
    cache_ = MediaCapabilitiesCache::GetInstance();
    cache_->ClearCache();
  }

  void TearDown() override {
    // Clean up the global mock object.
    delete g_mock_jni;
    g_mock_jni = nullptr;
  }

  // Helper function to create a dummy jobjectArray. In a real test, this would
  // be populated with mock codec info objects. For this example, returning a
  // null object is sufficient to test the call count.
  base::android::ScopedJavaLocalRef<jobjectArray> CreateFakeCodecInfoArray() {
    JNIEnv* env = base::android::AttachCurrentThread();
    jobjectArray fake_array =
        env->NewObjectArray(0, env->FindClass("java/lang/Object"), NULL);
    return base::android::ScopedJavaLocalRef<jobjectArray>(env, fake_array);
  }

  MockJni& mock() { return *g_mock_jni; }

  MediaCapabilitiesCache* cache_ = nullptr;
};

// --- Test Cases ---

TEST_F(MediaCapabilitiesCacheTest, CacheIsUsedForWidevineSupport) {
  // Expect the JNI function to be called only once.
  EXPECT_CALL(mock(), IsWidevineSupported(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(mock(), GetAllCodecCapabilityInfos(_))
      .Times(1)
      .WillOnce(Return(ByMove(CreateFakeCodecInfoArray())));

  // First call: Should trigger JNI call and populate the cache.
  bool supported1 = cache_->IsWidevineSupported();
  EXPECT_TRUE(supported1);

  // Second call: Should be served from the cache, not triggering a JNI call.
  bool supported2 = cache_->IsWidevineSupported();
  EXPECT_TRUE(supported2);
}

TEST_F(MediaCapabilitiesCacheTest, CacheIsClearedAndRepopulated) {
  // Expect JNI functions to be called twice because we clear the cache.
  EXPECT_CALL(mock(), IsWidevineSupported(_))
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(mock(), GetAllCodecCapabilityInfos(_))
      .Times(2)
      .WillRepeatedly(Return(ByMove(CreateFakeCodecInfoArray())));

  // First call: Populates the cache.
  bool supported1 = cache_->IsWidevineSupported();
  EXPECT_TRUE(supported1);

  // Second call: Hits the cache.
  bool supported2 = cache_->IsWidevineSupported();
  EXPECT_TRUE(supported2);

  // Clear the cache. This should invalidate all stored values.
  cache_->ClearCache();

  // Third call: Should miss the cache and call JNI again, getting the new
  // value.
  bool supported3 = cache_->IsWidevineSupported();
  EXPECT_FALSE(supported3);
}

TEST_F(MediaCapabilitiesCacheTest, CacheIsBypassedWhenDisabled) {
  cache_->ClearCache();

  // When disabled, every call should go directly to the JNI function.
  EXPECT_CALL(mock(), IsWidevineSupported(_))
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  // We do not expect GetAllCodecCapabilityInfos to be called when the cache is
  // disabled.
  EXPECT_CALL(mock(), GetAllCodecCapabilityInfos(_)).Times(0);

  // Call twice and expect the underlying JNI function to be called each time.
  EXPECT_TRUE(cache_->IsWidevineSupported());
  EXPECT_FALSE(cache_->IsWidevineSupported());
}

TEST_F(MediaCapabilitiesCacheTest, FindAudioDecoderPopulatesCache) {
  // Expect the codec info JNI function to be called only once.
  EXPECT_CALL(mock(), GetAllCodecCapabilityInfos(_))
      .Times(1)
      .WillOnce(Return(ByMove(CreateFakeCodecInfoArray())));

  // First call: Should trigger JNI call to populate the cache.
  // We expect an empty string because our mock returns no codecs.
  EXPECT_TRUE(cache_->FindAudioDecoder("audio/mp4a-latm", 128000).empty());

  // Second call: Should be served from the cache.
  EXPECT_TRUE(cache_->FindAudioDecoder("audio/opus", 128000).empty());
}

TEST_F(MediaCapabilitiesCacheTest, FindVideoDecoderPopulatesCache) {
  // Expect the codec info JNI function to be called only once.
  EXPECT_CALL(mock(), GetAllCodecCapabilityInfos(_))
      .Times(1)
      .WillOnce(Return(ByMove(CreateFakeCodecInfoArray())));

  // First call: Should trigger JNI call to populate the cache.
  EXPECT_TRUE(cache_
                  ->FindVideoDecoder("video/av01", /*must_support_secure=*/true,
                                     /*must_support_hdr=*/false,
                                     /*require_software_codec=*/false,
                                     /*must_support_tunnel_mode=*/false,
                                     /*frame_width=*/1920,
                                     /*frame_height=*/1080,
                                     /*bitrate=*/5000000, /*fps=*/30)
                  .empty());

  // Second call: Should be served from the cache.
  EXPECT_TRUE(cache_
                  ->FindVideoDecoder("video/avc", /*must_support_secure=*/false,
                                     /*must_support_hdr=*/false,
                                     /*require_software_codec=*/false,
                                     /*must_support_tunnel_mode=*/false,
                                     /*frame_width=*/1280, /*frame_height=*/720,
                                     /*bitrate=*/2000000, /*fps=*/30)
                  .empty());
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
