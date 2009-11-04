// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "media/audio/audio_output.h"
#include "media/audio/simple_sources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

class MockAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD4(OnMoreData, size_t(AudioOutputStream* stream, void* dest,
                                  size_t max_size, int pending_bytes));
  MOCK_METHOD1(OnClose, void(AudioOutputStream* stream));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

// Validate that the SineWaveAudioSource writes the expected values for
// the FORMAT_16BIT_MONO.
TEST(MacAudioTest, SineWaveAudio16MonoTest) {
  const size_t samples = 1024;
  const int freq = 200;

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             freq, AudioManager::kTelephoneSampleRate);

  // TODO(cpu): Put the real test when the mock renderer is ported.
  int16 buffer[samples] = { 0xffff };
  source.OnMoreData(NULL, buffer, sizeof(buffer), 0);
  EXPECT_EQ(0, buffer[0]);
  EXPECT_EQ(5126, buffer[1]);
}

// ===========================================================================
// Validation of AudioManager::AUDIO_PCM_LINEAR
//
// Unlike windows, the tests can reliably detect the existense of real
// audio devices on the bots thus no need for 'headless' detection.

// Test that can it be created and closed.
TEST(MacAudioTest, PCMWaveStreamGetAndClose) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  oas->Close();
}

// Test that it can be opened and closed.
TEST(MacAudioTest, PCMWaveStreamOpenAndClose) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 2, 8000, 16);
  ASSERT_TRUE(NULL != oas);
  EXPECT_TRUE(oas->Open(1024));
  oas->Close();
}

// This test produces actual audio for 1.5 seconds on the default wave device at
// 44.1K s/sec. Parameters have been chosen carefully so you should not hear
// pops or noises while the sound is playing. The sound must also be identical
// to the sound of PCMWaveStreamPlay200HzTone22KssMono test.
TEST(MacAudioTest, PCMWaveStreamPlay200HzTone44KssMono) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
  audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                             AudioManager::kAudioCDSampleRate, 16);
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             200.0, AudioManager::kAudioCDSampleRate);
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 10) * 2;

  EXPECT_TRUE(oas->Open(bytes_100_ms));

  oas->SetVolume(0.5);
  oas->Start(&source);
  usleep(1500000);

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
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
  audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                             AudioManager::kAudioCDSampleRate/2, 16);
  ASSERT_TRUE(NULL != oas);

  SineWaveAudioSource source(SineWaveAudioSource::FORMAT_16BIT_LINEAR_PCM, 1,
                             200.0, AudioManager::kAudioCDSampleRate/2);
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 20) * 2;

  EXPECT_TRUE(oas->Open(bytes_100_ms));
  oas->Start(&source);
  usleep(1500000);
  oas->Stop();
  oas->Close();
}

// Custom action to clear a memory buffer.
static void ClearBuffer(AudioOutputStream* strea, void* dest,
                        size_t max_size, size_t pending_bytes) {
  memset(dest, 0, max_size);
}

TEST(MacAudioTest, PCMWaveStreamPendingBytes) {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  ASSERT_TRUE(NULL != audio_man);
  if (!audio_man->HasAudioDevices())
    return;
  AudioOutputStream* oas =
      audio_man->MakeAudioStream(AudioManager::AUDIO_PCM_LINEAR, 1,
                                 AudioManager::kAudioCDSampleRate, 16);
  ASSERT_TRUE(NULL != oas);

  NiceMock<MockAudioSource> source;
  size_t bytes_100_ms = (AudioManager::kAudioCDSampleRate / 10) * 2;
  EXPECT_TRUE(oas->Open(bytes_100_ms));

  // We expect the amount of pending bytes will reaching |bytes_100_ms|
  // because the audio output stream has a double buffer scheme.
  // And then we will try to provide zero data so the amount of pending bytes
  // will go down and eventually read zero.
  InSequence s;
  EXPECT_CALL(source, OnMoreData(oas, NotNull(), bytes_100_ms, 0))
      .WillOnce(DoAll(Invoke(&ClearBuffer), Return(bytes_100_ms)));
  EXPECT_CALL(source, OnMoreData(oas, NotNull(), bytes_100_ms, bytes_100_ms))
      .WillOnce(DoAll(Invoke(&ClearBuffer), Return(bytes_100_ms)));
  EXPECT_CALL(source, OnMoreData(oas, NotNull(), bytes_100_ms, bytes_100_ms))
      .WillOnce(Return(0));
  EXPECT_CALL(source, OnMoreData(oas, NotNull(), bytes_100_ms, _))
      .Times(AnyNumber())
      .WillRepeatedly(Return(0));

  oas->Start(&source);
  usleep(500000);
  oas->Stop();
  oas->Close();
}
