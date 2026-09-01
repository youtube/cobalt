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

#include "starboard/android/shared/audio_renderer_passthrough.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "starboard/android/shared/audio_decoder_passthrough.h"
#include "starboard/android/shared/audio_track.h"
#include "starboard/android/shared/fake_audio_track.h"
#include "starboard/common/ref_counted.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

AudioStreamInfo CreateAudioStreamInfo(
    SbMediaAudioCodec codec = kSbMediaAudioCodecAc3,
    int channels = 6,
    int samples_per_second = 48000) {
  AudioStreamInfo info = {};
  info.codec = codec;
  info.number_of_channels = channels;
  info.samples_per_second = samples_per_second;
  return info;
}

std::vector<uint8_t> CreateAc3Frame(bool is_eac3,
                                    int numblkscod,
                                    int fscod = 0) {
  std::vector<uint8_t> buffer(64, 0);
  buffer[0] = 0x0B;
  buffer[1] = 0x77;
  if (is_eac3) {
    // E-AC-3: bitstream ID > 10 (e.g. 16: (16 << 3) = 0x80)
    buffer[5] = 0x80;
    buffer[4] = static_cast<uint8_t>(((fscod & 0x03) << 6) |
                                     ((numblkscod & 0x03) << 4));
  } else {
    // AC-3: bitstream ID <= 10 (e.g. 8: (8 << 3) = 0x40)
    buffer[5] = 0x40;
  }
  return buffer;
}

// Note: |frame_data| must outlive the returned InputBuffer because
// StubDeallocateSampleFunc does not take ownership or copy the buffer.
scoped_refptr<InputBuffer> CreateInputBuffer(
    const std::vector<uint8_t>& frame_data,
    int64_t timestamp) {
  SbPlayerSampleInfo sample_info = {};
  sample_info.type = kSbMediaTypeAudio;
  sample_info.buffer = frame_data.data();
  sample_info.buffer_size = static_cast<int>(frame_data.size());
  sample_info.timestamp = timestamp;
  CreateAudioStreamInfo().ConvertTo(&sample_info.audio_sample_info.stream_info);
  return make_scoped_refptr<InputBuffer>(StubDeallocateSampleFunc, nullptr,
                                         nullptr, sample_info);
}

class ThrottledAudioTrack : public FakeAudioTrack {
 public:
  ThrottledAudioTrack(int channels,
                      int sampling_frequency_hz,
                      SbMediaAudioSampleType sample_type,
                      size_t max_bytes_per_write)
      : FakeAudioTrack(channels, sampling_frequency_hz, sample_type),
        max_bytes_per_write_(max_bytes_per_write) {}

  int WriteSample(Span<const uint8_t> buffer, int64_t sync_time) override {
    size_t bytes_to_write = std::min(buffer.size(), max_bytes_per_write_);
    return FakeAudioTrack::WriteSample(MakeSpan(buffer.data(), bytes_to_write),
                                       sync_time);
  }

 private:
  const size_t max_bytes_per_write_;
};

TEST(AudioRendererPassthroughStaticTest, ParseAc3SyncframeAudioSampleCount) {
  // Too small buffer (< 6 bytes) returns default 1536 samples.
  uint8_t short_buffer[5] = {0x0B, 0x77, 0, 0, 0};
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                short_buffer, 5),
            1536);

  // AC-3 bitstream ID <= 10 (e.g. 8) returns 1536 samples.
  auto ac3_frame = CreateAc3Frame(/*is_eac3=*/false, /*numblkscod=*/0);
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                ac3_frame.data(), ac3_frame.size()),
            1536);

  // E-AC-3: numblkscod 0, 1, 2, 3 correspond to 1, 2, 3, 6 blocks of 256
  // samples.
  auto eac3_block1 = CreateAc3Frame(/*is_eac3=*/true, /*numblkscod=*/0);
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                eac3_block1.data(), eac3_block1.size()),
            256);

  auto eac3_block2 = CreateAc3Frame(/*is_eac3=*/true, /*numblkscod=*/1);
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                eac3_block2.data(), eac3_block2.size()),
            512);

  auto eac3_block3 = CreateAc3Frame(/*is_eac3=*/true, /*numblkscod=*/2);
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                eac3_block3.data(), eac3_block3.size()),
            768);

  auto eac3_block6 = CreateAc3Frame(/*is_eac3=*/true, /*numblkscod=*/3);
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                eac3_block6.data(), eac3_block6.size()),
            1536);

  // E-AC-3 with fscod == 3 returns 6 blocks (1536 samples).
  auto eac3_fscod3 = CreateAc3Frame(/*is_eac3=*/true, /*numblkscod=*/0,
                                    /*fscod=*/3);
  EXPECT_EQ(AudioRendererPassthrough::ParseAc3SyncframeAudioSampleCount(
                eac3_fscod3.data(), eac3_fscod3.size()),
            1536);
}

class AudioRendererPassthroughTest : public ::testing::Test {
 protected:
  void SetUp() override {
    audio_stream_info_ = CreateAudioStreamInfo();
    pending_audio_track_ = std::make_unique<FakeAudioTrack>(
        audio_stream_info_.number_of_channels,
        audio_stream_info_.samples_per_second,
        kSbMediaAudioSampleTypeInt16Deprecated);
    fake_audio_track_ = pending_audio_track_.get();

    auto decoder = std::make_unique<AudioDecoderPassthrough>(
        audio_stream_info_.samples_per_second);

    renderer_ = AudioRendererPassthrough::CreateForTesting(
        &job_queue_, audio_stream_info_, std::move(decoder),
        [this](SbMediaAudioCodingType, std::optional<SbMediaAudioSampleType>,
               int, int, int, std::optional<int>,
               bool) { return std::move(pending_audio_track_); });
    ASSERT_NE(renderer_, nullptr);

    renderer_->Initialize(
        std::bind(&AudioRendererPassthroughTest::OnError, this,
                  std::placeholders::_1, std::placeholders::_2),
        std::bind(&AudioRendererPassthroughTest::OnPrerolled, this),
        std::bind(&AudioRendererPassthroughTest::OnEnded, this));

    frame_ = CreateAc3Frame(/*is_eac3=*/false, /*numblkscod=*/0);
  }

  void OnError(SbPlayerError error, const std::string& error_message) {
    last_error_ = error;
    last_error_message_ = error_message;
    error_count_++;
  }

  void OnPrerolled() { prerolled_count_++; }

  void OnEnded() { ended_count_++; }

  bool WaitForCondition(const std::function<bool()>& condition,
                        int timeout_ms = 1000) {
    int elapsed_ms = 0;
    while (!condition() && elapsed_ms < timeout_ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      elapsed_ms += 10;
    }
    return condition();
  }

  JobQueue job_queue_;
  AudioStreamInfo audio_stream_info_;
  std::unique_ptr<FakeAudioTrack> pending_audio_track_;
  FakeAudioTrack* fake_audio_track_ = nullptr;
  std::unique_ptr<AudioRendererPassthrough> renderer_;
  std::vector<uint8_t> frame_;

  std::atomic<int> error_count_ = 0;
  std::atomic<SbPlayerError> last_error_ = kSbPlayerErrorDecode;
  std::string last_error_message_;
  std::atomic<int> prerolled_count_ = 0;
  std::atomic<int> ended_count_ = 0;
};

TEST_F(AudioRendererPassthroughTest, InitialState) {
  EXPECT_FALSE(renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(renderer_->CanAcceptMoreData());
  EXPECT_EQ(renderer_->GetAudioWriteHead(), 0);
  EXPECT_EQ(renderer_->AdjustTimestampToAudioClock(12345), 12345);

  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = 0.0;
  int64_t media_time = renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

  EXPECT_EQ(media_time, 0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_FALSE(is_underflow);
  EXPECT_DOUBLE_EQ(playback_rate, 1.0);
}

TEST_F(AudioRendererPassthroughTest, AudioTrackCreationFailure) {
  auto decoder = std::make_unique<AudioDecoderPassthrough>(
      audio_stream_info_.samples_per_second);

  auto failing_factory = [](SbMediaAudioCodingType,
                            std::optional<SbMediaAudioSampleType>, int, int,
                            int, std::optional<int>, bool) {
    return std::unique_ptr<AudioTrack>(nullptr);
  };

  auto renderer = AudioRendererPassthrough::CreateForTesting(
      &job_queue_, audio_stream_info_, std::move(decoder), failing_factory);
  ASSERT_NE(renderer, nullptr);

  std::atomic<int> error_received = 0;
  std::atomic<SbPlayerError> received_code = kSbPlayerErrorCapabilityChanged;
  renderer->Initialize(
      [&](SbPlayerError error, const std::string&) {
        error_received++;
        received_code = error;
      },
      [] {}, [] {});

  renderer->WriteSamples({CreateInputBuffer(frame_, 0)});
  ASSERT_TRUE(WaitForCondition([&] { return error_received > 0; }));

  EXPECT_EQ(received_code, kSbPlayerErrorDecode);
}

TEST_F(AudioRendererPassthroughTest, WriteSamplesAndConsume) {
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});

  ASSERT_TRUE(WaitForCondition(
      [&] { return fake_audio_track_->written_frames() > 0; }));

  EXPECT_TRUE(WaitForCondition([&] { return renderer_->CanAcceptMoreData(); }));
  EXPECT_EQ(error_count_, 0);
}

TEST_F(AudioRendererPassthroughTest, PrerollOnPartialWrite) {
  // Create a separate renderer with a ThrottledAudioTrack that only accepts up
  // to 16 bytes per write. The frame size is 64 bytes, so it cannot be written
  // in a single pass. A partial write triggers the prerolled callback.
  auto decoder = std::make_unique<AudioDecoderPassthrough>(
      audio_stream_info_.samples_per_second);

  auto renderer = AudioRendererPassthrough::CreateForTesting(
      &job_queue_, audio_stream_info_, std::move(decoder),
      [](SbMediaAudioCodingType, std::optional<SbMediaAudioSampleType>,
         int channels, int sample_rate, int, std::optional<int>, bool) {
        return std::make_unique<ThrottledAudioTrack>(
            channels, sample_rate, kSbMediaAudioSampleTypeInt16Deprecated,
            /*max_bytes_per_write=*/16);
      });
  ASSERT_NE(renderer, nullptr);

  std::atomic<int> prerolled = 0;
  renderer->Initialize([](SbPlayerError, const std::string&) {},
                       [&] { prerolled++; }, [] {});

  renderer->WriteSamples({CreateInputBuffer(frame_, 0)});

  EXPECT_TRUE(WaitForCondition([&] { return prerolled > 0; }));
}

TEST_F(AudioRendererPassthroughTest, PrerollOnEos) {
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  renderer_->WriteEndOfStream();

  EXPECT_TRUE(WaitForCondition([&] { return prerolled_count_ > 0; }));
}

TEST_F(AudioRendererPassthroughTest, PlayAndPause) {
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  renderer_->Play();
  ASSERT_TRUE(WaitForCondition([&] {
    return fake_audio_track_->play_state() == AudioTrack::PlayState::kPlaying;
  }));

  bool is_playing = false;
  bool is_eos_played = false;
  bool is_underflow = false;
  double playback_rate = 0.0;
  renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played, &is_underflow,
                                 &playback_rate);
  EXPECT_TRUE(is_playing);

  renderer_->Pause();
  ASSERT_TRUE(WaitForCondition([&] {
    return fake_audio_track_->play_state() == AudioTrack::PlayState::kPaused;
  }));

  renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played, &is_underflow,
                                 &playback_rate);
  EXPECT_FALSE(is_playing);
}

TEST_F(AudioRendererPassthroughTest, SetVolume) {
  renderer_->SetVolume(0.5);

  // Write a sample to trigger the processing thread update.
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});

  EXPECT_TRUE(
      WaitForCondition([&] { return fake_audio_track_->volume() == 0.5; }));
}

TEST_F(AudioRendererPassthroughTest, SetPlaybackRate) {
  renderer_->SetPlaybackRate(0.0);
  bool is_playing = false;
  bool is_eos_played = false;
  bool is_underflow = false;
  double playback_rate = -1.0;
  renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played, &is_underflow,
                                 &playback_rate);
  EXPECT_DOUBLE_EQ(playback_rate, 0.0);

  // Unsupported rates (> 0 and != 1.0) are clamped to 1.0.
  renderer_->SetPlaybackRate(1.5);
  renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played, &is_underflow,
                                 &playback_rate);
  EXPECT_DOUBLE_EQ(playback_rate, 1.0);
}

TEST_F(AudioRendererPassthroughTest, EndOfStreamWithoutSamples) {
  renderer_->WriteEndOfStream();

  EXPECT_TRUE(renderer_->IsEndOfStreamWritten());
  EXPECT_TRUE(renderer_->IsEndOfStreamPlayed());
  EXPECT_EQ(ended_count_, 1);
}

TEST_F(AudioRendererPassthroughTest, EndOfStreamPlaybackDrain) {
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  renderer_->Play();

  ASSERT_TRUE(WaitForCondition(
      [&] { return fake_audio_track_->written_frames() > 0; }));

  renderer_->WriteEndOfStream();
  ASSERT_TRUE(renderer_->IsEndOfStreamWritten());
  ASSERT_TRUE(WaitForCondition([&] { return ended_count_ > 0; }, 2000));

  EXPECT_TRUE(renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererPassthroughTest, DuplicateEndOfStream) {
  renderer_->WriteEndOfStream();
  renderer_->WriteEndOfStream();

  EXPECT_EQ(ended_count_, 1);
}

TEST_F(AudioRendererPassthroughTest, SeekAndReuseAudioTrack) {
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  renderer_->Play();
  ASSERT_TRUE(WaitForCondition([&] {
    return fake_audio_track_->play_state() == AudioTrack::PlayState::kPlaying;
  }));

  const int64_t kSeekTime = 500'000;
  renderer_->Seek(kSeekTime);

  EXPECT_FALSE(renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(renderer_->CanAcceptMoreData());
  EXPECT_EQ(fake_audio_track_->play_state(), AudioTrack::PlayState::kPaused);

  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = false;
  double playback_rate = 0.0;
  int64_t media_time = renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

  EXPECT_EQ(media_time, kSeekTime);
  EXPECT_FALSE(is_playing);

  // Resume playback by writing new samples. The existing FakeAudioTrack should
  // be reused.
  renderer_->WriteSamples({CreateInputBuffer(frame_, kSeekTime)});
  renderer_->Play();

  EXPECT_TRUE(WaitForCondition([&] {
    return fake_audio_track_->play_state() == AudioTrack::PlayState::kPlaying;
  }));
}

TEST_F(AudioRendererPassthroughTest, AudioDeviceCapabilityChanged) {
  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  ASSERT_TRUE(WaitForCondition(
      [&] { return fake_audio_track_->written_frames() > 0; }));

  fake_audio_track_->simulate_device_change(true);
  ASSERT_TRUE(WaitForCondition([&] { return error_count_ > 0; }));

  EXPECT_EQ(last_error_, kSbPlayerErrorCapabilityChanged);
}

TEST_F(AudioRendererPassthroughTest, DeadObjectError) {
  fake_audio_track_->set_write_error(AudioTrack::kAudioTrackErrorDeadObject);

  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  ASSERT_TRUE(WaitForCondition([&] { return error_count_ > 0; }));

  EXPECT_EQ(last_error_, kSbPlayerErrorCapabilityChanged);
}

TEST_F(AudioRendererPassthroughTest, GeneralWriteError) {
  fake_audio_track_->set_write_error(-1);

  renderer_->WriteSamples({CreateInputBuffer(frame_, 0)});
  ASSERT_TRUE(WaitForCondition([&] { return error_count_ > 0; }));

  EXPECT_EQ(last_error_, kSbPlayerErrorDecode);
}

TEST_F(AudioRendererPassthroughTest, GetCurrentMediaTimePausedAndPlaying) {
  const int64_t kSampleTimestamp = 100'000;
  renderer_->WriteSamples({CreateInputBuffer(frame_, kSampleTimestamp)});
  ASSERT_TRUE(WaitForCondition(
      [&] { return fake_audio_track_->written_frames() > 0; }));

  // Simulate 4800 frames consumed (100ms at 48000 Hz).
  fake_audio_track_->set_consumed_frames(4800);

  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = 0.0;

  // When paused, monotonic clock interpolation is not applied.
  // Playback time = audio_start_time (100,000) + 4800 * 1,000,000 / 48000
  // (100,000) = 200,000 us.
  int64_t media_time = renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_EQ(media_time, 200'000);
  EXPECT_FALSE(is_playing);
}

}  // namespace
}  // namespace starboard
