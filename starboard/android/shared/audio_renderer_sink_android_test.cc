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

#include "starboard/android/shared/audio_renderer_sink_android.h"

#include <memory>
#include <vector>

#include "starboard/android/shared/audio_sink_android.h"
#include "starboard/common/check_op.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kChannels = 2;
constexpr int kSamplingFrequencyHz = 48'000;
constexpr int kFramesPerChannel = 1'024;
constexpr int64_t kInitialMediaStartTimeUs = 0;
constexpr int64_t kSeekMediaStartTimeUs = 5'000'000;
constexpr SbMediaAudioSampleType kSampleType =
    kSbMediaAudioSampleTypeInt16Deprecated;

class FakeAudioSinkAndroid : public AudioSinkAndroid {
 public:
  FakeAudioSinkAndroid(bool flush_succeeds,
                       std::function<void(FakeAudioSinkAndroid*)> on_destroy)
      : flush_succeeds_(flush_succeeds), on_destroy_(std::move(on_destroy)) {}

  ~FakeAudioSinkAndroid() override {
    if (on_destroy_) {
      on_destroy_(this);
    }
  }

  void SetPlaybackRate(double playback_rate) override {
    playback_rate_ = playback_rate;
  }

  void SetVolume(double volume) override { volume_ = volume; }

  void SetStartTime(int64_t start_time_us) override {
    start_time_set_ = true;
    last_start_time_us_ = start_time_us;
  }

  bool Flush() override {
    flush_called_ = true;
    return flush_succeeds_;
  }

  bool flush_called() const { return flush_called_; }
  bool start_time_set() const { return start_time_set_; }
  int64_t last_start_time_us() const { return last_start_time_us_; }
  double playback_rate() const { return playback_rate_; }
  double volume() const { return volume_; }

 private:
  bool flush_succeeds_ = true;
  std::function<void(FakeAudioSinkAndroid*)> on_destroy_;
  bool flush_called_ = false;
  bool start_time_set_ = false;
  int64_t last_start_time_us_ = -1;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
};

class FakeAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  FakeAudioSinkType() {
    old_primary_type_ = SbAudioSinkImpl::GetPrimaryType();
    SbAudioSinkImpl::SetPrimaryType(this);
  }

  ~FakeAudioSinkType() override {
    SB_CHECK(SbAudioSinkImpl::GetPrimaryType() == this);
    SbAudioSinkImpl::SetPrimaryType(old_primary_type_);
  }

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frame_buffers_size_in_frames,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      void* context) override {
    auto sink = new FakeAudioSinkAndroid(
        flush_succeeds_, [this](FakeAudioSinkAndroid* destroyed_sink) {
          if (destroyed_sink == last_created_sink_) {
            last_sink_destroyed_ = true;
          }
          ++destroy_count_;
        });
    last_created_sink_ = sink;
    last_sink_destroyed_ = false;
    ++create_count_;
    return sink;
  }

  SbAudioSinkPrivate::Type* old_primary_type_ = nullptr;
  FakeAudioSinkAndroid* last_created_sink_ = nullptr;
  bool flush_succeeds_ = true;
  bool last_sink_destroyed_ = false;
  int create_count_ = 0;
  int destroy_count_ = 0;
};

class DummyRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  void GetSourceStatus(int* frames_in_buffer,
                       int* offset_in_frames,
                       bool* is_playing,
                       bool* is_eos_reached) override {
    *frames_in_buffer = 0;
    *offset_in_frames = 0;
    *is_playing = false;
    *is_eos_reached = false;
  }
  void ConsumeFrames(int frames_consumed, int64_t frames_consumed_at) override {
  }
  void OnError(bool capability_changed,
               const std::string& error_message) override {}
};

class AudioRendererSinkAndroidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake_sink_type_ = std::make_unique<FakeAudioSinkType>();
    dummy_buffer_.resize(kFramesPerChannel * sizeof(int16_t) * kChannels, 0);
    frame_buffers_[0] = dummy_buffer_.data();
  }

  void TearDown() override { fake_sink_type_.reset(); }

  std::unique_ptr<AudioRendererSink> CreateSink(bool allow_flush_during_seek) {
    return std::make_unique<AudioRendererSinkAndroid>(
        /*tunnel_mode_audio_session_id=*/std::nullopt,
        /*allow_audio_writing_on_pause=*/false,
        /*enable_video_renderer_vsp_adjustment=*/false, allow_flush_during_seek,
        /*pause_using_audio_track_state=*/false,
        [this](int64_t start_media_time, int channels,
               int sampling_frequency_hz,
               SbMediaAudioSampleType audio_sample_type,
               SbAudioSinkFrameBuffers frame_buffers,
               int frame_buffers_size_in_frames,
               SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
               SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
               SbAudioSinkPrivate::ErrorFunc error_func, void* context) {
          return fake_sink_type_->Create(
              channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
              frame_buffers_size_in_frames, update_source_status_func,
              consume_frames_func, error_func, context);
        });
  }

  std::unique_ptr<FakeAudioSinkType> fake_sink_type_;
  DummyRenderCallback dummy_callback_;
  std::vector<uint8_t> dummy_buffer_;
  void* frame_buffers_[1];
};

TEST_F(AudioRendererSinkAndroidTest, SeekWithFlushAllowedFlushesAndReusesSink) {
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/true);

  // 1. Initial Start
  renderer_sink->Start(kInitialMediaStartTimeUs, kChannels,
                       kSamplingFrequencyHz, kSampleType, frame_buffers_,
                       kFramesPerChannel, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);
  FakeAudioSinkAndroid* sink1 = fake_sink_type_->last_created_sink_;
  ASSERT_NE(sink1, nullptr);

  // 2. Seek: Reset() should flush instead of destroy
  renderer_sink->Reset();
  EXPECT_TRUE(sink1->flush_called());
  EXPECT_FALSE(fake_sink_type_->last_sink_destroyed_);
  // When flushed, HasStarted() returns false so caller knows it needs Start()
  EXPECT_FALSE(renderer_sink->HasStarted());

  // Updating playback rate and volume while flushed should be applied on next
  // Start().
  renderer_sink->SetPlaybackRate(1.5);
  renderer_sink->SetVolume(0.5);

  // 3. Resume after Seek: Start() with same format reuses existing sink
  renderer_sink->Start(kSeekMediaStartTimeUs, kChannels, kSamplingFrequencyHz,
                       kSampleType, frame_buffers_, kFramesPerChannel,
                       &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);  // Reused! No new create.
  EXPECT_TRUE(sink1->start_time_set());
  EXPECT_EQ(sink1->last_start_time_us(), kSeekMediaStartTimeUs);
  EXPECT_DOUBLE_EQ(sink1->playback_rate(), 1.5);
  EXPECT_DOUBLE_EQ(sink1->volume(), 0.5);
}

TEST_F(AudioRendererSinkAndroidTest, SeekWithFlushDisallowedDestroysSink) {
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/false);

  // 1. Initial Start
  renderer_sink->Start(kInitialMediaStartTimeUs, kChannels,
                       kSamplingFrequencyHz, kSampleType, frame_buffers_,
                       kFramesPerChannel, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);

  // 2. Seek: Reset() without flush should destroy the underlying sink
  renderer_sink->Reset();
  EXPECT_TRUE(fake_sink_type_->last_sink_destroyed_);
  EXPECT_FALSE(renderer_sink->HasStarted());

  // 3. Next Start creates a fresh sink
  renderer_sink->Start(kSeekMediaStartTimeUs, kChannels, kSamplingFrequencyHz,
                       kSampleType, frame_buffers_, kFramesPerChannel,
                       &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 2);
}

TEST_F(AudioRendererSinkAndroidTest, FlushFailureFallsBackToFullReset) {
  fake_sink_type_->flush_succeeds_ = false;
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/true);

  // 1. Initial Start
  renderer_sink->Start(kInitialMediaStartTimeUs, kChannels,
                       kSamplingFrequencyHz, kSampleType, frame_buffers_,
                       kFramesPerChannel, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);

  // 2. Seek: Reset() tries flush, flush returns false -> falls back to Reset()
  renderer_sink->Reset();
  EXPECT_TRUE(fake_sink_type_->last_sink_destroyed_);
  EXPECT_FALSE(renderer_sink->HasStarted());

  // 3. Next Start recreates the sink
  renderer_sink->Start(kSeekMediaStartTimeUs, kChannels, kSamplingFrequencyHz,
                       kSampleType, frame_buffers_, kFramesPerChannel,
                       &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 2);
}

TEST_F(AudioRendererSinkAndroidTest,
       FormatChangeDuringFlushedStateRecreatesSink) {
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/true);

  // 1. Initial Start with 2 channels, 48000Hz
  renderer_sink->Start(kInitialMediaStartTimeUs, kChannels,
                       kSamplingFrequencyHz, kSampleType, frame_buffers_,
                       kFramesPerChannel, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);

  // 2. Seek flushes the sink
  renderer_sink->Reset();
  EXPECT_FALSE(fake_sink_type_->last_sink_destroyed_);
  EXPECT_EQ(fake_sink_type_->destroy_count_, 0);
  EXPECT_FALSE(renderer_sink->HasStarted());

  // 3. Audio format changes (e.g. 6 channels / 5.1 surround instead of stereo)
  constexpr int kNewChannels = 6;
  std::vector<uint8_t> new_buffer(
      kFramesPerChannel * sizeof(int16_t) * kNewChannels, 0);
  void* new_frame_buffers[1] = {new_buffer.data()};
  renderer_sink->Start(kSeekMediaStartTimeUs, kNewChannels,
                       kSamplingFrequencyHz, kSampleType, new_frame_buffers,
                       kFramesPerChannel, &dummy_callback_);
  // Destroys old flushed sink and re-creates sink with new configuration
  EXPECT_EQ(fake_sink_type_->destroy_count_, 1);
  EXPECT_EQ(fake_sink_type_->create_count_, 2);
  EXPECT_FALSE(fake_sink_type_->last_sink_destroyed_);
  EXPECT_TRUE(renderer_sink->HasStarted());
}

}  // namespace
}  // namespace starboard
