// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const int kBufferSize = 8192;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;

class AudioRendererMixerInputTest : public testing::Test {
 public:
  AudioRendererMixerInputTest() {
    audio_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
        kBitsPerChannel, kBufferSize);

    mixer_input_ = new AudioRendererMixerInput(
        base::Bind(
            &AudioRendererMixerInputTest::GetMixer, base::Unretained(this)),
        base::Bind(
            &AudioRendererMixerInputTest::RemoveMixer, base::Unretained(this)));
    fake_callback_.reset(new FakeAudioRenderCallback(0));
    mixer_input_->Initialize(audio_parameters_, fake_callback_.get());
    EXPECT_CALL(*this, RemoveMixer(testing::_));
  }

  AudioRendererMixer* GetMixer(const AudioParameters& params) {
    if (!mixer_.get()) {
      scoped_refptr<MockAudioRendererSink> sink = new MockAudioRendererSink();
      EXPECT_CALL(*sink, Start());
      EXPECT_CALL(*sink, Stop());

      mixer_.reset(new AudioRendererMixer(
          audio_parameters_, audio_parameters_, sink));
    }
    return mixer_.get();
  }

  MOCK_METHOD1(RemoveMixer, void(const AudioParameters&));

 protected:
  virtual ~AudioRendererMixerInputTest() {}

  AudioParameters audio_parameters_;
  scoped_ptr<AudioRendererMixer> mixer_;
  scoped_refptr<AudioRendererMixerInput> mixer_input_;
  scoped_ptr<FakeAudioRenderCallback> fake_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerInputTest);
};

// Test callback() works as expected.
TEST_F(AudioRendererMixerInputTest, GetCallback) {
  EXPECT_EQ(mixer_input_->callback(), fake_callback_.get());
}

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
