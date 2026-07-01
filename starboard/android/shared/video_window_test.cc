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

#include <unistd.h>

#include <chrono>
#include <memory>

#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/media_codec_video_decoder.h"
#include "starboard/android/shared/video_surface_texture_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/thread.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

class VideoDecoderSurfaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_ = jni_zero::AttachCurrentThread();

    real_surface_ = VideoSurfaceTextureBridge::CreateSurfaceForTesting(env_);
    ASSERT_TRUE(real_surface_);
  }

  void TearDown() override {
    // Clean up global surface state
    starboard::SetVideoSurfaceForTesting(env_, nullptr);
  }

  JNIEnv* env_ = nullptr;
  jni_zero::ScopedJavaLocalRef<jobject> real_surface_;
};

TEST_F(VideoDecoderSurfaceTest, TeardownDuringSurfaceDestroyReleasesSurface) {
  // 1. Set the global video surface so the decoder can acquire it.
  starboard::SetVideoSurfaceForTesting(env_, real_surface_.obj());

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

  job_thread->Schedule([&]() {
    auto result = MediaCodecVideoDecoder::CreateForTesting(
        std::move(factory), job_thread->job_queue(), stream_config,
        tunnel_config, pipeline_config, platform_options);
    std::lock_guard lock(init_mutex);
    if (result) {
      decoder = std::move(result.value());
      init_success = true;
    }
    init_done = true;
    init_cv.notify_all();
  });

  {
    std::unique_lock lock(init_mutex);
    init_cv.wait(lock, [&] { return init_done; });
  }
  ASSERT_TRUE(init_success);
  ASSERT_NE(decoder, nullptr);

  std::atomic<bool> jni_thread_done{false};

  // Thread A: Simulate JNI thread receiving surface destroyed event.
  struct JniSimThread : public starboard::Thread {
    explicit JniSimThread(std::atomic<bool>& done)
        : Thread("JniSimThread"), done_(done) {}
    void Run() override {
      // JNIEnv is thread-local. Background threads must obtain their own local
      // JNIEnv pointer by attaching to the JVM.
      JNIEnv* env = jni_zero::AttachCurrentThread();
      SB_LOG(INFO) << "JNI thread: Calling OnVideoSurfaceChanged(nullptr)...";
      starboard::SetVideoSurfaceForTesting(env, nullptr);
      SB_LOG(INFO) << "JNI thread: OnVideoSurfaceChanged returned.";
      done_ = true;
    }
    std::atomic<bool>& done_;
  };

  auto start_time = CurrentMonotonicTime();
  JniSimThread jni_sim_thread(jni_thread_done);
  jni_sim_thread.Start();

  // Thread B: Simulate decoder teardown on the decoder thread.
  // We add a small delay to ensure the JNI thread can acquire the lock first,
  // which is necessary to trigger the deadlock on the unpatched code.
  job_thread->Schedule([decoder = std::move(decoder)] {
    SB_LOG(INFO) << "Decoder thread: Waiting before destroying...";
    usleep(100'000);
    SB_LOG(INFO) << "Decoder thread: Destroying decoder...";
  });

  jni_sim_thread.Join();
  auto duration = CurrentMonotonicTime() - start_time;
  EXPECT_LT(duration, 800'000);  // Expect less than 800ms (should be ~100ms
                                 // with fix, 1000ms with deadlock)

  SB_LOG(INFO) << "Stopping job thread...";
  job_thread->Stop();
  job_thread.reset();
  SB_LOG(INFO) << "Job thread stopped.";

  EXPECT_TRUE(jni_thread_done);
}

}  // namespace
}  // namespace starboard
