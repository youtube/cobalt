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

#include <android/api-level.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

#include "base/android/jni_string.h"
#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/experimental_features.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/testing/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::jni_zero::AttachCurrentThread;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;

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

// Fakes the video surface of VideoSurfaceView, and records how the decoder
// uses it.
class FakeVideoSurface : public MediaCodecVideoDecoder::VideoSurface {
 public:
  // Simulates another decoder holding the surface.
  void set_held_by_another_holder(bool held_by_another_holder) {
    held_by_another_holder_.store(held_by_another_holder);
  }

  bool held() const { return held_.load(); }
  int clean_up_count() const { return clean_up_count_.load(); }
  int reset_count() const { return reset_count_.load(); }
  int show_with_next_frame_count() const {
    return show_with_next_frame_count_.load();
  }
  std::vector<bool> visibility_changes() const {
    std::lock_guard lock(mutex_);
    return visibility_changes_;
  }

  // MediaCodecVideoDecoder::VideoSurface implementation.
  jni_zero::ScopedJavaLocalRef<jobject> Acquire() override {
    if (held_.load() || held_by_another_holder_.load()) {
      return {};
    }
    held_.store(true);
    return surface_.AsLocalRef(AttachCurrentThread());
  }
  void Release() override { held_.store(false); }
  bool IsHeldByAnotherHolder() const override {
    return held_by_another_holder_.load();
  }
  void CleanUp(SbDecodeTargetGraphicsContextProvider* gpu_provider) override {
    clean_up_count_++;
  }
  void Reset() override { reset_count_++; }
  void SetVisible(bool visible) override {
    std::lock_guard lock(mutex_);
    visibility_changes_.push_back(visible);
  }
  void ShowWithNextFrame() override { show_with_next_frame_count_++; }

 private:
  const jni_zero::ScopedJavaGlobalRef<jstring> surface_{
      base::android::ConvertUTF8ToJavaString(AttachCurrentThread(),
                                             "fake_video_surface")};
  std::atomic<bool> held_{false};
  std::atomic<bool> held_by_another_holder_{false};
  std::atomic<int> clean_up_count_{0};
  std::atomic<int> reset_count_{0};
  std::atomic<int> show_with_next_frame_count_{0};
  mutable std::mutex mutex_;
  std::vector<bool> visibility_changes_;  // Guarded by |mutex_|.
};

class MediaCodecVideoDecoderTest : public ::testing::Test {
 protected:
  // The decoder renders to an AndroidOverlay surface if |use_android_overlay|
  // is set, and to |fake_video_surface_| otherwise.
  void CreateDecoder(const std::string& max_video_capabilities = "",
                     const VideoStreamInfo* initial_stream_info = nullptr,
                     ExperimentalFeatures experimental_features = {},
                     bool enable_flush_during_seek = false,
                     std::optional<int> tunnel_mode_audio_session_id = {},
                     bool use_android_overlay = true) {
    auto factory = std::make_unique<FakeMediaCodecFactory>();
    fake_factory_ = factory.get();  // Save raw pointer before moving!

    MediaCodecVideoDecoder::StreamConfig stream_config{
        initial_stream_info ? *initial_stream_info : kDefaultVideoStreamInfo,
        /*drm_system=*/kSbDrmSystemInvalid, kSbPlayerOutputModePunchOut,
        /*decode_target_graphics_context_provider=*/
        &fake_graphics_context_provider_,
        /*surface_view=*/use_android_overlay ? dummy_surface_.obj() : nullptr,
        max_video_capabilities};

    MediaCodecVideoDecoder::TunnelModeConfig tunnel_config;
    tunnel_config.audio_session_id = tunnel_mode_audio_session_id;
    MediaCodecVideoDecoder::PipelineConfig pipeline_config;
    pipeline_config.experimental_features = std::move(experimental_features);
    pipeline_config.enable_flush_during_seek = enable_flush_during_seek;
    MediaCodecVideoDecoder::PlatformOptions platform_options;

    auto result = MediaCodecVideoDecoder::CreateForTesting(
        std::move(factory),  // Transfer ownership
        &fake_video_surface_, &job_queue_, stream_config, tunnel_config,
        pipeline_config, platform_options);

    ASSERT_TRUE(result);
    decoder_ = std::move(result.value());

    // Setup sink to avoid crashes in Tick() on decoder thread
    auto sink = decoder_->GetSink();
    ASSERT_NE(sink, nullptr);
    sink->SetRenderCB([this](
                          const VideoRendererSink::DrawFrameCB& draw_frame_cb) {
      {
        std::lock_guard lock(callback_mutex_);
        render_count_++;
        if (on_render_cb_) {
          on_render_cb_();
        }
      }
      std::lock_guard lock(render_mutex_);
      while (!frames_to_draw_.empty()) {
        scoped_refptr<VideoFrame> frame = std::move(frames_to_draw_.front());
        frames_to_draw_.pop_front();
        draw_frame_cb(frame, frame->timestamp() * 1000);
      }
    });
  }

  void QueueFrameForDraw(const scoped_refptr<VideoFrame>& frame) {
    std::lock_guard lock(render_mutex_);
    frames_to_draw_.push_back(frame);
  }

  // Creates a decoder that renders to the video surface of VideoSurfaceView,
  // i.e. |fake_video_surface_|, instead of an AndroidOverlay surface.
  void CreateDecoderForVideoSurfaceView(
      bool enable_flush_during_seek = false,
      std::optional<int> tunnel_mode_audio_session_id = {}) {
    CreateDecoder(/*max_video_capabilities=*/"",
                  /*initial_stream_info=*/nullptr,
                  /*experimental_features=*/{}, enable_flush_during_seek,
                  tunnel_mode_audio_session_id, /*use_android_overlay=*/false);
  }

  // Initializes the decoder, which then draws the decoded frames on the next
  // render while |draw_frames_| is set, and drops them otherwise.
  void InitializeDecoderWithDrawing() {
    decoder_->Initialize(
        [this](VideoDecoder::Status status,
               const scoped_refptr<VideoFrame>& frame) {
          if (frame && !frame->is_end_of_stream() && draw_frames_.load()) {
            QueueFrameForDraw(frame);
          }
        },
        [this](SbPlayerError error, const std::string& msg) {
          error_called_ = true;
        });
  }

  // Decodes a frame at |timestamp| via the input and output buffer |index| of
  // the current codec. Returns whether the frame was released for rendering,
  // as opposed to being dropped, or std::nullopt on failure.
  std::optional<bool> DecodeFrame(int index, int64_t timestamp) {
    decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(timestamp, 1024)});
    // Writing may have created a new codec, e.g. after a seek.
    FakeMediaCodec* codec = GetFakeVideoCodec();
    if (!codec) {
      return std::nullopt;
    }
    codec->SimulateInputBufferAvailable(index);
    if (!codec->WaitForInputQueue(1, 1000)) {
      return std::nullopt;
    }
    codec->ConsumeQueuedInputs();

    codec->SimulateOutputAvailable(index, /*flags=*/0, /*offset=*/0, timestamp,
                                   /*size=*/1024);
    if (!codec->WaitForOutputReleased(1, 1000)) {
      return std::nullopt;
    }
    std::vector<FakeMediaCodec::ReleasedOutput> released =
        codec->ConsumeReleasedOutputs();
    if (released.size() != 1) {
      return std::nullopt;
    }
    return released[0].render;
  }

  // Simulates Android destroying the video surface, e.g. on app suspend. Like
  // JNI_VideoSurfaceView_OnVideoSurfaceChanged(), this notifies the decoder
  // holding the surface, which no longer holds it afterwards.
  void DestroyVideoSurface() {
    decoder_->OnSurfaceDestroyed();
    fake_video_surface_.Release();
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
                         });
  }

  std::atomic<bool> error_called_{false};
  // Whether a decoder initialized via InitializeDecoderWithDrawing() draws the
  // decoded frames.
  std::atomic<bool> draw_frames_{true};

  std::mutex callback_mutex_;
  std::mutex render_mutex_;
  std::deque<scoped_refptr<VideoFrame>> frames_to_draw_;
  int need_more_input_count_ = 0;
  int render_count_ = 0;
  std::function<void()> on_render_cb_;
  JobQueue job_queue_;
  FakeMediaCodecFactory* fake_factory_ = nullptr;
  // Punch-out decoders pass it to VideoSurface::CleanUp() on destruction.
  SbDecodeTargetGraphicsContextProvider fake_graphics_context_provider_{
      nullptr, nullptr,
      [](SbDecodeTargetGraphicsContextProvider*,
         SbDecodeTargetGlesContextRunnerTarget,
         void*) {},
      nullptr};
  const jni_zero::ScopedJavaGlobalRef<jstring> dummy_surface_{
      base::android::ConvertUTF8ToJavaString(AttachCurrentThread(),
                                             "dummy_surface")};
  // Must outlive |decoder_|.
  FakeVideoSurface fake_video_surface_;

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

#if !BUILDFLAG(IS_STARBOARD)
TEST_F(MediaCodecVideoDecoderTest,
       SurfaceCleanupDefaultUsesCleanUpVideoSurface) {
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView();
  InitializeDecoderWithDrawing();
  EXPECT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 0);

  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 1);
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    ApplyTransactionToFrameOnSurfaceCleanupHidesOnTeardownAndShowsOnFirstDraw) {
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView();
  InitializeDecoderWithDrawing();

  // Frame 0 is dropped without drawing: should not show the surface.
  draw_frames_.store(false);
  EXPECT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(false));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 0);

  // Frame 1 is drawn: should show the surface together with it on API 34+, and
  // frame 2 should not show it a second time.
  draw_frames_.store(true);
  EXPECT_THAT(DecodeFrame(/*index=*/1, /*timestamp=*/20000), Optional(true));
  EXPECT_THAT(DecodeFrame(/*index=*/2, /*timestamp=*/30000), Optional(true));

  if (android_get_device_api_level() >= 34) {
    EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 1);
    EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());

    decoder_.reset();
    EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
    EXPECT_EQ(fake_video_surface_.visibility_changes(),
              std::vector<bool>{false});
  } else {
    EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 0);
    decoder_.reset();
    EXPECT_EQ(fake_video_surface_.clean_up_count(), 1);
    EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());
  }
  EXPECT_FALSE(error_called_);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    ApplyTransactionToFrameOnSurfaceCleanupHidesOnceAcrossResetForTeardownAndDestruction) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  // With enable_flush_during_seek=false, ResetForTeardown() tears down the
  // codec and releases the surface before ~MediaCodecVideoDecoder() runs.
  CreateDecoderForVideoSurfaceView(/*enable_flush_during_seek=*/false);
  InitializeDecoder();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(10000, 1024)});

  decoder_->ResetForTeardown();
  EXPECT_FALSE(fake_video_surface_.held());
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{false});

  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{false});
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       ApplyTransactionToFrameOnSurfaceCleanupHidesAfterSeekResetBeforeReinit) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView(/*enable_flush_during_seek=*/false);
  InitializeDecoderWithDrawing();
  ASSERT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 1);

  // A seek with enable_flush_during_seek=false tears down the codec and
  // releases the surface. If the decoder is then destroyed before the next
  // input buffer re-initializes the codec, the surface must still be hidden
  // because a frame was drawn and the surface was not destroyed.
  decoder_->Reset();
  EXPECT_FALSE(fake_video_surface_.held());
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());

  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{false});
  EXPECT_FALSE(error_called_);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    ApplyTransactionToFrameOnSurfaceCleanupDoesNotHideAfterSurfaceDestroyed) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView();
  InitializeDecoderWithDrawing();
  ASSERT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 1);

  DestroyVideoSurface();
  decoder_->ResetForTeardown();
  decoder_.reset();

  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    ApplyTransactionToFrameOnSurfaceCleanupHidesSurfaceAcquiredAfterDestroy) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView();
  InitializeDecoderWithDrawing();
  ASSERT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 1);

  // Simulate Android destroying the surface (e.g. on app suspend), and the
  // decoder acquiring a new surface after a seek (e.g. on app resume).
  DestroyVideoSurface();
  decoder_->Reset();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(20000, 1024)});
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 2);
  EXPECT_TRUE(fake_video_surface_.held());

  // The decoder holds the new surface, so it must hide it on teardown.
  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{false});
  EXPECT_FALSE(error_called_);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    ApplyTransactionToFrameOnSurfaceCleanupHidesSurfaceReleasedAfterReacquiring) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView(/*enable_flush_during_seek=*/false);
  InitializeDecoderWithDrawing();
  ASSERT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));

  // Simulate Android destroying the surface (e.g. on app suspend), and the
  // decoder rendering to a new surface after a seek (e.g. on app resume).
  DestroyVideoSurface();
  decoder_->Reset();
  ASSERT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/20000), Optional(true));
  ASSERT_EQ(fake_factory_->video_codec_create_count(), 2);

  // Another seek releases the new surface after rendering to it. The surface
  // must still be hidden on teardown, as only the previous one was destroyed.
  decoder_->Reset();
  EXPECT_FALSE(fake_video_surface_.held());
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());

  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{false});
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       ApplyTransactionToFrameOnSurfaceCleanupDoesNotHideSurfaceOfOtherHolder) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView(/*enable_flush_during_seek=*/false);
  InitializeDecoderWithDrawing();
  ASSERT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 1);

  // A seek with enable_flush_during_seek=false tears down the codec and
  // releases the surface. If another decoder acquires the surface before this
  // decoder is destroyed, this decoder must not hide it, as the other decoder
  // may already be rendering to it.
  decoder_->Reset();
  fake_video_surface_.set_held_by_another_holder(true);
  decoder_.reset();

  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       ApplyTransactionToFrameOnSurfaceCleanupShowsOnFirstTunnelFrame) {
  if (android_get_device_api_level() < 34) {
    GTEST_SKIP() << "Requires API level 34+";
  }
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  CreateDecoderForVideoSurfaceView(/*enable_flush_during_seek=*/false,
                                   /*tunnel_mode_audio_session_id=*/1);
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  // A previous non-tunnel playback may have hidden the surface, so it is shown
  // when the first tunnel mode frame is ready.
  fake_codec->SimulateFirstTunnelFrameReady();
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{true});

  // Tunnel mode playbacks reset the surface on teardown, instead of hiding or
  // cleaning it up.
  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.reset_count(), 1);
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 0);
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 0);
  EXPECT_EQ(fake_video_surface_.visibility_changes(), std::vector<bool>{true});
}

TEST_F(MediaCodecVideoDecoderTest,
       ApplyTransactionToFrameOnSurfaceCleanupIgnoresAndroidOverlaySurfaces) {
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);

  // A decoder rendering to an AndroidOverlay surface, e.g. for a secondary
  // video, must not change the visibility of VideoSurfaceView, which another
  // decoder may be rendering to. It cleans up the surface like without the
  // feature.
  CreateDecoder();
  InitializeDecoderWithDrawing();
  EXPECT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_FALSE(fake_video_surface_.held());
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 0);

  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 1);
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       ApplyTransactionToFrameOnSurfaceCleanupIsDisabledWithExoPlayer) {
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);
  // ExoPlayer doesn't show the surface again after a decoder hid it.
  scoped_feature_list.InitAndEnableFeature(features::kEnableExoPlayer);

  CreateDecoderForVideoSurfaceView();
  InitializeDecoderWithDrawing();
  EXPECT_THAT(DecodeFrame(/*index=*/0, /*timestamp=*/10000), Optional(true));
  EXPECT_EQ(fake_video_surface_.show_with_next_frame_count(), 0);

  decoder_.reset();
  EXPECT_EQ(fake_video_surface_.clean_up_count(), 1);
  EXPECT_TRUE(fake_video_surface_.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}
#endif  // !BUILDFLAG(IS_STARBOARD)

}  // namespace
}  // namespace starboard
