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

class MediaCodecVideoDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    video_stream_info_.codec = kSbMediaVideoCodecH264;
    video_stream_info_.frame_size = {1920, 1080};
    // Initialize color metadata to identity to avoid re-initialization on first
    // write
    video_stream_info_.color_metadata.primaries = kSbMediaPrimaryIdBt709;
    video_stream_info_.color_metadata.transfer = kSbMediaTransferIdBt709;
    video_stream_info_.color_metadata.matrix = kSbMediaMatrixIdBt709;
    video_stream_info_.color_metadata.range = kSbMediaRangeIdLimited;
  }

  void TearDown() override {
    decoder_.reset();
    fake_factory_ = nullptr;
  }

  void CreateDecoder() {
    auto factory = std::make_unique<FakeMediaCodecFactory>();
    fake_factory_ = factory.get();  // Save raw pointer before moving!

    MediaCodecVideoDecoder::StreamConfig stream_config{
        video_stream_info_,
        /*drm_system=*/kSbDrmSystemInvalid,
        kSbPlayerOutputModePunchOut,
        /*decode_target_graphics_context_provider=*/nullptr,
        /*surface_view=*/reinterpret_cast<void*>(1),
        /*max_video_capabilities=*/""};

    MediaCodecVideoDecoder::TunnelModeConfig tunnel_config;
    MediaCodecVideoDecoder::PipelineConfig pipeline_config;
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
        [](const VideoRendererSink::DrawFrameCB& draw_frame_cb) {});
  }

  scoped_refptr<InputBuffer> CreateDummyVideoInputBuffer(int64_t timestamp,
                                                         int size) {
    uint8_t* buffer = new uint8_t[size];
    memset(buffer, 0, size);

    struct TestSampleInfo {
      SbMediaType type = kSbMediaTypeVideo;
      const void* buffer;
      int buffer_size;
      int64_t timestamp;
      VideoSampleInfo video_sample_info;
      AudioSampleInfo audio_sample_info;
      const SbDrmSampleInfo* drm_info = nullptr;
      int side_data_count = 0;
      const SbPlayerSampleSideData* side_data = nullptr;
    } sample_info;

    sample_info.buffer = buffer;
    sample_info.buffer_size = size;
    sample_info.timestamp = timestamp;
    sample_info.video_sample_info.stream_info = video_stream_info_;

    auto deallocate_func = [](SbPlayer player, void* context,
                              const void* sample_buffer) {
      delete[] static_cast<const uint8_t*>(sample_buffer);
    };

    return new InputBuffer(deallocate_func, nullptr, nullptr, sample_info);
  }

  FakeMediaCodec* GetFakeVideoCodec() {
    return fake_factory_ ? fake_factory_->last_created_video_codec() : nullptr;
  }

  JobQueue job_queue_;
  FakeMediaCodecFactory* fake_factory_ = nullptr;
  VideoStreamInfo video_stream_info_;
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
          std::lock_guard<std::mutex> lock(output_mutex);
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

  auto queued_inputs = fake_codec->GetQueuedInputs();
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
  auto released_outputs = fake_codec->GetReleasedOutputs();
  ASSERT_EQ(released_outputs.size(), 1U);
  EXPECT_EQ(released_outputs[0], 0);

  EXPECT_FALSE(error_called);
}

}  // namespace
}  // namespace starboard
