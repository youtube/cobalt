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

#include "starboard/android/shared/media_codec_decoder.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/size.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

class MockHost : public MediaCodecDecoder::Host {
 public:
  MOCK_METHOD(void,
              ProcessOutputBuffer,
              (MediaCodec * media_codec,
               const DequeueOutputResult& output,
               int number_of_pending_inputs),
              (override));
  MOCK_METHOD(void,
              OnEndOfStreamWritten,
              (MediaCodec * media_codec),
              (override));
  MOCK_METHOD(void,
              RefreshOutputFormat,
              (MediaCodec * media_codec),
              (override));
  MOCK_METHOD(bool, Tick, (MediaCodec * media_codec), (override));
  MOCK_METHOD(int64_t, GetNextTickWaitDurationUs, (), (const, override));
  MOCK_METHOD(void, OnFlushing, (), (override));
  MOCK_METHOD(bool,
              IsBufferDecodeOnly,
              (const scoped_refptr<InputBuffer>& input_buffer),
              (override));
};

const AudioStreamInfo kDefaultAudioStreamInfo = [] {
  AudioStreamInfo info;
  info.codec = kSbMediaAudioCodecAac;
  info.samples_per_second = 44100;
  info.number_of_channels = 2;
  return info;
}();

class MediaCodecDecoderTest : public ::testing::Test {
 protected:
  scoped_refptr<InputBuffer> CreateDummyAudioInputBuffer(int64_t timestamp,
                                                         int size) {
    uint8_t* buffer = new uint8_t[size];
    memset(buffer, 0, size);

    SbPlayerSampleInfo sample_info = {};
    sample_info.type = kSbMediaTypeAudio;
    sample_info.buffer = buffer;
    sample_info.buffer_size = size;
    sample_info.timestamp = timestamp;
    kDefaultAudioStreamInfo.ConvertTo(
        &sample_info.audio_sample_info.stream_info);

    auto deallocate_func = [](SbPlayer player, void* context,
                              const void* sample_buffer) {
      delete[] static_cast<const uint8_t*>(sample_buffer);
    };

    return make_scoped_refptr<InputBuffer>(deallocate_func, /*player=*/nullptr,
                                           /*context=*/nullptr, sample_info);
  }

  FakeMediaCodec* GetFakeAudioCodec() {
    return factory_.last_created_audio_codec();
  }

  JobQueue job_queue_;
  FakeMediaCodecFactory factory_;
  NiceMock<MockHost> host_;

  std::unique_ptr<MediaCodecDecoder> decoder_;
};

TEST_F(MediaCodecDecoderTest, InitializesCorrectly) {
  auto result = MediaCodecDecoder::CreateForAudio(factory_, &job_queue_, &host_,
                                                  kDefaultAudioStreamInfo,
                                                  kSbDrmSystemInvalid);
  ASSERT_TRUE(result);
  decoder_ = std::move(result.value());

  FakeMediaCodec* fake_codec = GetFakeAudioCodec();
  ASSERT_NE(fake_codec, nullptr);
}

TEST_F(MediaCodecDecoderTest, BasicDecodingFlow) {
  auto result = MediaCodecDecoder::CreateForAudio(factory_, &job_queue_, &host_,
                                                  kDefaultAudioStreamInfo,
                                                  kSbDrmSystemInvalid);
  ASSERT_TRUE(result);
  decoder_ = std::move(result.value());

  FakeMediaCodec* fake_codec = GetFakeAudioCodec();
  ASSERT_NE(fake_codec, nullptr);

  bool error_called = false;
  decoder_->Initialize(
      [&error_called](SbPlayerError error, const std::string& msg) {
        error_called = true;
      });

  // 1. Simulate hardware making input buffer available
  fake_codec->SimulateInputBufferAvailable(0);

  // 2. Write input data
  auto input_buffer = CreateDummyAudioInputBuffer(10000, 512);
  decoder_->WriteInputBuffers({input_buffer});

  // 3. Wait for FakeMediaCodec to receive the queued input
  ASSERT_TRUE(fake_codec->WaitForInputQueue(1, 1000));

  auto queued_inputs = fake_codec->ConsumeQueuedInputs();
  ASSERT_EQ(queued_inputs.size(), 1U);
  EXPECT_EQ(queued_inputs[0].index, 0);
  EXPECT_EQ(queued_inputs[0].pts, 10000);
  EXPECT_EQ(queued_inputs[0].size, 512);

  // 4. Simulate output buffer ready
  // Host expectation
  std::mutex output_mutex;
  std::condition_variable output_cv;
  bool output_processed = false;
  DequeueOutputResult processed_output;

  EXPECT_CALL(host_, ProcessOutputBuffer(_, _, _))
      .WillOnce(Invoke([&](MediaCodec* codec, const DequeueOutputResult& output,
                           int number_of_pending_inputs) {
        std::lock_guard lock(output_mutex);
        output_processed = true;
        processed_output = output;
        // Test code usually releases output buffer
        codec->ReleaseOutputBuffer(output.index, false);
        output_cv.notify_all();
      }));

  fake_codec->SimulateOutputAvailable(0, 0, 0, 10000, 512);

  // Wait for host to process output
  std::unique_lock<std::mutex> lock(output_mutex);
  ASSERT_TRUE(
      output_cv.wait_for(lock, std::chrono::seconds(1),
                         [&output_processed]() { return output_processed; }));

  EXPECT_EQ(processed_output.index, 0);
  EXPECT_EQ(processed_output.presentation_time_microseconds, 10000);
  EXPECT_EQ(processed_output.num_bytes, 512);

  // Verify output was released back to fake codec
  ASSERT_TRUE(fake_codec->WaitForOutputReleased(1, 1000));
  auto released_outputs = fake_codec->ConsumeReleasedOutputs();
  ASSERT_EQ(released_outputs.size(), 1U);
  EXPECT_EQ(released_outputs[0].index, 0);
  EXPECT_FALSE(released_outputs[0].render);

  EXPECT_FALSE(error_called);
}

// Tests of the video decoder with dual decoder threads, run with the legacy
// thread functions and with the ones behind
// Media.EnableCodecThreadImprovements.
//
// The tests drive a FakeMediaCodec from the test thread the way the MediaCodec
// callback thread would (SimulateInputBufferAvailable() /
// SimulateOutputAvailable()) and observe what reaches the codec and the mock
// host.  Timing assertions are deliberately loose (hundreds of milliseconds)
// so they only fail on lost wakeups, which would otherwise be hidden by the
// input thread's 1 second fallback wait.
class MediaCodecDecoderVideoDualThreadsTest
    : public ::testing::TestWithParam<bool> {
 protected:
  // Upper bound for the time between writing an input (or an input buffer
  // index) and the codec receiving it.  Chosen to be well below the 1 second
  // fallback wait of the input thread, so a lost wakeup fails the test.
  static constexpr int kMaxQueueLatencyMs = 500;

  bool enable_codec_thread_improvements() const { return GetParam(); }

  void SetUp() override {
    ON_CALL(host_, GetNextTickWaitDurationUs())
        .WillByDefault(
            Return(MediaCodecDecoder::kDefaultWaitForNextFrameIntervalUs));
    ON_CALL(host_, ProcessOutputBuffer(_, _, _))
        .WillByDefault(
            Invoke([](MediaCodec* codec, const DequeueOutputResult& output,
                      int number_of_pending_inputs) {
              codec->ReleaseOutputBuffer(output.index, false);
            }));
    ON_CALL(host_, Tick(_)).WillByDefault(Invoke([this](MediaCodec*) {
      {
        std::lock_guard lock(tick_mutex_);
        ++tick_count_;
      }
      tick_condition_variable_.notify_all();
      return false;
    }));

    auto result = MediaCodecDecoder::CreateForVideo(
        factory_, &job_queue_, &host_, kSbMediaVideoCodecH264, Size(1920, 1080),
        /*max_frame_size=*/std::nullopt, /*fps=*/0,
        jni_zero::JavaRef<jobject>(), kSbDrmSystemInvalid,
        /*color_metadata=*/nullptr, /*require_software_codec=*/false,
        /*frame_rendered_cb=*/[](int64_t) {},
        /*first_tunnel_frame_ready_cb=*/[]() {},
        /*tunnel_mode_audio_session_id=*/std::nullopt,
        /*enable_frame_renderer_listener=*/false,
        /*max_video_input_size=*/0, /*flush_delay_usec=*/0,
        /*use_dual_threads=*/true, /*skip_video_frames_over_60_fps=*/false,
        /*enable_ndk_video=*/false, /*enable_trivial_optimizations=*/false,
        enable_codec_thread_improvements());
    ASSERT_TRUE(result);
    decoder_ = std::move(result.value());
    codec_ = factory_.last_created_video_codec();
    ASSERT_NE(codec_, nullptr);
    decoder_->Initialize([this](SbPlayerError, const std::string&) {
      error_called_.store(true);
    });
  }

  void TearDown() override {
    decoder_.reset();
    EXPECT_FALSE(error_called_.load());
  }

  scoped_refptr<InputBuffer> CreateVideoInputBuffer(int64_t timestamp) {
    constexpr int kSize = 1024;
    uint8_t* buffer = new uint8_t[kSize];
    memset(buffer, 0, kSize);

    SbPlayerSampleInfo sample_info = {};
    sample_info.type = kSbMediaTypeVideo;
    sample_info.buffer = buffer;
    sample_info.buffer_size = kSize;
    sample_info.timestamp = timestamp;
    sample_info.video_sample_info.stream_info.codec = kSbMediaVideoCodecH264;
    sample_info.video_sample_info.stream_info.mime = "";
    sample_info.video_sample_info.stream_info.max_video_capabilities = "";
    sample_info.video_sample_info.stream_info.frame_width = 1920;
    sample_info.video_sample_info.stream_info.frame_height = 1080;

    auto deallocate_func = [](SbPlayer player, void* context,
                              const void* sample_buffer) {
      delete[] static_cast<const uint8_t*>(sample_buffer);
    };
    return make_scoped_refptr<InputBuffer>(deallocate_func, /*player=*/nullptr,
                                           /*context=*/nullptr, sample_info);
  }

  // Writes one input buffer, waits for the codec to receive it and returns it.
  std::vector<FakeMediaCodec::QueuedInput> WriteAndWaitForQueuedInput(
      int64_t timestamp) {
    decoder_->WriteInputBuffers({CreateVideoInputBuffer(timestamp)});
    EXPECT_TRUE(codec_->WaitForInputQueue(1, kMaxQueueLatencyMs));
    return codec_->ConsumeQueuedInputs();
  }

  int tick_count() const { return tick_count_.load(); }

  // Waits for a Tick() after |after_tick_count| ticks and returns the elapsed
  // time, or -1 on timeout.
  int64_t WaitForNextTickUs(int after_tick_count, int timeout_ms) {
    const int64_t start = CurrentMonotonicTime();
    std::unique_lock lock(tick_mutex_);
    const bool ticked = tick_condition_variable_.wait_for(
        lock, std::chrono::milliseconds(timeout_ms),
        [&] { return tick_count_.load() > after_tick_count; });
    return ticked ? CurrentMonotonicTime() - start : -1;
  }

  JobQueue job_queue_;
  FakeMediaCodecFactory factory_;
  NiceMock<MockHost> host_;
  std::unique_ptr<MediaCodecDecoder> decoder_;
  FakeMediaCodec* codec_ = nullptr;
  std::atomic_bool error_called_{false};
  std::atomic_int tick_count_{0};
  std::mutex tick_mutex_;
  std::condition_variable tick_condition_variable_;
};

TEST_P(MediaCodecDecoderVideoDualThreadsTest,
       QueuesInputWhenIndexArrivesFirst) {
  codec_->SimulateInputBufferAvailable(3);
  auto queued = WriteAndWaitForQueuedInput(/*timestamp=*/10000);
  ASSERT_EQ(queued.size(), 1U);
  EXPECT_EQ(queued[0].index, 3);
  EXPECT_EQ(queued[0].pts, 10000);
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest,
       QueuesInputWhenIndexArrivesLater) {
  decoder_->WriteInputBuffers({CreateVideoInputBuffer(10000)});
  // Make sure the input thread is waiting for an input buffer index before it
  // arrives, which is the ordering where a missing signal would stall.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(codec_->WaitForInputQueue(1, 0));

  codec_->SimulateInputBufferAvailable(0);
  ASSERT_TRUE(codec_->WaitForInputQueue(1, kMaxQueueLatencyMs));
  auto queued = codec_->ConsumeQueuedInputs();
  ASSERT_EQ(queued.size(), 1U);
  EXPECT_EQ(queued[0].pts, 10000);
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest,
       QueuesInputsInOrderWhenIndicesTrickleIn) {
  constexpr int kInputs = 32;
  InputBuffers input_buffers;
  for (int i = 0; i < kInputs; ++i) {
    input_buffers.push_back(CreateVideoInputBuffer(i * 1000));
  }
  decoder_->WriteInputBuffers(input_buffers);

  for (int i = 0; i < kInputs; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    codec_->SimulateInputBufferAvailable(i % 4);
    ASSERT_TRUE(codec_->WaitForInputQueue(i + 1, kMaxQueueLatencyMs))
        << "input " << i << " wasn't queued in time";
  }
  auto queued = codec_->ConsumeQueuedInputs();
  ASSERT_EQ(queued.size(), static_cast<size_t>(kInputs));
  for (int i = 0; i < kInputs; ++i) {
    EXPECT_EQ(queued[i].pts, i * 1000) << "at " << i;
    EXPECT_EQ(queued[i].index, i % 4) << "at " << i;
  }
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest, QueuesMultipleInputsPerIndex) {
  // Several indices available before several inputs are written at once.
  for (int i = 0; i < 4; ++i) {
    codec_->SimulateInputBufferAvailable(i);
  }
  InputBuffers input_buffers;
  for (int i = 0; i < 6; ++i) {
    input_buffers.push_back(CreateVideoInputBuffer(i * 1000));
  }
  decoder_->WriteInputBuffers(input_buffers);
  ASSERT_TRUE(codec_->WaitForInputQueue(4, kMaxQueueLatencyMs));
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  // The two unmatched inputs are queued as soon as indices arrive.
  codec_->SimulateInputBufferAvailable(0);
  codec_->SimulateInputBufferAvailable(1);
  ASSERT_TRUE(codec_->WaitForInputQueue(6, kMaxQueueLatencyMs));
  auto queued = codec_->ConsumeQueuedInputs();
  ASSERT_EQ(queued.size(), 6U);
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(queued[i].pts, i * 1000) << "at " << i;
  }
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest, WritesEndOfStream) {
  codec_->SimulateInputBufferAvailable(0);
  ASSERT_EQ(WriteAndWaitForQueuedInput(/*timestamp=*/0).size(), 1U);

  EXPECT_CALL(host_, OnEndOfStreamWritten(_)).Times(1);
  decoder_->WriteEndOfStream();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  codec_->SimulateInputBufferAvailable(1);
  ASSERT_TRUE(codec_->WaitForInputQueue(1, kMaxQueueLatencyMs));
  auto queued = codec_->ConsumeQueuedInputs();
  ASSERT_EQ(queued.size(), 1U);
  EXPECT_EQ(queued[0].index, 1);
  EXPECT_TRUE(queued[0].flags & MediaCodec::kBufferFlagEndOfStream);
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest, ResumesAfterFlush) {
  codec_->SimulateInputBufferAvailable(0);
  ASSERT_EQ(WriteAndWaitForQueuedInput(/*timestamp=*/0).size(), 1U);
  // Leave unmatched inputs behind, then flush.
  decoder_->WriteInputBuffers(
      {CreateVideoInputBuffer(1000), CreateVideoInputBuffer(2000)});
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  std::atomic_int refresh_output_format_count{0};
  ON_CALL(host_, RefreshOutputFormat(_)).WillByDefault(Invoke([&](MediaCodec*) {
    ++refresh_output_format_count;
  }));
  // Flush() and the destructor each notify the host.
  EXPECT_CALL(host_, OnFlushing()).Times(AtLeast(1));
  ASSERT_TRUE(decoder_->Flush());
  EXPECT_EQ(decoder_->GetNumberOfPendingInputs(), 0U);

  codec_->SimulateInputBufferAvailable(0);
  auto queued = WriteAndWaitForQueuedInput(/*timestamp=*/5000);
  ASSERT_EQ(queued.size(), 1U);
  EXPECT_EQ(queued[0].pts, 5000);
  // The output thread picks up the output format change queued by Flush().
  const int64_t deadline =
      CurrentMonotonicTime() + kMaxQueueLatencyMs * int64_t{1000};
  while (refresh_output_format_count.load() == 0 &&
         CurrentMonotonicTime() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_GE(refresh_output_format_count.load(), 1);
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest, ProcessesOutputBuffersAndTicks) {
  codec_->SimulateInputBufferAvailable(0);
  ASSERT_EQ(WriteAndWaitForQueuedInput(/*timestamp=*/0).size(), 1U);

  constexpr int kOutputs = 8;
  EXPECT_CALL(host_, ProcessOutputBuffer(_, _, _)).Times(kOutputs);
  for (int i = 0; i < kOutputs; ++i) {
    codec_->SimulateOutputAvailable(i, /*flags=*/0, /*offset=*/0,
                                    /*pts=*/i * 1000, /*size=*/100);
  }
  ASSERT_TRUE(codec_->WaitForOutputReleased(kOutputs, kMaxQueueLatencyMs));
  EXPECT_EQ(codec_->ConsumeReleasedOutputs().size(),
            static_cast<size_t>(kOutputs));
  // The renderer keeps getting a chance to run afterwards.
  EXPECT_GE(WaitForNextTickUs(tick_count(), kMaxQueueLatencyMs), 0);
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest, TickCadenceFollowsHost) {
  // Only the improved output thread honors the host's requested wait; the
  // legacy one polls every 8ms.
  constexpr int64_t kRequestedWaitUs = 40'000;
  ON_CALL(host_, GetNextTickWaitDurationUs())
      .WillByDefault(Return(kRequestedWaitUs));
  codec_->SimulateInputBufferAvailable(0);
  ASSERT_EQ(WriteAndWaitForQueuedInput(/*timestamp=*/0).size(), 1U);

  // Let the output thread settle into idle ticking, then measure.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  const int ticks_before = tick_count();
  constexpr int kMeasureMs = 400;
  std::this_thread::sleep_for(std::chrono::milliseconds(kMeasureMs));
  const int ticks = tick_count() - ticks_before;
  if (enable_codec_thread_improvements()) {
    // ~10 ticks at 40ms; allow for scheduling jitter.
    EXPECT_GE(ticks, 5);
    EXPECT_LE(ticks, 16);
  } else {
    // ~50 ticks at 8ms.
    EXPECT_GE(ticks, 25);
  }
}

TEST_P(MediaCodecDecoderVideoDualThreadsTest,
       SetPlaybackRateWakesOutputThreadWhenImproved) {
  if (!enable_codec_thread_improvements()) {
    GTEST_SKIP() << "The legacy output thread polls regardless.";
  }
  // Ask for long idle waits so that the wakeup is observable.
  ON_CALL(host_, GetNextTickWaitDurationUs()).WillByDefault(Return(500'000));
  codec_->SimulateInputBufferAvailable(0);
  ASSERT_EQ(WriteAndWaitForQueuedInput(/*timestamp=*/0).size(), 1U);
  // Wait for the output thread to enter the long wait.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  const int ticks_before = tick_count();
  decoder_->SetPlaybackRate(0.0);
  const int64_t elapsed_us = WaitForNextTickUs(ticks_before, 100);
  EXPECT_GE(elapsed_us, 0) << "SetPlaybackRate() didn't wake the output thread";
  EXPECT_LT(elapsed_us, 100'000);
}

INSTANTIATE_TEST_SUITE_P(,
                         MediaCodecDecoderVideoDualThreadsTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "CodecThreadImprovements"
                                             : "Legacy";
                         });

}  // namespace
}  // namespace starboard
