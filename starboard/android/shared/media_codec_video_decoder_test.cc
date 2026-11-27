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

#include "starboard/android/shared/media_codec_video_decoder.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/experimental_features.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {
namespace {

using ::jni_zero::AttachCurrentThread;

const VideoStreamInfo kDefaultVideoStreamInfo = [] {
  VideoStreamInfo info;
  info.codec = kSbMediaVideoCodecH264;
  info.frame_size = {1920, 1080};
  info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  info.color_metadata.transfer = kSbMediaTransferIdBt709;
  info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  info.color_metadata.range = kSbMediaRangeIdLimited;
  return info;
}();

const VideoStreamInfo kHdrVideoStreamInfo = [] {
  VideoStreamInfo info = kDefaultVideoStreamInfo;
  info.color_metadata.primaries = kSbMediaPrimaryIdBt2020;
  info.color_metadata.transfer = kSbMediaTransferIdSmpteSt2084;
  info.color_metadata.matrix = kSbMediaMatrixIdBt2020NonconstantLuminance;
  info.color_metadata.mastering_metadata.luminance_max = 1000;
  return info;
}();

const VideoStreamInfo kAv1VideoStreamInfo = [] {
  VideoStreamInfo info = kDefaultVideoStreamInfo;
  info.codec = kSbMediaVideoCodecAv1;
  return info;
}();

ExperimentalFeatures MakeVideoDecoderImprovementsFeatures(int64_t flags) {
  return ExperimentalFeatures(ExperimentalFeatures::Map{
      {std::string(kMediaAndroidVideoDecoderImprovements.key()), flags}});
}

// Forces `MediaCapabilitiesCache::IsAv18kCappedAt30()` to return true so AV1
// decoders deterministically delay `InitializeCodec()` until enough inputs
// arrive to estimate frame rate.
//
// Lifetime & Ownership: Stack-allocated per test and restores the prior cache
// state on destruction.
//
// Threading: Used on the main test thread.
class ScopedForceAv1FpsCheck {
 public:
  ScopedForceAv1FpsCheck()
      : was_enabled_(MediaCapabilitiesCache::GetInstance()->IsEnabled()) {
    MediaCapabilitiesCache::GetInstance()->SetCacheEnabled(false);
  }

  ScopedForceAv1FpsCheck(const ScopedForceAv1FpsCheck&) = delete;
  ScopedForceAv1FpsCheck& operator=(const ScopedForceAv1FpsCheck&) = delete;

  ~ScopedForceAv1FpsCheck() {
    MediaCapabilitiesCache::GetInstance()->SetCacheEnabled(was_enabled_);
  }

 private:
  const bool was_enabled_;
};

// Ensures a global punch-out video surface is registered in `video_window.cc`
// during a test (even on headless test runs) and restores the prior surface on
// destruction.
//
// Lifetime & Ownership: Stack-allocated per test and owns any temporary
// `SurfaceTexture` and `Surface` objects it creates.
//
// Threading: Used on the main test thread.
class ScopedGlobalVideoSurface {
 public:
  ScopedGlobalVideoSurface() {
    JNIEnv* env = AttachCurrentThread();
    video_surface_view_class_.Reset(
        jni_zero::GetClass(env, "dev/cobalt/media/VideoSurfaceView"));
    notify_method_ = env->GetStaticMethodID(video_surface_view_class_.obj(),
                                            "notifyVideoSurfaceChanged",
                                            "(Landroid/view/Surface;)V");
    jmethodID get_current_method =
        env->GetStaticMethodID(video_surface_view_class_.obj(),
                               "getCurrentSurface", "()Landroid/view/Surface;");
    jni_zero::ScopedJavaLocalRef<jobject> current_surface(
        env, env->CallStaticObjectMethod(video_surface_view_class_.obj(),
                                         get_current_method));
    original_surface_.Reset(current_surface);
    if (VideoSurfaceHolder::IsVideoSurfaceAvailable()) {
      return;
    }

    CreateTemporarySurface(env, &fallback_surface_texture_, &fallback_surface_);
    NotifySurfaceChanged(env, fallback_surface_.obj());
  }

  ScopedGlobalVideoSurface(const ScopedGlobalVideoSurface&) = delete;
  ScopedGlobalVideoSurface& operator=(const ScopedGlobalVideoSurface&) = delete;

  ~ScopedGlobalVideoSurface() {
    JNIEnv* env = AttachCurrentThread();
    NotifySurfaceChanged(env, original_surface_.obj());
    ReleaseTemporarySurface(env, &fallback_surface_,
                            &fallback_surface_texture_);
    ReleaseTemporarySurface(env, &replacement_surface_,
                            &replacement_surface_texture_);
  }

  void ReplaceSurface() {
    JNIEnv* env = AttachCurrentThread();
    ReleaseTemporarySurface(env, &replacement_surface_,
                            &replacement_surface_texture_);
    CreateTemporarySurface(env, &replacement_surface_texture_,
                           &replacement_surface_);
    NotifySurfaceChanged(env, replacement_surface_.obj());
  }

 private:
  static void CreateTemporarySurface(
      JNIEnv* env,
      jni_zero::ScopedJavaGlobalRef<jobject>* out_surface_texture,
      jni_zero::ScopedJavaGlobalRef<jobject>* out_surface) {
    jni_zero::ScopedJavaLocalRef<jclass> surface_texture_class(
        env, env->FindClass("android/graphics/SurfaceTexture"));
    jmethodID surface_texture_ctor =
        env->GetMethodID(surface_texture_class.obj(), "<init>", "(I)V");
    jni_zero::ScopedJavaLocalRef<jobject> surface_texture(
        env, env->NewObject(surface_texture_class.obj(), surface_texture_ctor,
                            /*texName=*/0));

    jni_zero::ScopedJavaLocalRef<jclass> surface_class(
        env, env->FindClass("android/view/Surface"));
    jmethodID surface_ctor = env->GetMethodID(
        surface_class.obj(), "<init>", "(Landroid/graphics/SurfaceTexture;)V");
    jni_zero::ScopedJavaLocalRef<jobject> surface(
        env, env->NewObject(surface_class.obj(), surface_ctor,
                            surface_texture.obj()));

    out_surface_texture->Reset(surface_texture);
    out_surface->Reset(surface);
  }

  static void ReleaseTemporarySurface(
      JNIEnv* env,
      jni_zero::ScopedJavaGlobalRef<jobject>* surface,
      jni_zero::ScopedJavaGlobalRef<jobject>* surface_texture) {
    if (*surface) {
      jni_zero::ScopedJavaLocalRef<jclass> surface_class(
          env, env->FindClass("android/view/Surface"));
      jmethodID release_method =
          env->GetMethodID(surface_class.obj(), "release", "()V");
      env->CallVoidMethod(surface->obj(), release_method);
      surface->Reset();
    }
    if (*surface_texture) {
      jni_zero::ScopedJavaLocalRef<jclass> surface_texture_class(
          env, env->FindClass("android/graphics/SurfaceTexture"));
      jmethodID release_method =
          env->GetMethodID(surface_texture_class.obj(), "release", "()V");
      env->CallVoidMethod(surface_texture->obj(), release_method);
      surface_texture->Reset();
    }
  }

  void NotifySurfaceChanged(JNIEnv* env, jobject surface) {
    env->CallStaticVoidMethod(video_surface_view_class_.obj(), notify_method_,
                              surface);
  }

  jni_zero::ScopedJavaGlobalRef<jclass> video_surface_view_class_;
  jmethodID notify_method_ = nullptr;
  jni_zero::ScopedJavaGlobalRef<jobject> original_surface_;
  jni_zero::ScopedJavaGlobalRef<jobject> fallback_surface_texture_;
  jni_zero::ScopedJavaGlobalRef<jobject> fallback_surface_;
  jni_zero::ScopedJavaGlobalRef<jobject> replacement_surface_texture_;
  jni_zero::ScopedJavaGlobalRef<jobject> replacement_surface_;
};

class MediaCodecVideoDecoderTest : public ::testing::Test {
 protected:
  // Creates the decoder without asserting success, so tests can exercise
  // creation failures. |codec_creation_failures| is the number of initial
  // FakeMediaCodecFactory::CreateVideoMediaCodec() calls that fail.
  NonNullResult<std::unique_ptr<MediaCodecVideoDecoder>> TryCreateDecoder(
      const std::string& max_video_capabilities = "",
      const VideoStreamInfo* initial_stream_info = nullptr,
      ExperimentalFeatures experimental_features = {},
      bool enable_flush_during_seek = false,
      int codec_creation_failures = 0) {
    auto factory = std::make_unique<FakeMediaCodecFactory>();
    fake_factory_ = factory.get();  // Save raw pointer before moving!
    fake_factory_->set_video_codec_creation_failures_remaining(
        codec_creation_failures);

    MediaCodecVideoDecoder::StreamConfig stream_config{
        initial_stream_info ? *initial_stream_info : kDefaultVideoStreamInfo,
        /*drm_system=*/kSbDrmSystemInvalid, kSbPlayerOutputModePunchOut,
        /*decode_target_graphics_context_provider=*/
        &fake_graphics_context_provider_,
        /*surface_view=*/dummy_surface_.obj(), max_video_capabilities};

    MediaCodecVideoDecoder::TunnelModeConfig tunnel_config;
    MediaCodecVideoDecoder::PipelineConfig pipeline_config;
    pipeline_config.experimental_features = std::move(experimental_features);
    pipeline_config.enable_flush_during_seek = enable_flush_during_seek;
    MediaCodecVideoDecoder::PlatformOptions platform_options;

    return MediaCodecVideoDecoder::CreateForTesting(
        std::move(factory),  // Transfer ownership
        &job_queue_, stream_config, tunnel_config, pipeline_config,
        platform_options);
  }

  void CreateDecoder(const std::string& max_video_capabilities = "",
                     const VideoStreamInfo* initial_stream_info = nullptr,
                     ExperimentalFeatures experimental_features = {},
                     bool enable_flush_during_seek = false,
                     int codec_creation_failures = 0) {
    auto result =
        TryCreateDecoder(max_video_capabilities, initial_stream_info,
                         std::move(experimental_features),
                         enable_flush_during_seek, codec_creation_failures);

    ASSERT_TRUE(result);
    decoder_ = std::move(result.value());

    // Setup sink to avoid crashes in Tick() on decoder thread
    auto sink = decoder_->GetSink();
    ASSERT_NE(sink, nullptr);
    sink->SetRenderCB(
        [this](const VideoRendererSink::DrawFrameCB& draw_frame_cb) {
          std::lock_guard lock(callback_mutex_);
          render_count_++;
          if (on_render_cb_) {
            on_render_cb_();
          }
        });
  }

  // Experimental features with the H5VCC codec creation retry |setting|, which
  // is the retry count when it fits in the low byte (default base delay).
  static ExperimentalFeatures CodecCreationRetryFeatures(int setting) {
    return ExperimentalFeatures(ExperimentalFeatures::Map{
        {std::string(kMediaVideoCodecCreationRetry.key()),
         static_cast<int64_t>(setting)}});
  }

  // Creates a decoder with a retry budget of |codec_creation_retry_count| (at
  // most 255, so that it is not read as a packed setting) and the default base
  // delay, whose first |codec_creation_failures| MediaCodec creations fail.
  void CreateDecoderWithCodecCreationRetries(int codec_creation_retry_count,
                                             int codec_creation_failures) {
    CreateDecoder(/*max_video_capabilities=*/"",
                  /*initial_stream_info=*/nullptr,
                  CodecCreationRetryFeatures(codec_creation_retry_count),
                  /*enable_flush_during_seek=*/false, codec_creation_failures);
  }

  // The test thread owns |job_queue_|, so delayed jobs (e.g. codec creation
  // retries) only run when the queue is pumped. Pumps the queue until
  // |predicate| holds or |timeout_ms| elapses; returns whether it holds.
  bool PumpJobQueueUntil(std::function<bool()> predicate, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (!predicate()) {
      if (std::chrono::steady_clock::now() >= deadline) {
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      job_queue_.RunUntilIdle();
    }
    return true;
  }

  // Pumps the job queue for |duration_ms| regardless of state.
  void PumpJobQueueFor(int duration_ms) {
    PumpJobQueueUntil([] { return false; }, duration_ms);
  }

  scoped_refptr<InputBuffer> CreateDummyVideoInputBuffer(
      int64_t timestamp,
      int size,
      const VideoStreamInfo* stream_info = nullptr,
      bool is_key_frame = false) {
    uint8_t* buffer = new uint8_t[size];
    memset(buffer, 0, size);

    SbPlayerSampleInfo sample_info = {};
    sample_info.type = kSbMediaTypeVideo;
    sample_info.buffer = buffer;
    sample_info.buffer_size = size;
    sample_info.timestamp = timestamp;
    const VideoStreamInfo& info =
        stream_info ? *stream_info : kDefaultVideoStreamInfo;
    info.ConvertTo(&sample_info.video_sample_info.stream_info);
    sample_info.video_sample_info.is_key_frame = is_key_frame;

    auto deallocate_func = [](SbPlayer player, void* context,
                              const void* sample_buffer) {
      delete[] static_cast<const uint8_t*>(sample_buffer);
    };

    return make_scoped_refptr<InputBuffer>(deallocate_func, /*player=*/nullptr,
                                           /*context=*/nullptr, sample_info);
  }

  FakeMediaCodec* GetFakeVideoCodec() {
    return fake_factory_ ? fake_factory_->last_created_video_codec() : nullptr;
  }

  void InitializeDecoder() {
    decoder_->Initialize([](VideoDecoder::Status status,
                            const scoped_refptr<VideoFrame>& frame) {},
                         [this](SbPlayerError error, const std::string& msg) {
                           error_called_ = true;
                           error_count_++;
                           last_error_ = error;
                         });
  }

  std::atomic<bool> error_called_{false};
  std::atomic<int> error_count_{0};
  std::atomic<SbPlayerError> last_error_{kSbPlayerErrorMax};

  std::mutex callback_mutex_;
  int need_more_input_count_ = 0;
  int render_count_ = 0;
  std::function<void()> on_render_cb_;
  JobQueue job_queue_;
  FakeMediaCodecFactory* fake_factory_ = nullptr;
  int gles_context_runner_count_ = 0;
  // Punch-out decoders clean up the global video window on destruction, which
  // requires a provider when the test activity's video surface exists.
  SbDecodeTargetGraphicsContextProvider fake_graphics_context_provider_{
      nullptr, nullptr,
      [](SbDecodeTargetGraphicsContextProvider* provider,
         SbDecodeTargetGlesContextRunnerTarget,
         void*) {
        ++*static_cast<int*>(provider->gles_context_runner_context);
      },
      &gles_context_runner_count_};
  const jni_zero::ScopedJavaGlobalRef<jstring> dummy_surface_{
      base::android::ConvertUTF8ToJavaString(AttachCurrentThread(),
                                             "dummy_surface")};

  std::unique_ptr<MediaCodecVideoDecoder> decoder_;
};

TEST_F(MediaCodecVideoDecoderTest, InitializesCorrectly) {
  CreateDecoder();

  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);
}

TEST_F(MediaCodecVideoDecoderTest, BasicDecodingFlow) {
  CreateDecoder();

  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  bool error_called = false;
  std::mutex output_mutex;
  std::condition_variable output_cv;
  bool frame_received = false;
  scoped_refptr<VideoFrame> received_frame;

  // Initialize once with a callback that captures the frame
  decoder_->Initialize(
      [&](VideoDecoder::Status status, const scoped_refptr<VideoFrame>& frame) {
        if (frame && !frame->is_end_of_stream()) {
          std::lock_guard lock(output_mutex);
          frame_received = true;
          received_frame = frame;
          output_cv.notify_all();
        }
      },
      [&error_called](SbPlayerError error, const std::string& msg) {
        error_called = true;
      });

  // 1. Simulate hardware making input buffer available
  fake_codec->SimulateInputBufferAvailable(0);

  // 2. Write input data
  auto input_buffer = CreateDummyVideoInputBuffer(10000, 1024);
  decoder_->WriteInputBuffers({input_buffer});

  // 3. Wait for FakeMediaCodec to receive the queued input
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 1U);
  EXPECT_EQ(queued_inputs[0].index, 0);
  EXPECT_EQ(queued_inputs[0].pts, 10000);
  EXPECT_EQ(queued_inputs[0].size, 1024);

  // 4. Simulate output buffer ready
  fake_codec->SimulateOutputAvailable(0, 0, 0, 10000, 1024);

  // Wait for frame callback
  std::unique_lock<std::mutex> lock(output_mutex);
  ASSERT_TRUE(
      output_cv.wait_for(lock, std::chrono::seconds(1),
                         [&frame_received]() { return frame_received; }));

  ASSERT_NE(received_frame, nullptr);
  EXPECT_EQ(received_frame->timestamp(), 10000);

  // Releasing the VideoFrame should release the output buffer back to the fake
  // codec
  received_frame = nullptr;

  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  auto released_outputs = fake_codec->ConsumeReleasedOutputs();
  ASSERT_EQ(released_outputs.size(), 1U);
  EXPECT_EQ(released_outputs[0].index, 0);
  EXPECT_FALSE(released_outputs[0].render);

  EXPECT_FALSE(error_called);
}

TEST_F(MediaCodecVideoDecoderTest,
       InitializeCodecWithOversizedStreamSizeUpdatesMaxVideoSize) {
  // Create stream info with 1920x1080 resolution exceeding
  // max_video_capabilities (1280x720).
  VideoStreamInfo oversized_info = kDefaultVideoStreamInfo;
  oversized_info.frame_size = {1920, 1080};

  // Initializing codec with oversized initial stream resolution should
  // dynamically update max_video_size_.
  CreateDecoder("width=1280; height=720", &oversized_info);

  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    KeyFrameExceedingMaxVideoSizeRaisesCapabilityChangedErrorInWriteInputBuffers) {
  VideoStreamInfo initial_info = kDefaultVideoStreamInfo;
  initial_info.frame_size = {1280, 720};

  // Create decoder configured with max_video_capabilities="width=1280;
  // height=720".
  CreateDecoder("width=1280; height=720", &initial_info);

  SbPlayerError reported_error = kSbPlayerErrorDecode;
  bool error_called = false;

  decoder_->Initialize([](VideoDecoder::Status status,
                          const scoped_refptr<VideoFrame>& frame) {},
                       [&](SbPlayerError error, const std::string& msg) {
                         error_called = true;
                         reported_error = error;
                       });

  // Create a key frame stream info with 1920x1080 resolution (exceeding
  // 1280x720).
  VideoStreamInfo oversized_info = kDefaultVideoStreamInfo;
  oversized_info.frame_size = {1920, 1080};

  auto input_buffer = CreateDummyVideoInputBuffer(10000, 1024, &oversized_info,
                                                  /*is_key_frame=*/true);
  decoder_->WriteInputBuffers({input_buffer});

  EXPECT_TRUE(error_called);
  EXPECT_EQ(reported_error, kSbPlayerErrorCapabilityChanged);
}

TEST_F(MediaCodecVideoDecoderTest, BackpressureOnOutputFrame) {
  CreateDecoder();

  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  std::mutex local_mutex;
  std::condition_variable local_cv;
  bool output_received = false;
  VideoDecoder::Status received_status = VideoDecoder::kNeedMoreInput;

  decoder_->Initialize(
      [&](VideoDecoder::Status status, const scoped_refptr<VideoFrame>& frame) {
        std::lock_guard lock(callback_mutex_);
        if (status == VideoDecoder::kNeedMoreInput) {
          need_more_input_count_++;
        }
        if (frame) {
          received_status = status;
          std::lock_guard local_lock(local_mutex);
          output_received = true;
          local_cv.notify_all();
        }
      },
      [](SbPlayerError error, const std::string& msg) {});

  constexpr int kMaxPendingInputs = 512;

  // Write `kMaxPendingInputs + 1` buffers to fill the queue and go beyond.
  // We expect signals for the first `kMaxPendingInputs - 1` writes.
  for (int i = 0; i < kMaxPendingInputs + 1; ++i) {
    auto input_buffer = CreateDummyVideoInputBuffer(i * 1000, 1024);
    decoder_->WriteInputBuffers({input_buffer});
  }

  {
    std::lock_guard lock(callback_mutex_);
    EXPECT_EQ(need_more_input_count_, kMaxPendingInputs - 1);
  }

  // Simulate one input buffer available.
  // Decoder should process it, reducing the pending queue size to the threshold
  // (`kMaxPendingInputs`).
  fake_codec->SimulateInputBufferAvailable(0);
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  // Simulate output buffer ready.
  // Since the pending queue size is still at the threshold (>=
  // `kMaxPendingInputs`), the decoder should NOT signal `kNeedMoreInput` but
  // instead signal `kBufferFull`.
  fake_codec->SimulateOutputAvailable(/*index=*/0, /*flags=*/0, /*offset=*/0,
                                      /*pts=*/0, /*size=*/1024);

  // Wait for the output callback.
  std::unique_lock<std::mutex> local_lock(local_mutex);
  ASSERT_TRUE(
      local_cv.wait_for(local_lock, std::chrono::seconds(1),
                        [&output_received]() { return output_received; }));

  EXPECT_EQ(received_status, VideoDecoder::kBufferFull);
  {
    std::lock_guard lock(callback_mutex_);
    EXPECT_EQ(need_more_input_count_, kMaxPendingInputs - 1);
  }
}

TEST_F(MediaCodecVideoDecoderTest, FlushSeekSdrToHdrRebuildsCodec) {
  CreateDecoder(/*max_video_capabilities=*/"", /*initial_stream_info=*/nullptr,
                /*experimental_features=*/{},
                /*enable_flush_during_seek=*/true);
  InitializeDecoder();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  const int created = fake_factory_->video_codec_create_count();

  decoder_->Reset();
  // Reset() should flush the codec, not recreate it.
  ASSERT_EQ(fake_factory_->video_codec_create_count(), created);

  decoder_->WriteInputBuffers(
      {CreateDummyVideoInputBuffer(100'000'000, 1024, &kHdrVideoStreamInfo,
                                   /*is_key_frame=*/true)});

  EXPECT_EQ(fake_factory_->video_codec_create_count(), created + 1);
  auto color_metadata = fake_factory_->last_video_color_metadata();
  ASSERT_TRUE(color_metadata.has_value());
  EXPECT_EQ(color_metadata->transfer, kSbMediaTransferIdSmpteSt2084);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest, FlushSeekHdrToSdrRebuildsCodec) {
  CreateDecoder(/*max_video_capabilities=*/"", /*initial_stream_info=*/nullptr,
                /*experimental_features=*/{},
                /*enable_flush_during_seek=*/true);
  InitializeDecoder();
  decoder_->WriteInputBuffers(
      {CreateDummyVideoInputBuffer(0, 1024, &kHdrVideoStreamInfo,
                                   /*is_key_frame=*/true)});
  const int created = fake_factory_->video_codec_create_count();

  decoder_->Reset();
  // Reset() should flush the codec, not recreate it.
  ASSERT_EQ(fake_factory_->video_codec_create_count(), created);

  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(
      100'000'000, 1024, /*stream_info=*/nullptr, /*is_key_frame=*/true)});

  EXPECT_EQ(fake_factory_->video_codec_create_count(), created + 1);
  EXPECT_FALSE(fake_factory_->last_video_color_metadata().has_value());
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest, FlushSeekSameColorKeepsCodec) {
  CreateDecoder(/*max_video_capabilities=*/"", /*initial_stream_info=*/nullptr,
                /*experimental_features=*/{},
                /*enable_flush_during_seek=*/true);
  InitializeDecoder();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  const int created = fake_factory_->video_codec_create_count();

  decoder_->Reset();

  VideoStreamInfo larger_sdr_info = kDefaultVideoStreamInfo;
  larger_sdr_info.frame_size = {3840, 2160};
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(
      100'000'000, 1024, &larger_sdr_info, /*is_key_frame=*/true)});

  EXPECT_EQ(fake_factory_->video_codec_create_count(), created);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest, EndOfStreamWithoutInput) {
  CreateDecoder();

  int eos_frame_count = 0;
  VideoDecoder::Status received_status = VideoDecoder::kNeedMoreInput;
  bool eos_delivered_before_render = false;

  on_render_cb_ = [&]() {
    eos_delivered_before_render = (eos_frame_count == 1);
  };

  decoder_->Initialize(
      [&](VideoDecoder::Status status, const scoped_refptr<VideoFrame>& frame) {
        if (frame && frame->is_end_of_stream()) {
          eos_frame_count++;
          received_status = status;
        }
      },
      [](SbPlayerError error, const std::string& msg) {});

  decoder_->WriteEndOfStream();

  EXPECT_EQ(eos_frame_count, 1);
  EXPECT_EQ(received_status, VideoDecoder::kBufferFull);
  {
    std::lock_guard lock(callback_mutex_);
    EXPECT_EQ(render_count_, 1);
  }
  EXPECT_TRUE(eos_delivered_before_render);

  // A duplicate WriteEndOfStream() call should be ignored.
  decoder_->WriteEndOfStream();
  EXPECT_EQ(eos_frame_count, 1);
  {
    std::lock_guard lock(callback_mutex_);
    EXPECT_EQ(render_count_, 1);
  }
}

// Codec creation retry tests. Retries are scheduled on the job queue with a
// 10ms base delay and linear backoff, so the tests pump the queue with a
// generous timeout.
constexpr int kRetryWaitTimeoutMs = 2000;

TEST_F(MediaCodecVideoDecoderTest,
       CodecCreationFailureWithRetriesDisabledFailsCreate) {
  // Unset (0), negative (even -253, i.e. 0x...FF03, whose low byte alone would
  // mean 3 retries) and zero-count settings all disable retries.
  for (int setting : {0, -1, -253, 0x1E00}) {
    SCOPED_TRACE(::testing::Message() << "setting=" << setting);
    auto result = TryCreateDecoder(/*max_video_capabilities=*/"",
                                   /*initial_stream_info=*/nullptr,
                                   CodecCreationRetryFeatures(setting),
                                   /*enable_flush_during_seek=*/false,
                                   /*codec_creation_failures=*/1);

    ASSERT_FALSE(result);
    // The failed decoder (and the fake factory it owns) is destroyed, so only
    // the error message can be inspected.
    EXPECT_NE(result.error().find("Simulated MediaCodec creation failure"),
              std::string::npos);
  }
  fake_factory_ = nullptr;
}

TEST_F(MediaCodecVideoDecoderTest,
       CtorCodecCreationFailureIsRetriedAndInputsAreBuffered) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/1);
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 1);
  ASSERT_EQ(GetFakeVideoCodec(), nullptr);

  int need_more_input_count = 0;
  decoder_->Initialize(
      [&](VideoDecoder::Status status, const scoped_refptr<VideoFrame>& frame) {
        if (status == VideoDecoder::kNeedMoreInput) {
          need_more_input_count++;
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  // Inputs written while the retry is pending are buffered, and the decoder
  // keeps asking for more input.
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(33'000, 1024)});
  EXPECT_EQ(need_more_input_count, 2);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 2; },
      kRetryWaitTimeoutMs));
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  // The buffered inputs are handed to the newly created codec.
  fake_codec->SimulateInputBufferAvailable(0);
  fake_codec->SimulateInputBufferAvailable(1);
  ASSERT_TRUE(fake_codec->WaitForInputQueue(2, 1000));
  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 2U);
  EXPECT_EQ(queued_inputs[0].pts, 0);
  EXPECT_EQ(queued_inputs[1].pts, 33'000);

  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       CodecCreationRetriesExhaustedReportsDecodeError) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/2,
                                        /*codec_creation_failures=*/10);
  InitializeDecoder();

  ASSERT_TRUE(PumpJobQueueUntil([this] { return error_called_.load(); },
                                kRetryWaitTimeoutMs));
  EXPECT_EQ(last_error_, kSbPlayerErrorDecode);
  // Initial attempt + 2 retries.
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 3);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);

  // No further retries are scheduled, and the error is reported only once.
  PumpJobQueueFor(200);
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 3);
  EXPECT_EQ(error_count_, 1);
}

TEST_F(MediaCodecVideoDecoderTest,
       PackedCodecCreationRetrySettingSetsCountAndBaseDelay) {
  // 2 retries with a 30ms base delay, i.e. after 30 and 60ms. Bits 16-31 are
  // ignored.
  for (int setting : {0x1E02, 0x11E02}) {
    SCOPED_TRACE(::testing::Message() << "setting=" << setting);
    decoder_.reset();
    error_called_ = false;
    const auto start = std::chrono::steady_clock::now();
    CreateDecoder(/*max_video_capabilities=*/"",
                  /*initial_stream_info=*/nullptr,
                  CodecCreationRetryFeatures(setting),
                  /*enable_flush_during_seek=*/false,
                  /*codec_creation_failures=*/10);
    InitializeDecoder();

    ASSERT_TRUE(PumpJobQueueUntil([this] { return error_called_.load(); },
                                  kRetryWaitTimeoutMs));
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(last_error_, kSbPlayerErrorDecode);
    // Initial attempt + 2 retries.
    EXPECT_EQ(fake_factory_->video_codec_create_count(), 3);
    // A delay decoded too large (e.g. as seconds) is caught by the
    // kRetryWaitTimeoutMs bound above.
    EXPECT_GE(elapsed, std::chrono::milliseconds(30 + 60));
  }
}

TEST_F(MediaCodecVideoDecoderTest,
       BufferedInputsAreReleasedWhenCodecCreationRetriesAreExhausted) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/1,
                                        /*codec_creation_failures=*/10);
  InitializeDecoder();

  scoped_refptr<InputBuffer> input_buffer =
      CreateDummyVideoInputBuffer(0, 1024);
  decoder_->WriteInputBuffers({input_buffer});
  // The decoder holds a reference while the input is buffered for the retry.
  EXPECT_FALSE(input_buffer->HasOneRef());

  ASSERT_TRUE(PumpJobQueueUntil([this] { return error_called_.load(); },
                                kRetryWaitTimeoutMs));
  EXPECT_EQ(last_error_, kSbPlayerErrorDecode);
  // Once no codec will be created, the decoder no longer holds the input.
  EXPECT_TRUE(input_buffer->HasOneRef());
}

TEST_F(MediaCodecVideoDecoderTest,
       EndOfStreamIsForwardedAcrossMultipleCodecCreationRetries) {
  // The end of stream arrives while the first retry is pending, and the first
  // retry fails too, so it has to carry over to the second one.
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/2);
  InitializeDecoder();

  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  decoder_->WriteEndOfStream();

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 3; },
      kRetryWaitTimeoutMs));
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  fake_codec->SimulateInputBufferAvailable(0);
  fake_codec->SimulateInputBufferAvailable(1);
  ASSERT_TRUE(fake_codec->WaitForInputQueue(2, 1000));
  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 2U);
  EXPECT_EQ(queued_inputs[0].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_NE(queued_inputs[1].flags & MediaCodec::kBufferFlagEndOfStream, 0);

  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       FpsGatedCodecCreationRetryKeepsRequestingInput) {
  // AV1 codec creation is deferred until enough inputs arrive to estimate the
  // fps.
  ScopedForceAv1FpsCheck force_av1_fps_check;
  CreateDecoder(/*max_video_capabilities=*/"", &kAv1VideoStreamInfo,
                CodecCreationRetryFeatures(3),
                /*enable_flush_during_seek=*/false,
                /*codec_creation_failures=*/1);
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 0);

  int need_more_input_count = 0;
  decoder_->Initialize(
      [&](VideoDecoder::Status status, const scoped_refptr<VideoFrame>& frame) {
        if (status == VideoDecoder::kNeedMoreInput) {
          need_more_input_count++;
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  // The third input triggers codec creation, which fails and schedules a
  // retry. The decoder keeps requesting input while the retry is pending.
  for (int i = 0; i < 3; ++i) {
    decoder_->WriteInputBuffers(
        {CreateDummyVideoInputBuffer(i * 33'333, 1024, &kAv1VideoStreamInfo)});
  }
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 1);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);
  EXPECT_EQ(need_more_input_count, 3);

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 2; },
      kRetryWaitTimeoutMs));
  EXPECT_NE(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       FpsGatedEndOfStreamCreatesCodecAndForwardsEndOfStreamOnce) {
  ScopedForceAv1FpsCheck force_av1_fps_check;
  CreateDecoder(/*max_video_capabilities=*/"", &kAv1VideoStreamInfo);
  InitializeDecoder();

  // The end of stream arrives before enough inputs for the fps estimate, so
  // it triggers codec creation.
  decoder_->WriteInputBuffers(
      {CreateDummyVideoInputBuffer(0, 1024, &kAv1VideoStreamInfo)});
  decoder_->WriteEndOfStream();
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  for (int i = 0; i < 3; ++i) {
    fake_codec->SimulateInputBufferAvailable(i);
  }
  ASSERT_TRUE(fake_codec->WaitForInputQueue(2, 1000));
  // No second end of stream follows.
  EXPECT_FALSE(fake_codec->WaitForInputQueue(3, 200));
  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 2U);
  EXPECT_EQ(queued_inputs[0].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_NE(queued_inputs[1].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       FpsGatedEndOfStreamDuringCodecCreationRetryIsForwardedOnce) {
  ScopedForceAv1FpsCheck force_av1_fps_check;
  CreateDecoder(/*max_video_capabilities=*/"", &kAv1VideoStreamInfo,
                CodecCreationRetryFeatures(3),
                /*enable_flush_during_seek=*/false,
                /*codec_creation_failures=*/1);
  InitializeDecoder();
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 0);

  // The end of stream arrives before enough inputs for the fps estimate, so
  // it triggers codec creation, which fails and schedules a retry.
  decoder_->WriteInputBuffers(
      {CreateDummyVideoInputBuffer(0, 1024, &kAv1VideoStreamInfo)});
  decoder_->WriteEndOfStream();
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 1);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 2; },
      kRetryWaitTimeoutMs));
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  for (int i = 0; i < 3; ++i) {
    fake_codec->SimulateInputBufferAvailable(i);
  }
  ASSERT_TRUE(fake_codec->WaitForInputQueue(2, 1000));
  // No second end of stream follows.
  EXPECT_FALSE(fake_codec->WaitForInputQueue(3, 200));
  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 2U);
  EXPECT_EQ(queued_inputs[0].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_NE(queued_inputs[1].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       CodecCreationSucceedsAfterMultipleRetriesWithoutInput) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/2);
  InitializeDecoder();
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 1);

  // Initial attempt + 1 failed retry + 1 successful retry, all without any
  // input written, so the retries reuse the constructor's stream info.
  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 3; },
      kRetryWaitTimeoutMs));
  EXPECT_NE(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);

  // Inputs written after the codec exists go straight to it.
  GetFakeVideoCodec()->SimulateInputBufferAvailable(0);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  ASSERT_TRUE(GetFakeVideoCodec()->WaitForInputQueue(1, 1000));
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       EndOfStreamWithoutInputCancelsPendingCodecCreationRetry) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/1);

  int eos_frame_count = 0;
  decoder_->Initialize(
      [&](VideoDecoder::Status status, const scoped_refptr<VideoFrame>& frame) {
        if (frame && frame->is_end_of_stream()) {
          eos_frame_count++;
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  // EOS before any input ends the stream directly; no codec is needed.
  decoder_->WriteEndOfStream();
  EXPECT_EQ(eos_frame_count, 1);

  PumpJobQueueFor(300);
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 1);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       CapabilityChangedErrorCancelsPendingCodecCreationRetry) {
  VideoStreamInfo initial_info = kDefaultVideoStreamInfo;
  initial_info.frame_size = {1280, 720};
  CreateDecoder("width=1280; height=720", &initial_info,
                CodecCreationRetryFeatures(3),
                /*enable_flush_during_seek=*/false,
                /*codec_creation_failures=*/1);
  InitializeDecoder();
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 1);

  VideoStreamInfo oversized_info = kDefaultVideoStreamInfo;
  oversized_info.frame_size = {1920, 1080};
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(
      0, 1024, &oversized_info, /*is_key_frame=*/true)});
  ASSERT_TRUE(error_called_.load());
  EXPECT_EQ(last_error_, kSbPlayerErrorCapabilityChanged);

  // The errored decoder must not create a codec afterwards.
  PumpJobQueueFor(300);
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 1);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);
}

TEST_F(MediaCodecVideoDecoderTest, HdrColorMetadataSurvivesCodecCreationRetry) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/1);
  InitializeDecoder();

  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(
      0, 1024, &kHdrVideoStreamInfo, /*is_key_frame=*/true)});
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 2; },
      kRetryWaitTimeoutMs));
  ASSERT_NE(GetFakeVideoCodec(), nullptr);

  auto color_metadata = fake_factory_->last_video_color_metadata();
  ASSERT_TRUE(color_metadata.has_value());
  EXPECT_EQ(color_metadata->primaries, kSbMediaPrimaryIdBt2020);
  EXPECT_EQ(color_metadata->transfer, kSbMediaTransferIdSmpteSt2084);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       ResetDuringPendingCodecCreationRetryCancelsRetry) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/1);
  InitializeDecoder();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});

  decoder_->Reset();

  // The scheduled retry must not fire after Reset().
  PumpJobQueueFor(300);
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 1);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);

  // The next input starts a new initialization episode.
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 2);
  EXPECT_NE(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest, PostResetCodecCreationFailureIsRetried) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/1,
                                        /*codec_creation_failures=*/0);
  InitializeDecoder();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 1);

  // Without flush-during-seek, Reset() tears the codec down; the next input
  // recreates it.
  decoder_->Reset();
  fake_factory_->set_video_codec_creation_failures_remaining(1);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  EXPECT_EQ(fake_factory_->video_codec_create_count(), 2);
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 3; },
      kRetryWaitTimeoutMs));
  EXPECT_NE(GetFakeVideoCodec(), nullptr);
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       Av1DelayedInitSkipsSurfaceCleanupWhenGuarded) {
  ScopedForceAv1FpsCheck force_av1_fps_check;
  ScopedGlobalVideoSurface scoped_surface;
  ASSERT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  // Destroying an uninitialized AV1 decoder with 0 inputs should not clean up
  // the video surface when bit 0 is enabled.
  CreateDecoder(
      /*max_video_capabilities=*/"", &kAv1VideoStreamInfo,
      MakeVideoDecoderImprovementsFeatures(
          android_video_decoder_improvements::
              kGuardSurfaceResetAndCleanupByOwnership));
  ASSERT_EQ(GetFakeVideoCodec(), nullptr);
  decoder_.reset();
  EXPECT_EQ(gles_context_runner_count_, 0);

  // Destroying an AV1 decoder after fewer than 3 inputs (even when
  // `ResetForTeardown()` runs first, as in `~VideoRendererImpl()`) should also
  // skip surface cleanup because `InitializeCodec()` still has not run.
  CreateDecoder(
      /*max_video_capabilities=*/"", &kAv1VideoStreamInfo,
      MakeVideoDecoderImprovementsFeatures(
          android_video_decoder_improvements::
              kGuardSurfaceResetAndCleanupByOwnership));
  InitializeDecoder();
  decoder_->WriteInputBuffers(
      {CreateDummyVideoInputBuffer(0, 1024, &kAv1VideoStreamInfo)});
  ASSERT_EQ(GetFakeVideoCodec(), nullptr);
  decoder_->ResetForTeardown();
  decoder_.reset();
  EXPECT_EQ(gles_context_runner_count_, 0);
}

TEST_F(MediaCodecVideoDecoderTest,
       Av1DelayedInitCleansUpSurfaceWhenUnguardedOrNegativePlacebo) {
  ScopedForceAv1FpsCheck force_av1_fps_check;
  ScopedGlobalVideoSurface scoped_surface;
  ASSERT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  CreateDecoder(/*max_video_capabilities=*/"", &kAv1VideoStreamInfo);
  ASSERT_EQ(GetFakeVideoCodec(), nullptr);
  decoder_.reset();
  EXPECT_EQ(gles_context_runner_count_, 1);

  CreateDecoder(/*max_video_capabilities=*/"", &kAv1VideoStreamInfo,
                MakeVideoDecoderImprovementsFeatures(-1));
  ASSERT_EQ(GetFakeVideoCodec(), nullptr);
  decoder_.reset();
  EXPECT_EQ(gles_context_runner_count_, 2);
}

TEST_F(MediaCodecVideoDecoderTest,
       InitializedDecodersStillCleanUpSurfaceAfterResetForTeardownWhenGuarded) {
  ScopedForceAv1FpsCheck force_av1_fps_check;
  ScopedGlobalVideoSurface scoped_surface;
  ASSERT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  // Eagerly initialized H.264 playback torn down via `ResetForTeardown()`.
  CreateDecoder(
      /*max_video_capabilities=*/"", /*initial_stream_info=*/nullptr,
      MakeVideoDecoderImprovementsFeatures(
          android_video_decoder_improvements::
              kGuardSurfaceResetAndCleanupByOwnership));
  InitializeDecoder();
  ASSERT_NE(GetFakeVideoCodec(), nullptr);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  decoder_->ResetForTeardown();
  decoder_.reset();
  EXPECT_EQ(gles_context_runner_count_, 1);

  // Delayed AV1 playback once 3 inputs trigger `InitializeCodec()`, then torn
  // down via `ResetForTeardown()`.
  CreateDecoder(
      /*max_video_capabilities=*/"", &kAv1VideoStreamInfo,
      MakeVideoDecoderImprovementsFeatures(
          android_video_decoder_improvements::
              kGuardSurfaceResetAndCleanupByOwnership));
  InitializeDecoder();
  ASSERT_EQ(GetFakeVideoCodec(), nullptr);
  decoder_->WriteInputBuffers(
      {CreateDummyVideoInputBuffer(0, 1024, &kAv1VideoStreamInfo),
       CreateDummyVideoInputBuffer(33'333, 1024, &kAv1VideoStreamInfo),
       CreateDummyVideoInputBuffer(66'666, 1024, &kAv1VideoStreamInfo)});
  ASSERT_NE(GetFakeVideoCodec(), nullptr);
  decoder_->ResetForTeardown();
  decoder_.reset();
  EXPECT_EQ(gles_context_runner_count_, 2);
}

TEST_F(MediaCodecVideoDecoderTest,
       GlobalSurfaceDestroyedOrHeldByOtherDecoderSkipsCleanupWhenGuarded) {
  ScopedGlobalVideoSurface scoped_surface;
  ASSERT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  MediaCodecVideoDecoder::StreamConfig global_surface_stream_config{
      kDefaultVideoStreamInfo,
      /*drm_system=*/kSbDrmSystemInvalid, kSbPlayerOutputModePunchOut,
      /*decode_target_graphics_context_provider=*/
      &fake_graphics_context_provider_,
      /*surface_view=*/nullptr};
  MediaCodecVideoDecoder::TunnelModeConfig tunnel_config;
  MediaCodecVideoDecoder::PipelineConfig pipeline_config;
  pipeline_config.experimental_features = MakeVideoDecoderImprovementsFeatures(
      android_video_decoder_improvements::
          kGuardSurfaceResetAndCleanupByOwnership);
  MediaCodecVideoDecoder::PlatformOptions platform_options;

  auto first_decoder = MediaCodecVideoDecoder::CreateForTesting(
      std::make_unique<FakeMediaCodecFactory>(), &job_queue_,
      global_surface_stream_config, tunnel_config, pipeline_config,
      platform_options);
  ASSERT_TRUE(first_decoder);
  EXPECT_FALSE(VideoSurfaceHolder::IsVideoSurfaceAvailable());

  // A second punch-out decoder failing to acquire the global surface must not
  // clear the first decoder's active surface when its failed instance is
  // destroyed.
  auto second_decoder = MediaCodecVideoDecoder::CreateForTesting(
      std::make_unique<FakeMediaCodecFactory>(), &job_queue_,
      global_surface_stream_config, tunnel_config, pipeline_config,
      platform_options);
  EXPECT_FALSE(second_decoder);
  EXPECT_EQ(gles_context_runner_count_, 0);

  // Replacing the global surface triggers `OnSurfaceDestroyed()` on
  // `first_decoder`; destroying `first_decoder` afterwards must not clear the
  // replacement surface.
  scoped_surface.ReplaceSurface();
  EXPECT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
  first_decoder.value().reset();
  EXPECT_EQ(gles_context_runner_count_, 0);
}

TEST_F(MediaCodecVideoDecoderTest, AndroidVideoDecoderImprovementsBitfield) {
  using android_video_decoder_improvements::
      kGuardSurfaceResetAndCleanupByOwnership;
  constexpr uint32_t kOtherImprovementBit = 1u << 1;

  EXPECT_FALSE(IsAndroidVideoDecoderImprovementEnabled(
      GetAndroidVideoDecoderImprovementsFlags(ExperimentalFeatures()),
      kGuardSurfaceResetAndCleanupByOwnership));

  EXPECT_FALSE(IsAndroidVideoDecoderImprovementEnabled(
      GetAndroidVideoDecoderImprovementsFlags(
          MakeVideoDecoderImprovementsFeatures(0)),
      kGuardSurfaceResetAndCleanupByOwnership));

  // Negative values act as disabled/placebo even though bit 0 is set in two's
  // complement representation.
  EXPECT_FALSE(IsAndroidVideoDecoderImprovementEnabled(
      GetAndroidVideoDecoderImprovementsFlags(
          MakeVideoDecoderImprovementsFeatures(-1)),
      kGuardSurfaceResetAndCleanupByOwnership));

  EXPECT_FALSE(IsAndroidVideoDecoderImprovementEnabled(
      GetAndroidVideoDecoderImprovementsFlags(
          MakeVideoDecoderImprovementsFeatures(kOtherImprovementBit)),
      kGuardSurfaceResetAndCleanupByOwnership));

  EXPECT_TRUE(IsAndroidVideoDecoderImprovementEnabled(
      GetAndroidVideoDecoderImprovementsFlags(
          MakeVideoDecoderImprovementsFeatures(
              kGuardSurfaceResetAndCleanupByOwnership)),
      kGuardSurfaceResetAndCleanupByOwnership));

  EXPECT_TRUE(IsAndroidVideoDecoderImprovementEnabled(
      GetAndroidVideoDecoderImprovementsFlags(
          MakeVideoDecoderImprovementsFeatures(
              kGuardSurfaceResetAndCleanupByOwnership | kOtherImprovementBit)),
      kGuardSurfaceResetAndCleanupByOwnership));
}

}  // namespace
}  // namespace starboard
