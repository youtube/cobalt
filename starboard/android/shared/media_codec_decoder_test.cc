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

#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

#include "starboard/android/shared/media_codec.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

class FakeMediaCodec : public MediaCodec {
 public:
  FakeMediaCodec(Handler* handler) : handler_(handler) {
    EXPECT_TRUE(handler_);
    for (int i = 0; i < kNumBuffers; ++i) {
      buffers_[i].resize(kBufferSize);
    }
  }

  ~FakeMediaCodec() override = default;

  jni_zero::ScopedJavaLocalRef<jobject> GetInputBuffer(jint index) override {
    return {};
  }

  void* GetInputBufferAddress(jint index, size_t* capacity) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index < 0 || index >= kNumBuffers) {
      return nullptr;
    }
    *capacity = kBufferSize;
    return buffers_[index].data();
  }

  jint QueueInputBuffer(jint index,
                        jint offset,
                        jint size,
                        jlong presentation_time_microseconds,
                        jint flags,
                        jboolean is_decode_only) override {
    std::lock_guard<std::mutex> lock(mutex_);
    EXPECT_GE(index, 0);
    EXPECT_LT(index, kNumBuffers);

    QueuedInput input;
    input.index = index;
    input.offset = offset;
    input.size = size;
    input.pts = presentation_time_microseconds;
    input.flags = flags;
    queued_inputs_.push_back(input);

    cond_var_.notify_all();
    return 0;  // Success
  }

  jint QueueSecureInputBuffer(jint index,
                              jint offset,
                              const SbDrmSampleInfo& drm_sample_info,
                              jlong presentation_time_microseconds,
                              jboolean is_decode_only) override {
    return -1;  // Not supported in Fake
  }

  jni_zero::ScopedJavaLocalRef<jobject> GetOutputBuffer(jint index) override {
    return {};
  }

  void ReleaseOutputBuffer(jint index, jboolean render) override {
    std::lock_guard<std::mutex> lock(mutex_);
    released_outputs_.push_back(index);
    cond_var_.notify_all();
  }

  void ReleaseOutputBufferAtTimestamp(jint index,
                                      jlong render_timestamp_ns) override {
    ReleaseOutputBuffer(index, true);
  }

  void SetPlaybackRate(double playback_rate) override {}
  bool Restart() override { return true; }
  jint Flush() override {
    std::lock_guard<std::mutex> lock(mutex_);
    queued_inputs_.clear();
    released_outputs_.clear();
    return 0;
  }
  std::optional<FrameSize> GetOutputSize() override {
    return FrameSize(1920, 1080, false);
  }
  std::optional<AudioOutputFormatResult> GetAudioOutputFormat() override {
    return AudioOutputFormatResult{44100, 2};
  }

  // Test control methods
  void SimulateInputBufferAvailable(int index) {
    handler_->OnMediaCodecInputBufferAvailable(index);
  }

  void SimulateOutputAvailable(int index,
                               int flags,
                               int offset,
                               int64_t pts,
                               int size) {
    handler_->OnMediaCodecOutputBufferAvailable(index, flags, offset, pts,
                                                size);
  }

  bool WaitForInputQueue(size_t num_packets, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cond_var_.wait_for(
        lock, std::chrono::milliseconds(timeout_ms),
        [this, num_packets]() { return queued_inputs_.size() >= num_packets; });
  }

  bool WaitForOutputReleased(size_t num_packets, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cond_var_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                              [this, num_packets]() {
                                return released_outputs_.size() >= num_packets;
                              });
  }

  struct QueuedInput {
    int index;
    int offset;
    int size;
    int64_t pts;
    int flags;
  };

  std::vector<QueuedInput> GetQueuedInputs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<QueuedInput>(queued_inputs_.begin(),
                                    queued_inputs_.end());
  }

  std::vector<int> GetReleasedOutputs() {
    std::lock_guard<std::mutex> lock(mutex_);
    return released_outputs_;
  }

 private:
  static constexpr int kNumBuffers = 8;
  static constexpr size_t kBufferSize = 8192;

  Handler* const handler_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  std::vector<std::vector<uint8_t>> buffers_{kNumBuffers};
  std::deque<QueuedInput> queued_inputs_;
  std::vector<int> released_outputs_;
};

class FakeMediaCodecFactory : public MediaCodec::Factory {
 public:
  FakeMediaCodecFactory() = default;
  ~FakeMediaCodecFactory() override = default;

  std::unique_ptr<MediaCodec> CreateAudioMediaCodec(
      const AudioStreamInfo& audio_stream_info,
      MediaCodec::Handler* handler,
      jobject j_media_crypto) override {
    auto fake = std::make_unique<FakeMediaCodec>(handler);
    last_created_audio_codec_ = fake.get();
    return fake;
  }

  NonNullResult<std::unique_ptr<MediaCodec>> CreateVideoMediaCodec(
      SbMediaVideoCodec video_codec,
      const Size& frame_size_hint,
      int fps,
      const std::optional<Size>& max_frame_size,
      MediaCodec::Handler* handler,
      jobject j_surface,
      jobject j_media_crypto,
      const SbMediaColorMetadata* color_metadata,
      bool enable_frame_renderer_listener,
      bool require_secured_decoder,
      bool require_software_codec,
      std::optional<int> tunnel_mode_audio_session_id,
      bool force_big_endian_hdr_metadata,
      int max_video_input_size,
      bool skip_video_frames_over_60_fps) override {
    auto fake = std::make_unique<FakeMediaCodec>(handler);
    last_created_video_codec_ = fake.get();
    return std::move(fake);
  }

  FakeMediaCodec* last_created_audio_codec() const {
    return last_created_audio_codec_;
  }
  FakeMediaCodec* last_created_video_codec() const {
    return last_created_video_codec_;
  }

 private:
  FakeMediaCodec* last_created_audio_codec_ = nullptr;
  FakeMediaCodec* last_created_video_codec_ = nullptr;
};

class MockHost : public MediaCodecDecoder::Host {
 public:
  MOCK_METHOD(void,
              ProcessOutputBuffer,
              (MediaCodec * media_codec, const DequeueOutputResult& output),
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
  MOCK_METHOD(void, OnFlushing, (), (override));
  MOCK_METHOD(bool,
              IsBufferDecodeOnly,
              (const scoped_refptr<InputBuffer>& input_buffer),
              (override));
};

class MediaCodecDecoderTest : public ::testing::Test {
 protected:
  MediaCodecDecoderTest() {}

  void SetUp() override {
    audio_stream_info_.codec = kSbMediaAudioCodecAac;
    audio_stream_info_.samples_per_second = 44100;
    audio_stream_info_.number_of_channels = 2;
  }

  void TearDown() override { decoder_.reset(); }

  scoped_refptr<InputBuffer> CreateDummyAudioInputBuffer(int64_t timestamp,
                                                         int size) {
    uint8_t* buffer = new uint8_t[size];
    memset(buffer, 0, size);

    struct TestSampleInfo {
      SbMediaType type = kSbMediaTypeAudio;
      const void* buffer;
      int buffer_size;
      int64_t timestamp;
      AudioSampleInfo audio_sample_info;
      VideoSampleInfo video_sample_info;
      const SbDrmSampleInfo* drm_info = nullptr;
      int side_data_count = 0;
      const SbPlayerSampleSideData* side_data = nullptr;
    } sample_info;

    sample_info.buffer = buffer;
    sample_info.buffer_size = size;
    sample_info.timestamp = timestamp;
    sample_info.audio_sample_info.stream_info = audio_stream_info_;

    auto deallocate_func = [](SbPlayer player, void* context,
                              const void* sample_buffer) {
      delete[] static_cast<const uint8_t*>(sample_buffer);
    };

    return new InputBuffer(deallocate_func, nullptr, nullptr, sample_info);
  }

  FakeMediaCodec* GetFakeAudioCodec() {
    return factory_.last_created_audio_codec();
  }

  JobQueue job_queue_;
  FakeMediaCodecFactory factory_;
  NiceMock<MockHost> host_;
  AudioStreamInfo audio_stream_info_;
  std::unique_ptr<MediaCodecDecoder> decoder_;
};

TEST_F(MediaCodecDecoderTest, InitializesCorrectly) {
  auto result = MediaCodecDecoder::CreateForAudio(
      &job_queue_, &host_, audio_stream_info_, kSbDrmSystemInvalid, &factory_);
  ASSERT_TRUE(result);
  decoder_ = std::move(result.value());

  FakeMediaCodec* fake_codec = GetFakeAudioCodec();
  ASSERT_NE(fake_codec, nullptr);
}

TEST_F(MediaCodecDecoderTest, BasicDecodingFlow) {
  auto result = MediaCodecDecoder::CreateForAudio(
      &job_queue_, &host_, audio_stream_info_, kSbDrmSystemInvalid, &factory_);
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

  auto queued_inputs = fake_codec->GetQueuedInputs();
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

  EXPECT_CALL(host_, ProcessOutputBuffer(_, _))
      .WillOnce(
          Invoke([&](MediaCodec* codec, const DequeueOutputResult& output) {
            std::lock_guard<std::mutex> lock(output_mutex);
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
  auto released_outputs = fake_codec->GetReleasedOutputs();
  ASSERT_EQ(released_outputs.size(), 1U);
  EXPECT_EQ(released_outputs[0], 0);

  EXPECT_FALSE(error_called);
}

}  // namespace
}  // namespace starboard
