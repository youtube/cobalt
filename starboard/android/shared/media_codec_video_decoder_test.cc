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
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "base/android/jni_string.h"
#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/experimental_features.h"
#include "starboard/shared/starboard/player/job_queue.h"
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

  // Experimental features setting the H5VCC codec creation retry count.
  static ExperimentalFeatures CodecCreationRetryFeatures(int retry_count) {
    return ExperimentalFeatures(ExperimentalFeatures::Map{
        {std::string(kMediaVideoCodecCreationRetryCount.key()),
         static_cast<int64_t>(retry_count)}});
  }

  // Creates a decoder with a retry budget of |codec_creation_retry_count|,
  // whose first |codec_creation_failures| MediaCodec creations fail.
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

// Codec creation retry tests. Retries are scheduled on the job queue with a
// 50ms base delay and linear backoff, so the tests pump the queue with a
// generous timeout.
constexpr int kRetryWaitTimeoutMs = 2000;

TEST_F(MediaCodecVideoDecoderTest,
       CodecCreationFailureWithoutRetriesFailsCreate) {
  auto result = TryCreateDecoder(/*max_video_capabilities=*/"",
                                 /*initial_stream_info=*/nullptr,
                                 /*experimental_features=*/{},
                                 /*enable_flush_during_seek=*/false,
                                 /*codec_creation_failures=*/1);

  ASSERT_FALSE(result);
  // The failed decoder (and the fake factory it owns) is destroyed, so only
  // the error message can be inspected.
  EXPECT_NE(result.error().find("Simulated MediaCodec creation failure"),
            std::string::npos);
  fake_factory_ = nullptr;
}

TEST_F(MediaCodecVideoDecoderTest,
       NegativeCodecCreationRetryCountDisablesRetries) {
  auto result = TryCreateDecoder(/*max_video_capabilities=*/"",
                                 /*initial_stream_info=*/nullptr,
                                 CodecCreationRetryFeatures(-1),
                                 /*enable_flush_during_seek=*/false,
                                 /*codec_creation_failures=*/1);

  EXPECT_FALSE(result);
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
       EndOfStreamDuringPendingCodecCreationRetryIsForwarded) {
  CreateDecoderWithCodecCreationRetries(/*codec_creation_retry_count=*/3,
                                        /*codec_creation_failures=*/1);
  InitializeDecoder();

  decoder_->WriteInputBuffers({CreateDummyVideoInputBuffer(0, 1024)});
  decoder_->WriteEndOfStream();
  EXPECT_EQ(GetFakeVideoCodec(), nullptr);

  ASSERT_TRUE(PumpJobQueueUntil(
      [this] { return fake_factory_->video_codec_create_count() == 2; },
      kRetryWaitTimeoutMs));
  FakeMediaCodec* fake_codec = GetFakeVideoCodec();
  ASSERT_NE(fake_codec, nullptr);

  fake_codec->SimulateInputBufferAvailable(0);
  fake_codec->SimulateInputBufferAvailable(1);
  ASSERT_TRUE(fake_codec->WaitForInputQueue(2, 1000));
  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 2U);
  EXPECT_EQ(queued_inputs[0].pts, 0);
  EXPECT_EQ(queued_inputs[0].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_NE(queued_inputs[1].flags & MediaCodec::kBufferFlagEndOfStream, 0);

  EXPECT_FALSE(error_called_);
}

TEST_F(MediaCodecVideoDecoderTest,
       EndOfStreamIsForwardedAcrossMultipleCodecCreationRetries) {
  // The first retry fails too, so the end of stream latched during the first
  // pending retry has to carry over to the second one.
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
  // fps, as long as 8K AV1 is treated as capped at 30fps.
  MediaCapabilitiesCache::GetInstance()->SetAv1OptEnabled(false);
  ASSERT_TRUE(MediaCapabilitiesCache::GetInstance()->IsAv18kCappedAt30());
  VideoStreamInfo av1_info = kDefaultVideoStreamInfo;
  av1_info.codec = kSbMediaVideoCodecAv1;
  CreateDecoder(/*max_video_capabilities=*/"", &av1_info,
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
        {CreateDummyVideoInputBuffer(i * 33'333, 1024, &av1_info)});
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

}  // namespace
}  // namespace starboard
