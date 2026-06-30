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

#include "starboard/android/shared/audio_track_audio_sink_type.h"

#include <unistd.h>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "starboard/android/shared/fake_audio_track.h"
#include "starboard/common/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

class AudioTrackAudioSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    frame_buffer_.resize(1024 * 2 * sizeof(float), 0);
    frame_buffers_[0] = frame_buffer_.data();
  }

  static void UpdateSourceStatusCB(int* frames_in_buffer,
                                   int* offset_in_frames,
                                   bool* is_playing,
                                   bool* is_eos_reached,
                                   void* context) {
    auto* fixture = static_cast<AudioTrackAudioSinkTest*>(context);
    *frames_in_buffer = fixture->frames_in_buffer_;
    *offset_in_frames = fixture->offset_in_frames_;
    *is_playing = fixture->is_playing_;
    *is_eos_reached = fixture->is_eos_reached_;
  }

  static void ConsumeFramesCB(int frames_consumed,
                              int64_t frames_consumed_at,
                              void* context) {
    auto* fixture = static_cast<AudioTrackAudioSinkTest*>(context);
    fixture->total_frames_consumed_ += frames_consumed;
  }

  std::vector<uint8_t> frame_buffer_;
  void* frame_buffers_[1];
  std::atomic<int> frames_in_buffer_ = 512;
  std::atomic<int> offset_in_frames_ = 0;
  std::atomic<bool> is_playing_ = true;
  std::atomic<bool> is_eos_reached_ = false;
  std::atomic<int> total_frames_consumed_ = 0;
};

TEST_F(AudioTrackAudioSinkTest, CreateAndDestroy) {
  AudioTrackAudioSinkType type;
  auto fake_track = std::make_unique<FakeAudioTrack>(
      2, 48000, kSbMediaAudioSampleTypeFloat32);
  FakeAudioTrack* track_ptr = fake_track.get();

  AudioTrackAudioSinkType::Callbacks callbacks{
      UpdateSourceStatusCB,
      ConsumeFramesCB,
      /*error=*/nullptr,
  };

  auto sink = AudioTrackAudioSink::CreateForTesting(
      &type, 2, 48000, kSbMediaAudioSampleTypeFloat32, frame_buffers_, 1024,
      512, callbacks, 0,
      /*tunnel_mode_audio_session_id=*/std::nullopt,
      /*allow_audio_writing_on_pause=*/false, std::move(fake_track), this);

  ASSERT_NE(sink, nullptr);
  EXPECT_TRUE(sink->IsType(&type));

  // Let the sink thread run and process frames.
  int elapsed_ms = 0;
  while (track_ptr->written_frames() == 0 && elapsed_ms < 1000) {
    usleep(10'000);
    elapsed_ms += 10;
  }
  EXPECT_GT(track_ptr->written_frames(), 0);

  sink.reset();
}

TEST_F(AudioTrackAudioSinkTest, PauseAndResumePlayback) {
  AudioTrackAudioSinkType type;
  auto fake_track = std::make_unique<FakeAudioTrack>(
      2, 48000, kSbMediaAudioSampleTypeFloat32);
  FakeAudioTrack* track_ptr = fake_track.get();

  AudioTrackAudioSinkType::Callbacks callbacks{
      UpdateSourceStatusCB,
      ConsumeFramesCB,
      /*error=*/nullptr,
  };

  auto sink = AudioTrackAudioSink::CreateForTesting(
      &type, 2, 48000, kSbMediaAudioSampleTypeFloat32, frame_buffers_, 1024,
      512, callbacks, 0,
      /*tunnel_mode_audio_session_id=*/std::nullopt,
      /*allow_audio_writing_on_pause=*/false, std::move(fake_track), this);

  ASSERT_NE(sink, nullptr);
  int elapsed_ms = 0;
  while (track_ptr->play_state() != AudioTrack::PlayState::kPlaying &&
         elapsed_ms < 1000) {
    usleep(10'000);
    elapsed_ms += 10;
  }
  EXPECT_EQ(track_ptr->play_state(), AudioTrack::PlayState::kPlaying);

  is_playing_ = false;
  elapsed_ms = 0;
  while (track_ptr->play_state() != AudioTrack::PlayState::kPaused &&
         elapsed_ms < 1000) {
    usleep(10'000);
    elapsed_ms += 10;
  }
  EXPECT_EQ(track_ptr->play_state(), AudioTrack::PlayState::kPaused);

  is_playing_ = true;
  elapsed_ms = 0;
  while (track_ptr->play_state() != AudioTrack::PlayState::kPlaying &&
         elapsed_ms < 1000) {
    usleep(10'000);
    elapsed_ms += 10;
  }
  EXPECT_EQ(track_ptr->play_state(), AudioTrack::PlayState::kPlaying);
}

}  // namespace
}  // namespace starboard
