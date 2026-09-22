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

#include <vector>

#include "starboard/android/shared/audio_sink_android.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

class FakeAudioSinkAndroid : public AudioSinkAndroid {
 public:
  FakeAudioSinkAndroid(SbAudioSinkPrivate::Type* type,
                       bool* destroyed,
                       bool* flushed = nullptr,
                       bool flush_succeeds = true)
      : type_(type),
        destroyed_(destroyed),
        flushed_(flushed),
        flush_succeeds_(flush_succeeds) {}

  ~FakeAudioSinkAndroid() override {
    if (destroyed_) {
      *destroyed_ = true;
    }
  }

  bool IsType(SbAudioSinkPrivate::Type* type) override { return type == type_; }

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
    if (flushed_) {
      *flushed_ = true;
    }
    return flush_succeeds_;
  }

  int GetUnderrunCount() override { return underrun_count_; }
  int GetStartThresholdInFrames() override { return 1024; }

  SbAudioSinkPrivate::Type* type_;
  bool* destroyed_ = nullptr;
  bool* flushed_ = nullptr;
  bool flush_succeeds_ = true;
  bool flush_called_ = false;
  bool start_time_set_ = false;
  int64_t last_start_time_us_ = -1;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
  int underrun_count_ = 0;
};

class FakeAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  FakeAudioSinkType() {
    old_primary_type_ = SbAudioSinkImpl::GetPrimaryType();
    SbAudioSinkImpl::SetPrimaryType(this);
  }

  ~FakeAudioSinkType() override {
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
    auto sink = new FakeAudioSinkAndroid(this, destroyed_ptr_, flushed_ptr_,
                                         flush_succeeds_);
    last_created_sink_ = sink;
    ++create_count_;
    return sink;
  }

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) override { delete audio_sink; }

  SbAudioSinkPrivate::Type* old_primary_type_ = nullptr;
  FakeAudioSinkAndroid* last_created_sink_ = nullptr;
  bool* destroyed_ptr_ = nullptr;
  bool* flushed_ptr_ = nullptr;
  bool flush_succeeds_ = true;
  int create_count_ = 0;
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
    fake_sink_type_->destroyed_ptr_ = &sink_destroyed_;
    fake_sink_type_->flushed_ptr_ = &sink_flushed_;
    dummy_buffer_.resize(1024 * sizeof(int16_t) * 2, 0);
    frame_buffers_[0] = dummy_buffer_.data();
  }

  void TearDown() override { fake_sink_type_.reset(); }

  std::unique_ptr<AudioRendererSinkAndroid> CreateSink(
      bool allow_flush_during_seek) {
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
  bool sink_destroyed_ = false;
  bool sink_flushed_ = false;
};

TEST_F(AudioRendererSinkAndroidTest, SeekWithFlushAllowedFlushesAndReusesSink) {
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/true);

  // 1. Initial Start
  renderer_sink->Start(
      /*media_start_time=*/0, /*channels=*/2, /*sampling_frequency_hz=*/48000,
      kSbMediaAudioSampleTypeInt16Deprecated, frame_buffers_, 1024,
      &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);
  FakeAudioSinkAndroid* sink1 = fake_sink_type_->last_created_sink_;
  ASSERT_NE(sink1, nullptr);

  // 2. Seek: Reset() should flush instead of destroy
  renderer_sink->Reset();
  EXPECT_TRUE(sink_flushed_);
  EXPECT_FALSE(sink_destroyed_);
  // When flushed, HasStarted() returns false so caller knows it needs Start()
  EXPECT_FALSE(renderer_sink->HasStarted());

  // 3. Resume after Seek: Start() with same format reuses existing sink
  renderer_sink->Start(
      /*media_start_time=*/5000000, /*channels=*/2,
      /*sampling_frequency_hz=*/48000, kSbMediaAudioSampleTypeInt16Deprecated,
      frame_buffers_, 1024, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);  // Reused! No new create.
  EXPECT_TRUE(sink1->start_time_set_);
  EXPECT_EQ(sink1->last_start_time_us_, 5000000);
}

TEST_F(AudioRendererSinkAndroidTest, SeekWithFlushDisallowedDestroysSink) {
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/false);

  // 1. Initial Start
  renderer_sink->Start(
      /*media_start_time=*/0, /*channels=*/2, /*sampling_frequency_hz=*/48000,
      kSbMediaAudioSampleTypeInt16Deprecated, frame_buffers_, 1024,
      &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);
  FakeAudioSinkAndroid* sink1 = fake_sink_type_->last_created_sink_;
  ASSERT_NE(sink1, nullptr);

  // 2. Seek: Reset() without flush should destroy the underlying sink
  renderer_sink->Reset();
  EXPECT_FALSE(sink_flushed_);
  EXPECT_TRUE(sink_destroyed_);
  EXPECT_FALSE(renderer_sink->HasStarted());

  // 3. Next Start creates a fresh sink
  sink_destroyed_ = false;
  sink_flushed_ = false;
  renderer_sink->Start(
      /*media_start_time=*/5000000, /*channels=*/2,
      /*sampling_frequency_hz=*/48000, kSbMediaAudioSampleTypeInt16Deprecated,
      frame_buffers_, 1024, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 2);
}

TEST_F(AudioRendererSinkAndroidTest, FlushFailureFallsBackToFullReset) {
  fake_sink_type_->flush_succeeds_ = false;
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/true);

  // 1. Initial Start
  renderer_sink->Start(
      /*media_start_time=*/0, /*channels=*/2, /*sampling_frequency_hz=*/48000,
      kSbMediaAudioSampleTypeInt16Deprecated, frame_buffers_, 1024,
      &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 1);

  // 2. Seek: Reset() tries flush, flush returns false -> falls back to Reset()
  renderer_sink->Reset();
  EXPECT_TRUE(sink_destroyed_);
  EXPECT_FALSE(renderer_sink->HasStarted());

  // 3. Next Start recreates the sink
  sink_destroyed_ = false;
  sink_flushed_ = false;
  renderer_sink->Start(
      /*media_start_time=*/5000000, /*channels=*/2,
      /*sampling_frequency_hz=*/48000, kSbMediaAudioSampleTypeInt16Deprecated,
      frame_buffers_, 1024, &dummy_callback_);
  EXPECT_TRUE(renderer_sink->HasStarted());
  EXPECT_EQ(fake_sink_type_->create_count_, 2);
}

TEST_F(AudioRendererSinkAndroidTest,
       FormatChangeDuringFlushedStateRecreatesSink) {
  auto renderer_sink = CreateSink(/*allow_flush_during_seek=*/true);

  // 1. Initial Start with 2 channels, 48000Hz
  renderer_sink->Start(
      /*media_start_time=*/0, /*channels=*/2, /*sampling_frequency_hz=*/48000,
      kSbMediaAudioSampleTypeInt16Deprecated, frame_buffers_, 1024,
      &dummy_callback_);
  EXPECT_EQ(fake_sink_type_->create_count_, 1);

  // 2. Seek flushes the sink
  renderer_sink->Reset();
  EXPECT_TRUE(sink_flushed_);
  EXPECT_FALSE(sink_destroyed_);

  // 3. Audio format changes (e.g. 6 channels / 5.1 surround instead of stereo)
  renderer_sink->Start(
      /*media_start_time=*/5000000, /*channels=*/6,
      /*sampling_frequency_hz=*/48000, kSbMediaAudioSampleTypeInt16Deprecated,
      frame_buffers_, 1024, &dummy_callback_);
  // Re-creates sink with new configuration because channel count changed
  EXPECT_EQ(fake_sink_type_->create_count_, 2);
  EXPECT_TRUE(sink_destroyed_);
  EXPECT_TRUE(renderer_sink->HasStarted());
}

}  // namespace
}  // namespace starboard
