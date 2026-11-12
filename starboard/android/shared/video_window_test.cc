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
#include <stdint.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/media_codec_video_decoder.h"
#include "starboard/android/shared/video_surface_texture_bridge.h"
#include "starboard/decode_target.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/experimental_features.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/testing/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

namespace {

class TestSurfaceHolder : public VideoSurfaceHolder {
 public:
  TestSurfaceHolder() = default;
  ~TestSurfaceHolder() { ReleaseVideoSurface(); }

  jni_zero::ScopedJavaLocalRef<jobject> Acquire() {
    return AcquireVideoSurface();
  }

  void Release() { ReleaseVideoSurface(); }

  bool surface_destroyed() const { return surface_destroyed_.load(); }

  void OnSurfaceDestroyed() override { surface_destroyed_.store(true); }

 private:
  std::atomic<bool> surface_destroyed_{false};
};

void CountingGlesContextRunner(SbDecodeTargetGraphicsContextProvider* provider,
                               SbDecodeTargetGlesContextRunnerTarget,
                               void*) {
  if (provider && provider->gles_context_runner_context) {
    static_cast<std::atomic<int>*>(provider->gles_context_runner_context)
        ->fetch_add(1);
  }
}

class CountingFakeMediaCodecFactory : public FakeMediaCodecFactory {
 public:
  CountingFakeMediaCodecFactory(const std::atomic<int>* cleanup_count,
                                std::atomic<int>* cleanup_count_at_create,
                                std::atomic<int>* create_count = nullptr)
      : cleanup_count_(cleanup_count),
        cleanup_count_at_create_(cleanup_count_at_create),
        create_count_(create_count) {}

  NonNullResult<std::unique_ptr<MediaCodec>> CreateVideoMediaCodec(
      SbMediaVideoCodec video_codec,
      const Size& frame_size_hint,
      int fps,
      const std::optional<Size>& max_frame_size,
      MediaCodec::Handler* handler,
      const jni_zero::JavaRef<jobject>& j_surface,
      const jni_zero::JavaRef<jobject>& j_media_crypto,
      const SbMediaColorMetadata* color_metadata,
      const MediaCodec::VideoPlatformOptions& platform_options) override {
    if (cleanup_count_ && cleanup_count_at_create_) {
      cleanup_count_at_create_->store(cleanup_count_->load());
    }
    if (create_count_) {
      create_count_->fetch_add(1);
    }
    return FakeMediaCodecFactory::CreateVideoMediaCodec(
        video_codec, frame_size_hint, fps, max_frame_size, handler, j_surface,
        j_media_crypto, color_metadata, platform_options);
  }

 private:
  const std::atomic<int>* const cleanup_count_;
  std::atomic<int>* const cleanup_count_at_create_;
  std::atomic<int>* const create_count_;
};

scoped_refptr<InputBuffer> CreateDummyH264InputBuffer(int64_t timestamp) {
  constexpr int kBufferSize = 64;
  uint8_t* buffer = new uint8_t[kBufferSize];
  std::memset(buffer, 0, kBufferSize);

  SbPlayerSampleInfo sample_info = {};
  sample_info.type = kSbMediaTypeVideo;
  sample_info.buffer = buffer;
  sample_info.buffer_size = kBufferSize;
  sample_info.timestamp = timestamp;
  sample_info.video_sample_info.stream_info.codec = kSbMediaVideoCodecH264;
  sample_info.video_sample_info.stream_info.mime = "";
  sample_info.video_sample_info.stream_info.max_video_capabilities = "";
  sample_info.video_sample_info.stream_info.frame_width = 640;
  sample_info.video_sample_info.stream_info.frame_height = 480;
  sample_info.video_sample_info.stream_info.color_metadata.primaries =
      kSbMediaPrimaryIdBt709;
  sample_info.video_sample_info.stream_info.color_metadata.transfer =
      kSbMediaTransferIdBt709;
  sample_info.video_sample_info.stream_info.color_metadata.matrix =
      kSbMediaMatrixIdBt709;
  sample_info.video_sample_info.stream_info.color_metadata.range =
      kSbMediaRangeIdLimited;
  sample_info.video_sample_info.is_key_frame = true;

  auto deallocate_func = [](SbPlayer, void*, const void* sample_buffer) {
    delete[] static_cast<const uint8_t*>(sample_buffer);
  };

  return make_scoped_refptr<InputBuffer>(deallocate_func, /*player=*/nullptr,
                                         /*context=*/nullptr, sample_info);
}

void ReleaseJavaObject(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj) {
  if (!obj) {
    return;
  }
  jclass cls = env->GetObjectClass(obj.obj());
  if (cls) {
    jmethodID release_method = env->GetMethodID(cls, "release", "()V");
    if (release_method) {
      env->CallVoidMethod(obj.obj(), release_method);
    }
    env->DeleteLocalRef(cls);
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
}

void NotifyVideoSurfaceChanged(JNIEnv* env, jobject surface) {
  jclass view_cls =
      jni_zero::GetClass(env, "dev/cobalt/media/VideoSurfaceView").Release();
  if (env->ExceptionCheck() || !view_cls) {
    env->ExceptionClear();
    ADD_FAILURE() << "Failed to find dev/cobalt/media/VideoSurfaceView";
    return;
  }
  jmethodID notify_method = env->GetStaticMethodID(
      view_cls, "notifyVideoSurfaceChanged", "(Landroid/view/Surface;)V");
  if (env->ExceptionCheck() || !notify_method) {
    env->ExceptionClear();
    env->DeleteLocalRef(view_cls);
    ADD_FAILURE()
        << "Failed to find VideoSurfaceView.notifyVideoSurfaceChanged";
    return;
  }
  env->CallStaticVoidMethod(view_cls, notify_method, surface);
  env->DeleteLocalRef(view_cls);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    ADD_FAILURE() << "Exception in VideoSurfaceView.notifyVideoSurfaceChanged";
  }
}

void RestoreActivitySurface(JNIEnv* env) {
  jclass view_cls =
      jni_zero::GetClass(env, "dev/cobalt/media/VideoSurfaceView").Release();
  if (env->ExceptionCheck() || !view_cls) {
    env->ExceptionClear();
    return;
  }
  jmethodID get_surface = env->GetStaticMethodID(view_cls, "getCurrentSurface",
                                                 "()Landroid/view/Surface;");
  if (env->ExceptionCheck() || !get_surface) {
    env->ExceptionClear();
    env->DeleteLocalRef(view_cls);
    return;
  }
  jobject current_surface = env->CallStaticObjectMethod(view_cls, get_surface);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    env->DeleteLocalRef(view_cls);
    return;
  }
  NotifyVideoSurfaceChanged(env, current_surface);
  if (current_surface) {
    env->DeleteLocalRef(current_surface);
  }
  env->DeleteLocalRef(view_cls);
}

void DummyDeallocateSampleFunc(SbPlayer, void*, const void*) {}
void DummyDecoderStatusFunc(SbPlayer,
                            void*,
                            SbMediaType,
                            SbPlayerDecoderState,
                            int) {}

struct PlayerWaitContext {
  std::mutex mutex;
  std::condition_variable cv;
  bool initialized = false;
  bool has_error = false;

  void Reset() {
    std::lock_guard lock(mutex);
    initialized = false;
    has_error = false;
  }

  void WaitForInitOrError(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex);
    cv.wait_for(lock, timeout, [this]() { return initialized || has_error; });
  }
};

void TestPlayerStatusFunc(SbPlayer, void* context, SbPlayerState state, int) {
  if (!context || state != kSbPlayerStateInitialized) {
    return;
  }
  auto* wait_context = static_cast<PlayerWaitContext*>(context);
  std::lock_guard lock(wait_context->mutex);
  wait_context->initialized = true;
  wait_context->cv.notify_all();
}

void TestPlayerErrorFunc(SbPlayer, void* context, SbPlayerError, const char*) {
  if (!context) {
    return;
  }
  auto* wait_context = static_cast<PlayerWaitContext*>(context);
  std::lock_guard lock(wait_context->mutex);
  wait_context->has_error = true;
  wait_context->cv.notify_all();
}

class VideoWindowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    StarboardExtensionExperimentalFeatures exp_features = {nullptr, 0};
    SetExperimentalFeaturesForCurrentThread(&exp_features);

    JNIEnv* env = jni_zero::AttachCurrentThread();
    ASSERT_NE(env, nullptr);
    surface_texture_ =
        VideoSurfaceTextureBridge::CreateVideoSurfaceTexture(env, 0);
    ASSERT_TRUE(surface_texture_);
    surface_ = VideoSurfaceTextureBridge::CreateSurface(env, surface_texture_);
    ASSERT_TRUE(surface_);
    NotifyVideoSurfaceChanged(env, surface_.obj());
  }

  void TearDown() override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    NotifyVideoSurfaceChanged(env, nullptr);
    ReleaseJavaObject(env, surface_);
    ReleaseJavaObject(env, surface_texture_);
    surface_.Reset();
    surface_texture_.Reset();
    RestoreActivitySurface(env);
  }

  jni_zero::ScopedJavaGlobalRef<jobject> surface_texture_;
  jni_zero::ScopedJavaGlobalRef<jobject> surface_;
};

TEST_F(VideoWindowTest, HasVideoSurfaceRemainsTrueWhileHeld) {
  EXPECT_TRUE(VideoSurfaceHolder::HasVideoSurface());
  EXPECT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  TestSurfaceHolder holder;
  auto acquired = holder.Acquire();
  ASSERT_FALSE(acquired.is_null());

  EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
  EXPECT_TRUE(VideoSurfaceHolder::HasVideoSurface());

  holder.Release();
  EXPECT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
  EXPECT_TRUE(VideoSurfaceHolder::HasVideoSurface());

  JNIEnv* env = jni_zero::AttachCurrentThread();
  NotifyVideoSurfaceChanged(env, nullptr);
  EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
  EXPECT_FALSE(VideoSurfaceHolder::HasVideoSurface());
}

TEST_F(VideoWindowTest, AcquireFailsImmediatelyWhenHeldAndFlagDisabled) {
  features::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kCobaltWaitForVideoSurfaceRelease);

  TestSurfaceHolder holder1;
  TestSurfaceHolder holder2;

  auto first = holder1.Acquire();
  ASSERT_FALSE(first.is_null());

  auto start = std::chrono::steady_clock::now();
  auto second = holder2.Acquire();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();

  EXPECT_TRUE(second.is_null());
  EXPECT_LT(elapsed_ms, 100);

  holder1.Release();
}

TEST_F(VideoWindowTest, AcquireWaitsForReleaseAndSucceedsWhenFlagEnabled) {
  features::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kCobaltWaitForVideoSurfaceRelease);

  TestSurfaceHolder holder1;
  TestSurfaceHolder holder2;

  auto first = holder1.Acquire();
  ASSERT_FALSE(first.is_null());

  std::promise<void> started_promise;
  std::future<void> started_future = started_promise.get_future();
  std::atomic<bool> acquired_second{false};
  std::thread waiter([&holder2, &started_promise, &acquired_second]() {
    started_promise.set_value();
    {
      auto second = holder2.Acquire();
      acquired_second.store(!second.is_null());
      if (!second.is_null()) {
        holder2.Release();
      }
    }
    jni_zero::DetachFromVM();
  });

  started_future.wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  holder1.Release();
  auto start = std::chrono::steady_clock::now();
  waiter.join();
  auto join_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start)
                     .count();

  EXPECT_TRUE(acquired_second.load());
  EXPECT_LT(join_ms, 400);
}

TEST_F(VideoWindowTest, AcquireTimesOutWhenSurfaceRemainsHeld) {
  features::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kCobaltWaitForVideoSurfaceRelease);
  ASSERT_EQ(features::kCobaltWaitForVideoSurfaceReleaseTimeoutMs.Get(), 500);

  TestSurfaceHolder holder1;
  TestSurfaceHolder holder2;

  auto first = holder1.Acquire();
  ASSERT_FALSE(first.is_null());

  auto start = std::chrono::steady_clock::now();
  auto second = holder2.Acquire();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();

  EXPECT_TRUE(second.is_null());
  EXPECT_GE(elapsed_ms, 450);
  EXPECT_LT(elapsed_ms, 1500);

  holder1.Release();
}

TEST_F(VideoWindowTest, AcquireWakesEarlyWhenSurfaceDestroyed) {
  features::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kCobaltWaitForVideoSurfaceRelease);

  TestSurfaceHolder holder1;
  TestSurfaceHolder holder2;

  auto first = holder1.Acquire();
  ASSERT_FALSE(first.is_null());

  std::promise<void> started_promise;
  std::future<void> started_future = started_promise.get_future();
  std::atomic<bool> acquired_second{true};
  std::atomic<int64_t> waiter_elapsed_ms{0};
  std::thread waiter(
      [&holder2, &started_promise, &acquired_second, &waiter_elapsed_ms]() {
        started_promise.set_value();
        {
          auto wait_start = std::chrono::steady_clock::now();
          auto second = holder2.Acquire();
          waiter_elapsed_ms.store(
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - wait_start)
                  .count());
          acquired_second.store(!second.is_null());
          if (!second.is_null()) {
            holder2.Release();
          }
        }
        jni_zero::DetachFromVM();
      });

  started_future.wait();
  auto start = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  JNIEnv* env = jni_zero::AttachCurrentThread();
  NotifyVideoSurfaceChanged(env, nullptr);
  waiter.join();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();

  EXPECT_FALSE(acquired_second.load());
  EXPECT_TRUE(holder1.surface_destroyed());
  EXPECT_GE(waiter_elapsed_ms.load(), 25);
  EXPECT_LT(elapsed_ms, 350);
}

TEST_F(VideoWindowTest,
       SbPlayerCreatePunchOutSucceedsDuringConcurrentHoldWhenFlagEnabled) {
  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = kSbDrmSystemInvalid;
  creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
  creation_param.audio_stream_info.mime = "";
  creation_param.video_stream_info.codec = kSbMediaVideoCodecH264;
  creation_param.video_stream_info.mime = "";
  creation_param.video_stream_info.max_video_capabilities = "";
  creation_param.video_stream_info.frame_width = 640;
  creation_param.video_stream_info.frame_height = 480;
  creation_param.video_stream_info.color_metadata.bits_per_channel = 8;
  creation_param.video_stream_info.color_metadata.primaries =
      kSbMediaPrimaryIdBt709;
  creation_param.video_stream_info.color_metadata.transfer =
      kSbMediaTransferIdBt709;
  creation_param.video_stream_info.color_metadata.matrix =
      kSbMediaMatrixIdBt709;
  creation_param.output_mode = kSbPlayerOutputModePunchOut;

  std::atomic<int> cleanup_count{0};
  SbDecodeTargetGraphicsContextProvider fake_provider{
      nullptr, nullptr, &CountingGlesContextRunner, &cleanup_count};

  TestSurfaceHolder holder;
  auto held_surface = holder.Acquire();
  ASSERT_FALSE(held_surface.is_null());

  PlayerWaitContext wait_context;

  // Subcase (a): flag disabled -> fails immediately when surface is held.
  {
    features::ScopedFeatureList scoped_features;
    scoped_features.InitAndDisableFeature(
        features::kCobaltWaitForVideoSurfaceRelease);
    SbPlayer player = SbPlayerCreate(
        kSbWindowInvalid, &creation_param, DummyDeallocateSampleFunc,
        DummyDecoderStatusFunc, TestPlayerStatusFunc, TestPlayerErrorFunc,
        &wait_context, &fake_provider);
    EXPECT_FALSE(SbPlayerIsValid(player));
  }

  holder.Release();
  held_surface = holder.Acquire();
  ASSERT_FALSE(held_surface.is_null());

  // Subcase (b): flag enabled + outgoing holder releases after 50ms ->
  // SbPlayerCreate returns immediately without blocking main thread, and
  // PlayerWorker asynchronously initializes the decoder once surface is freed.
  {
    features::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeature(
        features::kCobaltWaitForVideoSurfaceRelease);
    wait_context.Reset();
    cleanup_count.store(0);

    auto start = std::chrono::steady_clock::now();
    SbPlayer player = SbPlayerCreate(
        kSbWindowInvalid, &creation_param, DummyDeallocateSampleFunc,
        DummyDecoderStatusFunc, TestPlayerStatusFunc, TestPlayerErrorFunc,
        &wait_context, &fake_provider);
    auto create_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();

    EXPECT_TRUE(SbPlayerIsValid(player));
    EXPECT_LT(create_ms, 300);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    holder.Release();

    if (SbPlayerIsValid(player)) {
      wait_context.WaitForInitOrError(std::chrono::milliseconds(5000));
      EXPECT_TRUE(wait_context.initialized);
      EXPECT_FALSE(wait_context.has_error);
      SbPlayerDestroy(player);
      EXPECT_EQ(cleanup_count.load(), 1);
    }
  }

  // Subcase (c): flag enabled + surface remains held past timeout ->
  // SbPlayerCreate still stays non-blocking, while PlayerWorker times out,
  // reports a player error after ~500ms, and does not clean up or release the
  // active holder's surface on destroy.
  held_surface = holder.Acquire();
  ASSERT_FALSE(held_surface.is_null());
  {
    features::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeature(
        features::kCobaltWaitForVideoSurfaceRelease);
    wait_context.Reset();
    cleanup_count.store(0);

    auto start = std::chrono::steady_clock::now();
    SbPlayer player = SbPlayerCreate(
        kSbWindowInvalid, &creation_param, DummyDeallocateSampleFunc,
        DummyDecoderStatusFunc, TestPlayerStatusFunc, TestPlayerErrorFunc,
        &wait_context, &fake_provider);
    auto create_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();

    EXPECT_TRUE(SbPlayerIsValid(player));
    EXPECT_LT(create_ms, 300);

    if (SbPlayerIsValid(player)) {
      wait_context.WaitForInitOrError(std::chrono::milliseconds(5000));
      auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
      EXPECT_TRUE(wait_context.has_error);
      EXPECT_FALSE(wait_context.initialized);
      EXPECT_GE(wait_ms, 450);
      SbPlayerDestroy(player);
      EXPECT_EQ(cleanup_count.load(), 0);
      EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
    }
  }

  holder.Release();
}

TEST_F(VideoWindowTest,
       MediaCodecVideoDecoderRetainsSurfaceAcrossResetAndReleasesAfterCleanup) {
  features::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kCobaltWaitForVideoSurfaceRelease);

  std::atomic<int> cleanup_count{0};
  std::atomic<int> cleanup_count_at_decoder2_create{-1};
  std::atomic<int> decoder2_codec_create_count{0};
  SbDecodeTargetGraphicsContextProvider fake_provider{
      nullptr, nullptr, &CountingGlesContextRunner, &cleanup_count};

  MediaCodecVideoDecoder::StreamConfig stream_config;
  stream_config.video_stream_info.codec = kSbMediaVideoCodecH264;
  stream_config.video_stream_info.frame_size = {640, 480};
  stream_config.video_stream_info.color_metadata.primaries =
      kSbMediaPrimaryIdBt709;
  stream_config.video_stream_info.color_metadata.transfer =
      kSbMediaTransferIdBt709;
  stream_config.video_stream_info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  stream_config.video_stream_info.color_metadata.range = kSbMediaRangeIdLimited;
  stream_config.drm_system = kSbDrmSystemInvalid;
  stream_config.output_mode = kSbPlayerOutputModePunchOut;
  stream_config.decode_target_graphics_context_provider = &fake_provider;
  stream_config.surface_view = nullptr;

  MediaCodecVideoDecoder::TunnelModeConfig tunnel_config;
  MediaCodecVideoDecoder::PipelineConfig pipeline_config;
  pipeline_config.enable_flush_during_seek = false;
  MediaCodecVideoDecoder::PlatformOptions platform_options;

  std::unique_ptr<JobThread> thread1 = JobThread::Create("VideoDec1");
  std::unique_ptr<JobThread> thread2 = JobThread::Create("VideoDec2");

  std::unique_ptr<MediaCodecVideoDecoder> decoder1;
  thread1->ScheduleAndWait([&]() {
    auto result = MediaCodecVideoDecoder::CreateForTesting(
        std::make_unique<CountingFakeMediaCodecFactory>(nullptr, nullptr),
        thread1->job_queue(), stream_config, tunnel_config, pipeline_config,
        platform_options);
    ASSERT_TRUE(result);
    decoder1 = std::move(result.value());
  });
  ASSERT_NE(decoder1, nullptr);

  EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
  EXPECT_TRUE(VideoSurfaceHolder::HasVideoSurface());

  // ResetForTeardown runs TeardownCodec() when enable_flush_during_seek is
  // false; the decoder must still hold and not clean up the surface until
  // ~MediaCodecVideoDecoder().
  thread1->ScheduleAndWait([&]() { decoder1->ResetForTeardown(); });
  EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
  EXPECT_EQ(cleanup_count.load(), 0);

  std::promise<void> decoder2_started_promise;
  std::future<void> decoder2_started_future =
      decoder2_started_promise.get_future();
  std::promise<bool> decoder2_created_promise;
  std::future<bool> decoder2_created_future =
      decoder2_created_promise.get_future();
  std::unique_ptr<MediaCodecVideoDecoder> decoder2;

  thread2->Schedule([&]() {
    decoder2_started_promise.set_value();
    auto result = MediaCodecVideoDecoder::CreateForTesting(
        std::make_unique<CountingFakeMediaCodecFactory>(
            &cleanup_count, &cleanup_count_at_decoder2_create,
            &decoder2_codec_create_count),
        thread2->job_queue(), stream_config, tunnel_config, pipeline_config,
        platform_options);
    if (result) {
      decoder2 = std::move(result.value());
      decoder2_created_promise.set_value(true);
    } else {
      decoder2_created_promise.set_value(false);
    }
  });

  decoder2_started_future.wait();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  thread1->ScheduleAndWait([&]() { decoder1.reset(); });

  ASSERT_TRUE(decoder2_created_future.get());
  EXPECT_EQ(cleanup_count_at_decoder2_create.load(), 1);
  EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  // Rebuilding the codec on the same decoder after Reset() must reuse the held
  // surface via the re-entrant AcquireVideoSurface() path without failing or
  // self-deadlocking.
  std::atomic<bool> decoder2_error{false};
  thread2->ScheduleAndWait([&]() {
    auto sink = decoder2->GetSink();
    ASSERT_NE(sink, nullptr);
    sink->SetRenderCB([](const VideoRendererSink::DrawFrameCB&) {});
    decoder2->Initialize(
        [](VideoDecoder::Status, const scoped_refptr<VideoFrame>&) {},
        [&decoder2_error](SbPlayerError, const std::string&) {
          decoder2_error.store(true);
        });
    decoder2->Reset();
    decoder2->WriteInputBuffers({CreateDummyH264InputBuffer(1000)});
  });
  EXPECT_FALSE(decoder2_error.load());
  EXPECT_EQ(decoder2_codec_create_count.load(), 2);

  thread2->ScheduleAndWait([&]() { decoder2.reset(); });
  EXPECT_EQ(cleanup_count.load(), 2);
  EXPECT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  thread1->Stop();
  thread2->Stop();
}

}  // namespace
}  // namespace starboard
