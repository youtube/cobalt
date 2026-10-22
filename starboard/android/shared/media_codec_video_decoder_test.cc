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
#include <vector>

#include "base/android/jni_string.h"
#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/video_window.h"
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

class MediaCodecVideoDecoderTest : public ::testing::Test {
 protected:
  void CreateDecoder(const std::string& max_video_capabilities = "",
                     const VideoStreamInfo* initial_stream_info = nullptr,
                     ExperimentalFeatures experimental_features = {},
                     bool enable_flush_during_seek = false) {
    auto factory = std::make_unique<FakeMediaCodecFactory>();
    fake_factory_ = factory.get();  // Save raw pointer before moving!

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

    auto result = MediaCodecVideoDecoder::CreateForTesting(
        std::move(factory),  // Transfer ownership
        &job_queue_, stream_config, tunnel_config, pipeline_config,
        platform_options);

    ASSERT_TRUE(result);
    decoder_ = std::move(result.value());

    // Setup sink to avoid crashes in Tick() on decoder thread
    auto sink = decoder_->GetSink();
    ASSERT_NE(sink, nullptr);
    sink->SetRenderCB(
        [this](const VideoRendererSink::DrawFrameCB& draw_frame_cb) {
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

  std::mutex callback_mutex_;
  std::mutex render_mutex_;
  std::deque<scoped_refptr<VideoFrame>> frames_to_draw_;
  int need_more_input_count_ = 0;
  int render_count_ = 0;
  std::function<void()> on_render_cb_;
  JobQueue job_queue_;
  FakeMediaCodecFactory* fake_factory_ = nullptr;
  // Punch-out decoders clean up the global video window on destruction, which
  // requires a provider when the test activity's video surface exists.
  SbDecodeTargetGraphicsContextProvider fake_graphics_context_provider_{
      nullptr, nullptr,
      [](SbDecodeTargetGraphicsContextProvider*,
         SbDecodeTargetGlesContextRunnerTarget,
         void*) {},
      nullptr};
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

#if !BUILDFLAG(IS_STARBOARD)
class TestVideoSurfaceObserver : public VideoSurfaceHolder::TestObserver {
 public:
  TestVideoSurfaceObserver() {
    VideoSurfaceHolder::SetTestObserverForTesting(this);
  }
  ~TestVideoSurfaceObserver() override {
    VideoSurfaceHolder::SetTestObserverForTesting(nullptr);
  }

  void OnCleanUpVideoSurface() override { clean_up_count_++; }
  void OnSetVideoSurfaceVisible(bool visible) override {
    std::lock_guard lock(mutex_);
    visibility_changes_.push_back(visible);
  }
  void OnShowVideoSurfaceWithNextFrame() override {
    show_with_next_frame_count_++;
  }

  int clean_up_count() const { return clean_up_count_.load(); }
  int show_with_next_frame_count() const {
    return show_with_next_frame_count_.load();
  }
  std::vector<bool> visibility_changes() const {
    std::lock_guard lock(mutex_);
    return visibility_changes_;
  }

 private:
  std::atomic<int> clean_up_count_{0};
  std::atomic<int> show_with_next_frame_count_{0};
  mutable std::mutex mutex_;
  std::vector<bool> visibility_changes_;
};

TEST_F(MediaCodecVideoDecoderTest,
       SurfaceCleanupDefaultUsesCleanUpVideoSurface) {
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);
  TestVideoSurfaceObserver observer;

  CreateDecoder();
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  decoder_->Initialize(
      [this](VideoDecoder::Status status,
             const scoped_refptr<VideoFrame>& frame) {
        if (frame && !frame->is_end_of_stream()) {
          QueueFrameForDraw(frame);
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  fake_codec->SimulateInputBufferAvailable(0);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(10000, 1024)});
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  fake_codec->SimulateOutputAvailable(0, 0, 0, 10000, 1024);
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  auto released = fake_codec->ConsumeReleasedOutputs();
  ASSERT_EQ(released.size(), 1U);
  EXPECT_TRUE(released[0].render);
  EXPECT_EQ(observer.show_with_next_frame_count(), 0);
  EXPECT_TRUE(observer.visibility_changes().empty());

  decoder_.reset();
  EXPECT_EQ(observer.clean_up_count(), 1);
  EXPECT_TRUE(observer.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}

TEST_F(
    MediaCodecVideoDecoderTest,
    ApplyTransactionToFrameOnSurfaceCleanupHidesOnTeardownAndShowsOnFirstDraw) {
  features::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kApplyTransactionToFrameOnSurfaceCleanup);
  TestVideoSurfaceObserver observer;

  CreateDecoder();
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  std::atomic<bool> draw_frames{false};
  decoder_->Initialize(
      [this, &draw_frames](VideoDecoder::Status status,
                           const scoped_refptr<VideoFrame>& frame) {
        if (frame && !frame->is_end_of_stream() && draw_frames.load()) {
          QueueFrameForDraw(frame);
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  fake_codec->SimulateInputBufferAvailable(0);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(10000, 1024)});
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  // Frame 0 is dropped without drawing: should not trigger
  // ShowVideoSurfaceWithNextFrame().
  fake_codec->SimulateOutputAvailable(0, 0, 0, 10000, 1024);
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  auto released = fake_codec->ConsumeReleasedOutputs();
  ASSERT_EQ(released.size(), 1U);
  EXPECT_FALSE(released[0].render);
  EXPECT_EQ(observer.show_with_next_frame_count(), 0);

  // Frame 1 is drawn: should trigger ShowVideoSurfaceWithNextFrame() on API 34+
  // and Frame 2 should not trigger it a second time.
  draw_frames.store(true);
  fake_codec->SimulateOutputAvailable(1, 0, 0, 20000, 1024);
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  released = fake_codec->ConsumeReleasedOutputs();
  ASSERT_EQ(released.size(), 1U);
  EXPECT_TRUE(released[0].render);

  fake_codec->SimulateOutputAvailable(2, 0, 0, 30000, 1024);
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  released = fake_codec->ConsumeReleasedOutputs();
  ASSERT_EQ(released.size(), 1U);
  EXPECT_TRUE(released[0].render);

  if (android_get_device_api_level() >= 34) {
    EXPECT_EQ(observer.show_with_next_frame_count(), 1);
    EXPECT_TRUE(observer.visibility_changes().empty());

    decoder_.reset();
    EXPECT_EQ(observer.clean_up_count(), 0);
    EXPECT_EQ(observer.visibility_changes(), std::vector<bool>{false});
  } else {
    EXPECT_EQ(observer.show_with_next_frame_count(), 0);
    decoder_.reset();
    EXPECT_EQ(observer.clean_up_count(), 1);
    EXPECT_TRUE(observer.visibility_changes().empty());
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
  TestVideoSurfaceObserver observer;

  // With enable_flush_during_seek=false, ResetForTeardown() tears down the
  // codec and releases the surface before ~MediaCodecVideoDecoder() runs.
  CreateDecoder(/*max_video_capabilities=*/"", /*initial_stream_info=*/nullptr,
                /*experimental_features=*/{},
                /*enable_flush_during_seek=*/false);
  InitializeDecoder();
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(10000, 1024)});

  decoder_->ResetForTeardown();
  EXPECT_EQ(observer.visibility_changes(), std::vector<bool>{false});

  decoder_.reset();
  EXPECT_EQ(observer.clean_up_count(), 0);
  EXPECT_EQ(observer.visibility_changes(), std::vector<bool>{false});
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
  TestVideoSurfaceObserver observer;

  CreateDecoder(/*max_video_capabilities=*/"", /*initial_stream_info=*/nullptr,
                /*experimental_features=*/{},
                /*enable_flush_during_seek=*/false);
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  decoder_->Initialize(
      [this](VideoDecoder::Status status,
             const scoped_refptr<VideoFrame>& frame) {
        if (frame && !frame->is_end_of_stream()) {
          QueueFrameForDraw(frame);
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  fake_codec->SimulateInputBufferAvailable(0);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(10000, 1024)});
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  fake_codec->SimulateOutputAvailable(0, 0, 0, 10000, 1024);
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  EXPECT_EQ(observer.show_with_next_frame_count(), 1);

  // A seek with enable_flush_during_seek=false tears down the codec (setting
  // owns_video_surface_=false). If the decoder is then destroyed before the
  // next input buffer re-initializes the codec, the surface must still be
  // hidden because a frame was drawn and the surface was not destroyed.
  decoder_->Reset();
  EXPECT_TRUE(observer.visibility_changes().empty());

  decoder_.reset();
  EXPECT_EQ(observer.clean_up_count(), 0);
  EXPECT_EQ(observer.visibility_changes(), std::vector<bool>{false});
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
  TestVideoSurfaceObserver observer;

  CreateDecoder();
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  decoder_->Initialize(
      [this](VideoDecoder::Status status,
             const scoped_refptr<VideoFrame>& frame) {
        if (frame && !frame->is_end_of_stream()) {
          QueueFrameForDraw(frame);
        }
      },
      [this](SbPlayerError error, const std::string& msg) {
        error_called_ = true;
      });

  fake_codec->SimulateInputBufferAvailable(0);
  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(10000, 1024)});
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  fake_codec->SimulateOutputAvailable(0, 0, 0, 10000, 1024);
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  EXPECT_EQ(observer.show_with_next_frame_count(), 1);

  // Simulate Android destroying the SurfaceView surface (e.g. on app suspend).
  decoder_->OnSurfaceDestroyed();
  decoder_->ResetForTeardown();
  decoder_.reset();

  EXPECT_EQ(observer.clean_up_count(), 0);
  EXPECT_TRUE(observer.visibility_changes().empty());
  EXPECT_FALSE(error_called_);
}
#endif  // !BUILDFLAG(IS_STARBOARD)

}  // namespace
}  // namespace starboard
