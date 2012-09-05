// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>  // std::min
#include <limits>

#include "base/logging.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Validate that the SineWaveAudioSource writes the expected values.
TEST(SimpleSources, SineWaveAudioSource) {
  const uint32 samples = 1024;
  const uint32 bytes_per_sample = 2;
  const int freq = 200;

  SineWaveAudioSource source(1, freq, AudioParameters::kTelephoneSampleRate);

  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  AudioParameters params(
      AudioParameters::AUDIO_MOCK, CHANNEL_LAYOUT_MONO,
      AudioParameters::kTelephoneSampleRate, bytes_per_sample * 8, samples);
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(params);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open());

  oas->Start(&source);
  oas->Stop();

  ASSERT_TRUE(FakeAudioOutputStream::GetCurrentFakeStream());
  const AudioBus* last_audio_bus =
      FakeAudioOutputStream::GetCurrentFakeStream()->audio_bus();
  ASSERT_TRUE(NULL != last_audio_bus);

  uint32 half_period = AudioParameters::kTelephoneSampleRate / (freq * 2);

  // Spot test positive incursion of sine wave.
  EXPECT_NEAR(0, last_audio_bus->channel(0)[0],
              std::numeric_limits<float>::epsilon());
  EXPECT_FLOAT_EQ(0.15643446f, last_audio_bus->channel(0)[1]);
  EXPECT_LT(last_audio_bus->channel(0)[1], last_audio_bus->channel(0)[2]);
  EXPECT_LT(last_audio_bus->channel(0)[2], last_audio_bus->channel(0)[3]);
  // Spot test negative incursion of sine wave.
  EXPECT_NEAR(0, last_audio_bus->channel(0)[half_period],
              std::numeric_limits<float>::epsilon());
  EXPECT_FLOAT_EQ(-0.15643446f, last_audio_bus->channel(0)[half_period + 1]);
  EXPECT_GT(last_audio_bus->channel(0)[half_period + 1],
            last_audio_bus->channel(0)[half_period + 2]);
  EXPECT_GT(last_audio_bus->channel(0)[half_period + 2],
            last_audio_bus->channel(0)[half_period + 3]);

  oas->Close();
}

TEST(SimpleSources, SineWaveAudioCapped) {
  SineWaveAudioSource source(1, 200, AudioParameters::kTelephoneSampleRate);

  static const int kSampleCap = 100;
  source.CapSamples(kSampleCap);

  scoped_ptr<AudioBus> audio_bus = AudioBus::Create(1, 2 * kSampleCap);
  EXPECT_EQ(source.OnMoreData(
      audio_bus.get(), AudioBuffersState()), kSampleCap);
  EXPECT_EQ(source.OnMoreData(audio_bus.get(), AudioBuffersState()), 0);
  source.Reset();
  EXPECT_EQ(source.OnMoreData(
      audio_bus.get(), AudioBuffersState()), kSampleCap);
}

}  // namespace media
