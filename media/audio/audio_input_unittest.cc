// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kSamplingRate = 8000;
static const int kSamplesPerPacket = kSamplingRate / 20;

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  explicit TestInputCallback(int max_data_bytes)
      : callback_count_(0),
        had_error_(0),
        max_data_bytes_(max_data_bytes) {
  }
  virtual void OnData(AudioInputStream* stream, const uint8* data,
                      uint32 size, uint32 hardware_delay_bytes, double volume) {
    ++callback_count_;
    // Read the first byte to make sure memory is good.
    if (size) {
      ASSERT_LE(static_cast<int>(size), max_data_bytes_);
      int value = data[0];
      EXPECT_GE(value, 0);
    }
  }
  virtual void OnClose(AudioInputStream* stream) {}
  virtual void OnError(AudioInputStream* stream, int code) {
    ++had_error_;
  }
  // Returns how many times OnData() has been called.
  int callback_count() const {
    return callback_count_;
  }
  // Returns how many times the OnError callback was called.
  int had_error() const {
    return had_error_;
  }

 private:
  int callback_count_;
  int had_error_;
  int max_data_bytes_;
};

static bool CanRunAudioTests(AudioManager* audio_man) {
  bool has_input = audio_man->HasAudioInputDevices();

  if (!has_input)
    LOG(WARNING) << "No input devices detected";

  return has_input;
}

static AudioInputStream* CreateTestAudioInputStream(AudioManager* audio_man) {
  AudioInputStream* ais = audio_man->MakeAudioInputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      kSamplingRate, 16, kSamplesPerPacket),
                      AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(NULL != ais);
  return ais;
}

// Test that AudioInputStream rejects out of range parameters.
TEST(AudioInputTest, SanityOnMakeParams) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!CanRunAudioTests(audio_man.get()))
    return;

  AudioParameters::Format fmt = AudioParameters::AUDIO_PCM_LINEAR;
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_7_1, 8000, 16,
                      kSamplesPerPacket), AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_MONO, 1024 * 1024, 16,
                      kSamplesPerPacket), AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, 8000, 80,
                      kSamplesPerPacket), AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, 8000, 80,
                      1000 * kSamplesPerPacket),
                      AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_UNSUPPORTED, 8000, 16,
                      kSamplesPerPacket), AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, -8000, 16,
                      kSamplesPerPacket), AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, 8000, -16,
                      kSamplesPerPacket), AudioManagerBase::kDefaultDeviceId));
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_STEREO, 8000, 16, -1024),
      AudioManagerBase::kDefaultDeviceId));
}

// Test create and close of an AudioInputStream without recording audio.
TEST(AudioInputTest, CreateAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!CanRunAudioTests(audio_man.get()))
    return;
  AudioInputStream* ais = CreateTestAudioInputStream(audio_man.get());
  ais->Close();
}

// Test create, open and close of an AudioInputStream without recording audio.
TEST(AudioInputTest, OpenAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!CanRunAudioTests(audio_man.get()))
    return;
  AudioInputStream* ais = CreateTestAudioInputStream(audio_man.get());
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Test create, open, stop and close of an AudioInputStream without recording.
TEST(AudioInputTest, OpenStopAndClose) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!CanRunAudioTests(audio_man.get()))
    return;
  AudioInputStream* ais = CreateTestAudioInputStream(audio_man.get());
  EXPECT_TRUE(ais->Open());
  ais->Stop();
  ais->Close();
}

// Test a normal recording sequence using an AudioInputStream.
// This test currently fails on the Linux bots (crbug.com/165401).
#if defined(OS_LINUX)
#define MAYBE_Record DISABLED_Record
#else
#define MAYBE_Record Record
#endif // defined(OS_LINUX)
TEST(AudioInputTest, MAYBE_Record) {
  scoped_ptr<AudioManager> audio_man(AudioManager::Create());
  if (!CanRunAudioTests(audio_man.get()))
    return;
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  AudioInputStream* ais = CreateTestAudioInputStream(audio_man.get());
  EXPECT_TRUE(ais->Open());

  TestInputCallback test_callback(kSamplesPerPacket * 4);
  ais->Start(&test_callback);
  // Verify at least 500ms worth of audio was recorded, after giving sufficient
  // extra time.
  message_loop.PostDelayedTask(
      FROM_HERE,
      MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(690));
  message_loop.Run();
  EXPECT_GE(test_callback.callback_count(), 1);
  EXPECT_FALSE(test_callback.had_error());

  ais->Stop();
  ais->Close();
}

}  // namespace media
