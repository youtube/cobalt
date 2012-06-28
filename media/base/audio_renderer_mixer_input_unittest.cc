// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_util.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;

class AudioRendererMixerInputTest : public ::testing::Test {
 public:
  AudioRendererMixerInputTest() {
    audio_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
        kBitsPerChannel, GetHighLatencyOutputBufferSize(kSampleRate));

    mixer_ = new AudioRendererMixer(audio_parameters_, new NullAudioSink());
    mixer_input_ = new AudioRendererMixerInput(mixer_);
    fake_callback_.reset(new FakeAudioRenderCallback(audio_parameters_));
    mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
  }

  // Render audio_parameters_.frames_per_buffer() frames into |audio_data_| and
  // verify the result against |check_value|.
  void RenderAndValidateAudioData(float check_value) {
    const std::vector<float*>& audio_data = mixer_input_->audio_data();

    ASSERT_EQ(fake_callback_->Render(
        audio_data, audio_parameters_.frames_per_buffer(), 0),
        audio_parameters_.frames_per_buffer());

    for (size_t i = 0; i < audio_data.size(); ++i)
      for (int j = 0; j < audio_parameters_.frames_per_buffer(); j++)
        ASSERT_FLOAT_EQ(check_value, audio_data[i][j]);
  }

 protected:
  virtual ~AudioRendererMixerInputTest() {}

  AudioParameters audio_parameters_;
  scoped_refptr<AudioRendererMixer> mixer_;
  scoped_refptr<AudioRendererMixerInput> mixer_input_;
  scoped_ptr<FakeAudioRenderCallback> fake_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerInputTest);
};

// Test that getting and setting the volume work as expected.
TEST_F(AudioRendererMixerInputTest, GetSetVolume) {
  // Starting volume should be 0.
  double volume = 1.0f;
  mixer_input_->GetVolume(&volume);
  EXPECT_EQ(volume, 1.0f);

  const double kVolume = 0.5f;
  EXPECT_TRUE(mixer_input_->SetVolume(kVolume));
  mixer_input_->GetVolume(&volume);
  EXPECT_EQ(volume, kVolume);
}

// Test audio_data() is allocated correctly.
TEST_F(AudioRendererMixerInputTest, GetAudioData) {
  RenderAndValidateAudioData(fake_callback_->fill_value());

  // TODO(dalecurtis): Perform alignment and size checks when we switch over to
  // FFmpeg optimized vector_fmac.
}

// Test callback() works as expected.
TEST_F(AudioRendererMixerInputTest, GetCallback) {
  EXPECT_EQ(mixer_input_->callback(), fake_callback_.get());
  RenderAndValidateAudioData(fake_callback_->fill_value());
}

// Test Start()/Play()/Pause()/Stop()/playing() all work as expected.  Also
// implicitly tests that AddMixerInput() and RemoveMixerInput() work without
// crashing; functional tests for these methods are in AudioRendererMixerTest.
TEST_F(AudioRendererMixerInputTest, StartPlayPauseStopPlaying) {
  mixer_input_->Start();
  EXPECT_FALSE(mixer_input_->playing());
  mixer_input_->Play();
  EXPECT_TRUE(mixer_input_->playing());
  mixer_input_->Pause(false);
  EXPECT_FALSE(mixer_input_->playing());
  mixer_input_->Play();
  EXPECT_TRUE(mixer_input_->playing());
  mixer_input_->Stop();
  EXPECT_FALSE(mixer_input_->playing());
}

}  // namespace media
