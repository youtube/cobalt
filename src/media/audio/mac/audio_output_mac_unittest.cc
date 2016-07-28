// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

class MockAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD2(OnMoreData, int(AudioBus* audio_bus,
                               AudioBuffersState buffers_state));
  MOCK_METHOD3(OnMoreIOData, int(AudioBus* source,
                                 AudioBus* dest,
                                 AudioBuffersState buffers_state));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

// ===========================================================================
// Validation of AudioParameters::AUDIO_PCM_LINEAR
//

// Test that can it be created and closed.
TEST(MacAudioTest, PCMWaveStreamGetAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices())
    return;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      8000, 16, 1024));
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that it can be opened and closed.
TEST(MacAudioTest, PCMWaveStreamOpenAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices())
    return;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      8000, 16, 1024));
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open());
  oas->Close();
}

// This test produces actual audio for 1.5 seconds on the default wave device at
// 44.1K s/sec. Parameters have been chosen carefully so you should not hear
// pops or noises while the sound is playing. The sound must also be identical
// to the sound of PCMWaveStreamPlay200HzTone22KssMono test.
TEST(MacAudioTest, PCMWaveStreamPlay200HzTone44KssMono) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices())
    return;
  uint32 frames_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate, 16, frames_100_ms));
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open());

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate);
  oas->SetVolume(0.5);
  oas->Start(&source);
  usleep(500000);

  // Test that the volume is within the set limits.
  double volume = 0.0;
  oas->GetVolume(&volume);
  EXPECT_LT(volume, 0.51);
  EXPECT_GT(volume, 0.49);
  oas->Stop();
  oas->Close();
}

// This test produces actual audio for 1.5 seconds on the default wave device at
// 22K s/sec. Parameters have been chosen carefully so you should not hear pops
// or noises while the sound is playing. The sound must also be identical to the
// sound of PCMWaveStreamPlay200HzTone44KssMono test.
TEST(MacAudioTest, PCMWaveStreamPlay200HzTone22KssMono) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices())
    return;
  uint32 frames_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate / 2, 16,
                      frames_100_ms));
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(1, 200.0, AudioParameters::kAudioCDSampleRate/2);
  EXPECT_TRUE(oas->Open());
  oas->Start(&source);
  usleep(1500000);
  oas->Stop();
  oas->Close();
}

// Custom action to clear a memory buffer.
static int ClearBuffer(AudioBus* audio_bus,
                       AudioBuffersState buffers_state) {
  audio_bus->Zero();
  return audio_bus->frames();
}

TEST(MacAudioTest, PCMWaveStreamPendingBytes) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!audio_man->HasAudioOutputDevices())
    return;

  uint32 frames_100_ms = AudioParameters::kAudioCDSampleRate / 10;
  AudioOutputStream* oas = audio_man->MakeAudioOutputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO,
                      AudioParameters::kAudioCDSampleRate, 16, frames_100_ms));
  ASSERT_TRUE(NULL != oas);

  NiceMock<MockAudioSource> source;
  EXPECT_TRUE(oas->Open());

  uint32 bytes_100_ms = frames_100_ms * 2;

  // We expect the amount of pending bytes will reaching |bytes_100_ms|
  // because the audio output stream has a double buffer scheme.
  // And then we will try to provide zero data so the amount of pending bytes
  // will go down and eventually read zero.
  InSequence s;
  EXPECT_CALL(source, OnMoreData(NotNull(),
                                 Field(&AudioBuffersState::pending_bytes, 0)))
      .WillOnce(Invoke(ClearBuffer));
  EXPECT_CALL(source, OnMoreData(NotNull(),
                                 Field(&AudioBuffersState::pending_bytes,
                                       bytes_100_ms)))
      .WillOnce(Invoke(ClearBuffer));
  EXPECT_CALL(source, OnMoreData(NotNull(),
                                 Field(&AudioBuffersState::pending_bytes,
                                       bytes_100_ms)))
      .WillOnce(Return(0));
  EXPECT_CALL(source, OnMoreData(NotNull(), _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));

  oas->Start(&source);
  usleep(500000);
  oas->Stop();
  oas->Close();
}

}  // namespace media
