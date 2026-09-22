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
#include <optional>
#include <vector>

#include "starboard/android/shared/android_audio_sink.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

class FakeAudioSinkType;

class FakeAndroidAudioSink : public AndroidAudioSink {
 public:
  explicit FakeAndroidAudioSink(FakeAudioSinkType* type, bool* destroyed_flag)
      : type_(type), destroyed_flag_(destroyed_flag) {
    if (destroyed_flag_) {
      *destroyed_flag_ = false;
    }
  }

  ~FakeAndroidAudioSink() override {
    if (destroyed_flag_) {
      *destroyed_flag_ = true;
    }
  }

  bool IsType(Type* type) override { return true; }

  void SetPlaybackRate(double playback_rate) override {
    playback_rate_ = playback_rate;
  }

  void SetVolume(double volume) override { volume_ = volume; }

  void SetStartTime(int64_t start_time) override {
    start_time_ = start_time;
    set_start_time_called_ = true;
  }

  bool Flush() override {
    flush_called_ = true;
    return flush_return_value_;
  }

  int GetUnderrunCount() override { return underrun_count_; }
  int GetStartThresholdInFrames() override { return 1024; }

  FakeAudioSinkType* type_;
  bool* destroyed_flag_;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
  int64_t start_time_ = 0;
  bool set_start_time_called_ = false;
  bool flush_called_ = false;
  bool flush_return_value_ = true;
  int underrun_count_ = 0;
};

class FakeAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  FakeAudioSinkType() { SbAudioSinkImpl::SetPrimaryType(this); }

  ~FakeAudioSinkType() override { SbAudioSinkImpl::SetPrimaryType(nullptr); }

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      void* context) override {
    return kSbAudioSinkInvalid;
  }

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid;
  }

  void Destroy(SbAudioSink audio_sink) override {
    delete static_cast<FakeAndroidAudioSink*>(audio_sink);
  }
};

class FakeRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  void GetSourceStatus(int* frames_in_buffer,
                       int* offset_in_frames,
                       bool* is_playing,
                       bool* is_eos_reached) override {
    *frames_in_buffer = 1024;
    *offset_in_frames = 0;
    *is_playing = true;
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
    buffer_data_.resize(1024 * 2 * sizeof(float), 0);
    frame_buffers_[0] = buffer_data_.data();
  }

  void TearDown() override { fake_sink_type_.reset(); }

  AudioRendererSinkImpl::CreateAudioSinkFunc CreateFakeSinkFactory(
      FakeAndroidAudioSink** sink_out,
      bool* destroyed_flag) {
    return
        [this, sink_out, destroyed_flag](
            int64_t start_media_time, int channels, int sampling_frequency_hz,
            SbMediaAudioSampleType audio_sample_type,
            SbAudioSinkFrameBuffers frame_buffers,
            int frame_buffers_size_in_frames,
            SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
            SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
            SbAudioSinkPrivate::ErrorFunc error_func,
            void* context) -> SbAudioSink {
          auto* fake_sink =
              new FakeAndroidAudioSink(fake_sink_type_.get(), destroyed_flag);
          if (sink_out) {
            *sink_out = fake_sink;
          }
          return fake_sink;
        };
  }

  std::unique_ptr<FakeAudioSinkType> fake_sink_type_;
  std::vector<uint8_t> buffer_data_;
  void* frame_buffers_[1];
  FakeRenderCallback render_callback_;
};

TEST_F(AudioRendererSinkAndroidTest, SeekWithFlushAllowedFlushesAndReusesSink) {
  FakeAndroidAudioSink* fake_sink = nullptr;
  bool sink_destroyed = false;

  AudioRendererSinkAndroid sink(
      /*tunnel_mode_audio_session_id=*/std::nullopt,
      /*allow_audio_writing_on_pause=*/false,
      /*enable_video_renderer_vsp_adjustment=*/false,
      /*allow_flush_during_seek=*/true,
      /*pause_using_audio_track_state=*/false,
      CreateFakeSinkFactory(&fake_sink, &sink_destroyed));

  // Initial playback start
  sink.Start(0, 2, 48000, kSbMediaAudioSampleTypeFloat32, frame_buffers_, 1024,
             &render_callback_);
  ASSERT_NE(fake_sink, nullptr);
  EXPECT_TRUE(sink.HasStarted());
  EXPECT_FALSE(sink.is_flushed());

  // Initiate seek (Reset)
  sink.Reset();

  // Verify Flush was called on sink and sink was NOT destroyed
  EXPECT_TRUE(fake_sink->flush_called_);
  EXPECT_FALSE(sink_destroyed);
  EXPECT_TRUE(sink.is_flushed());
  EXPECT_FALSE(sink.HasStarted());  // HasStarted() returns false while flushed

  // Resume playback after seek with identical configuration
  sink.Start(1'500'000, 2, 48000, kSbMediaAudioSampleTypeFloat32,
             frame_buffers_, 1024, &render_callback_);

  // Verify the existing sink was reused and SetStartTime was updated
  EXPECT_TRUE(fake_sink->set_start_time_called_);
  EXPECT_EQ(fake_sink->start_time_, 1'500'000);
  EXPECT_FALSE(sink.is_flushed());
  EXPECT_TRUE(sink.HasStarted());
  EXPECT_FALSE(sink_destroyed);

  sink.Stop();
  EXPECT_TRUE(sink_destroyed);
}

TEST_F(AudioRendererSinkAndroidTest, SeekWithFlushDisallowedDestroysSink) {
  FakeAndroidAudioSink* fake_sink = nullptr;
  bool sink_destroyed = false;

  AudioRendererSinkAndroid sink(
      /*tunnel_mode_audio_session_id=*/std::nullopt,
      /*allow_audio_writing_on_pause=*/false,
      /*enable_video_renderer_vsp_adjustment=*/false,
      /*allow_flush_during_seek=*/false,
      /*pause_using_audio_track_state=*/false,
      CreateFakeSinkFactory(&fake_sink, &sink_destroyed));

  sink.Start(0, 2, 48000, kSbMediaAudioSampleTypeFloat32, frame_buffers_, 1024,
             &render_callback_);
  ASSERT_NE(fake_sink, nullptr);
  EXPECT_TRUE(sink.HasStarted());

  // Reset with flush disallowed must destroy the sink
  sink.Reset();

  EXPECT_FALSE(fake_sink->flush_called_);
  EXPECT_TRUE(sink_destroyed);
  EXPECT_FALSE(sink.is_flushed());
  EXPECT_FALSE(sink.HasStarted());
}

TEST_F(AudioRendererSinkAndroidTest, FlushFailureFallsBackToFullReset) {
  FakeAndroidAudioSink* fake_sink = nullptr;
  bool sink_destroyed = false;

  AudioRendererSinkAndroid sink(
      /*tunnel_mode_audio_session_id=*/std::nullopt,
      /*allow_audio_writing_on_pause=*/false,
      /*enable_video_renderer_vsp_adjustment=*/false,
      /*allow_flush_during_seek=*/true,
      /*pause_using_audio_track_state=*/false,
      CreateFakeSinkFactory(&fake_sink, &sink_destroyed));

  sink.Start(0, 2, 48000, kSbMediaAudioSampleTypeFloat32, frame_buffers_, 1024,
             &render_callback_);
  ASSERT_NE(fake_sink, nullptr);

  // Simulate Flush() failing on the underlying sink
  fake_sink->flush_return_value_ = false;

  sink.Reset();

  EXPECT_TRUE(fake_sink->flush_called_);
  // Failed flush must fall back to full reset, destroying the sink
  EXPECT_TRUE(sink_destroyed);
  EXPECT_FALSE(sink.is_flushed());
  EXPECT_FALSE(sink.HasStarted());
}

TEST_F(AudioRendererSinkAndroidTest,
       FormatChangeDuringFlushedStateRecreatesSink) {
  FakeAndroidAudioSink* first_sink = nullptr;
  bool first_sink_destroyed = false;

  FakeAndroidAudioSink* second_sink = nullptr;
  bool second_sink_destroyed = false;

  int factory_invocations = 0;
  auto factory =
      [&](int64_t start_media_time, int channels, int sampling_frequency_hz,
          SbMediaAudioSampleType audio_sample_type,
          SbAudioSinkFrameBuffers frame_buffers,
          int frame_buffers_size_in_frames,
          SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
          SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
          SbAudioSinkPrivate::ErrorFunc error_func,
          void* context) -> SbAudioSink {
    ++factory_invocations;
    if (factory_invocations == 1) {
      first_sink = new FakeAndroidAudioSink(fake_sink_type_.get(),
                                            &first_sink_destroyed);
      return first_sink;
    } else {
      second_sink = new FakeAndroidAudioSink(fake_sink_type_.get(),
                                             &second_sink_destroyed);
      return second_sink;
    }
  };

  AudioRendererSinkAndroid sink(
      /*tunnel_mode_audio_session_id=*/std::nullopt,
      /*allow_audio_writing_on_pause=*/false,
      /*enable_video_renderer_vsp_adjustment=*/false,
      /*allow_flush_during_seek=*/true,
      /*pause_using_audio_track_state=*/false, factory);

  // Start with 2 channels (stereo)
  sink.Start(0, 2, 48000, kSbMediaAudioSampleTypeFloat32, frame_buffers_, 1024,
             &render_callback_);
  EXPECT_EQ(factory_invocations, 1);

  // Seek and flush
  sink.Reset();
  EXPECT_TRUE(sink.is_flushed());
  EXPECT_FALSE(first_sink_destroyed);

  // Post-seek stream switches to 6 channels (5.1 surround sound)
  sink.Start(1'000'000, 6, 48000, kSbMediaAudioSampleTypeFloat32,
             frame_buffers_, 1024, &render_callback_);

  // Cannot reuse existing 2-channel sink; first sink must be destroyed and
  // second created
  EXPECT_TRUE(first_sink_destroyed);
  EXPECT_EQ(factory_invocations, 2);
  ASSERT_NE(second_sink, nullptr);
  EXPECT_FALSE(sink.is_flushed());
  EXPECT_TRUE(sink.HasStarted());

  sink.Stop();
  EXPECT_TRUE(second_sink_destroyed);
}

}  // namespace
}  // namespace starboard
