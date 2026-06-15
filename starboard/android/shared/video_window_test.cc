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

#include "starboard/android/shared/video_window.h"

#include <jni.h>

#include <chrono>
#include <memory>
#include <thread>

#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/media_codec_video_decoder.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
extern "C" void Muxed_dev_cobalt_media_VideoSurfaceView_onVideoSurfaceChanged(
    JNIEnv* env,
    jobject surface);
}

namespace starboard {
namespace android {
namespace shared {
namespace {

class VideoDecoderSurfaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = jni_zero::AttachCurrentThread();
    ASSERT_NE(env_, nullptr);

    // Create a real Java Surface to pass to ANativeWindow_fromSurface.
    // 1. Find SurfaceTexture class
    jclass surface_texture_class =
        env_->FindClass("android/graphics/SurfaceTexture");
    ASSERT_NE(surface_texture_class, nullptr);

    // 2. Get constructor SurfaceTexture(int texName)
    jmethodID surface_texture_ctor =
        env_->GetMethodID(surface_texture_class, "<init>", "(I)V");
    ASSERT_NE(surface_texture_ctor, nullptr);

    // 3. Create SurfaceTexture instance with dummy texture ID 1
    jobject surface_texture =
        env_->NewObject(surface_texture_class, surface_texture_ctor, 1);
    ASSERT_NE(surface_texture, nullptr);

    // 4. Find Surface class
    jclass surface_class = env_->FindClass("android/view/Surface");
    ASSERT_NE(surface_class, nullptr);

    // 5. Get constructor Surface(SurfaceTexture surfaceTexture)
    jmethodID surface_ctor = env_->GetMethodID(
        surface_class, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
    ASSERT_NE(surface_ctor, nullptr);

    // 6. Create Surface instance
    jobject surface =
        env_->NewObject(surface_class, surface_ctor, surface_texture);
    ASSERT_NE(surface, nullptr);

    real_surface_ = jni_zero::ScopedJavaLocalRef<jobject>(env_, surface);
    ASSERT_FALSE(real_surface_.is_null());
  }

  void TearDown() override {
    // Clean up global surface state
    Muxed_dev_cobalt_media_VideoSurfaceView_onVideoSurfaceChanged(env_,
                                                                  nullptr);
  }

  JNIEnv* env_ = nullptr;
  jni_zero::ScopedJavaLocalRef<jobject> real_surface_;
};

TEST_F(VideoDecoderSurfaceTest, TeardownDuringSurfaceDestroyReleasesSurface) {
  // 1. Set the global video surface so the decoder can acquire it.
  Muxed_dev_cobalt_media_VideoSurfaceView_onVideoSurfaceChanged(
      env_, real_surface_.obj());

  // 2. Create JobThread for the decoder.
  std::unique_ptr<JobThread> job_thread = JobThread::Create("decoder_thread");
  ASSERT_NE(job_thread, nullptr);

  // 3. Create Decoder Config (punch-out mode, null surface_view to force
  // acquisition).
  MediaCodecVideoDecoder::StreamConfig stream_config{
      []() {
        VideoStreamInfo info;
        info.codec = kSbMediaVideoCodecH264;
        info.frame_size = {1920, 1080};
        return info;
      }(),
      /*drm_system=*/kSbDrmSystemInvalid,
      kSbPlayerOutputModePunchOut,
      /*decode_target_graphics_context_provider=*/nullptr,
      /*surface_view=*/nullptr,
      /*max_video_capabilities=*/""};

  MediaCodecVideoDecoder::TunnelModeConfig tunnel_config;
  MediaCodecVideoDecoder::PipelineConfig pipeline_config;
  MediaCodecVideoDecoder::PlatformOptions platform_options;

  auto factory = std::make_unique<FakeMediaCodecFactory>();

  std::unique_ptr<MediaCodecVideoDecoder> decoder;
  std::mutex init_mutex;
  std::condition_variable init_cv;
  bool init_done = false;
  bool init_success = false;

  job_thread->job_queue()->Schedule([&]() {
    auto result = MediaCodecVideoDecoder::CreateForTesting(
        std::move(factory), job_thread->job_queue(), stream_config,
        tunnel_config, pipeline_config, platform_options);
    std::lock_guard<std::mutex> lock(init_mutex);
    if (result) {
      decoder = std::move(result.value());
      init_success = true;
    }
    init_done = true;
    init_cv.notify_all();
  });

  {
    std::unique_lock<std::mutex> lock(init_mutex);
    init_cv.wait(lock, [&] { return init_done; });
  }
  ASSERT_TRUE(init_success);
  ASSERT_NE(decoder, nullptr);

  std::atomic<bool> jni_thread_done{false};

  // Thread A: Simulate JNI thread receiving surface destroyed event.
  auto start_time = std::chrono::steady_clock::now();
  std::thread jni_sim_thread([&]() {
    SB_LOG(INFO) << "JNI thread: Calling OnVideoSurfaceChanged(nullptr)...";
    Muxed_dev_cobalt_media_VideoSurfaceView_onVideoSurfaceChanged(env_,
                                                                  nullptr);
    SB_LOG(INFO) << "JNI thread: OnVideoSurfaceChanged returned.";
    jni_thread_done = true;
  });

  // Thread B: Simulate decoder teardown on the decoder thread.
  // We add a small delay to ensure the JNI thread can acquire the lock first,
  // which is necessary to trigger the deadlock on the unpatched code.
  MediaCodecVideoDecoder* raw_decoder = decoder.release();
  job_thread->job_queue()->Schedule([raw_decoder]() {
    SB_LOG(INFO) << "Decoder thread: Waiting before destroying...";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SB_LOG(INFO) << "Decoder thread: Destroying decoder...";
    delete raw_decoder;
    SB_LOG(INFO) << "Decoder thread: Decoder destroyed.";
  });

  jni_sim_thread.join();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time);
  EXPECT_LT(duration.count(), 800);  // Expect less than 800ms (should be ~100ms
                                     // with fix, 1000ms with deadlock)

  SB_LOG(INFO) << "Stopping job thread...";
  job_thread->Stop();
  job_thread.reset();
  SB_LOG(INFO) << "Job thread stopped.";

  EXPECT_TRUE(jni_thread_done);
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
