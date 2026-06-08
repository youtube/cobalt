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

#include "starboard/android/shared/fake_media_codec.h"
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

  EXPECT_CALL(host_, ProcessOutputBuffer(_, _))
      .WillOnce(
          Invoke([&](MediaCodec* codec, const DequeueOutputResult& output) {
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

}  // namespace
}  // namespace starboard
