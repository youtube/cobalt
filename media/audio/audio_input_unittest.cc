// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

static const int kSamplingRate = 8000;
static const int kSamplesPerPacket = kSamplingRate / 20;

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  explicit TestInputCallback(int max_data_bytes)
      : callback_count_(0),
        had_error_(0),
        was_closed_(0),
        max_data_bytes_(max_data_bytes) {
  }
  virtual void OnData(AudioInputStream* stream, const uint8* data,
                      uint32 size, uint32 hardware_delay_bytes) {
    ++callback_count_;
    // Read the first byte to make sure memory is good.
    if (size) {
      ASSERT_LE(static_cast<int>(size), max_data_bytes_);
      int value = data[0];
      EXPECT_TRUE(value >= 0);
    }
  }
  virtual void OnClose(AudioInputStream* stream) {
    ++was_closed_;
  }
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

  void set_error(bool error) {
    had_error_ += error ? 1 : 0;
  }
  // Returns how many times the OnClose callback was called.
  int was_closed() const {
    return was_closed_;
  }

 private:
  int callback_count_;
  int had_error_;
  int was_closed_;
  int max_data_bytes_;
};

// Specializes TestInputCallback to simulate a sink that blocks for some time
// in the OnData callback.
class TestInputCallbackBlocking : public TestInputCallback {
 public:
  TestInputCallbackBlocking(int max_data_bytes, int block_after_callback,
                            int block_for_ms)
      : TestInputCallback(max_data_bytes),
        block_after_callback_(block_after_callback),
        block_for_ms_(block_for_ms) {
  }
  virtual void OnData(AudioInputStream* stream, const uint8* data,
                      uint32 size, uint32 hardware_delay_bytes) {
    // Call the base, which increments the callback_count_.
    TestInputCallback::OnData(stream, data, size, hardware_delay_bytes);
    if (callback_count() > block_after_callback_)
      base::PlatformThread::Sleep(block_for_ms_);
  }

 private:
  int block_after_callback_;
  int block_for_ms_;
};

static bool CanRunAudioTests() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return false;

  AudioManager* audio_man = AudioManager::GetAudioManager();
  if (NULL == audio_man)
    return false;

  return audio_man->HasAudioInputDevices();
}

static AudioInputStream* CreateTestAudioInputStream() {
  AudioManager* audio_man = AudioManager::GetAudioManager();
  AudioInputStream* ais = audio_man->MakeAudioInputStream(
      AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_STEREO,
                      kSamplingRate, 16, kSamplesPerPacket),
                      AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(NULL != ais);
  return ais;
}

// Test that AudioInputStream rejects out of range parameters.
TEST(AudioInputTest, SanityOnMakeParams) {
  if (!CanRunAudioTests())
    return;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  AudioParameters::Format fmt = AudioParameters::AUDIO_PCM_LINEAR;
  EXPECT_TRUE(NULL == audio_man->MakeAudioInputStream(
      AudioParameters(fmt, CHANNEL_LAYOUT_7POINT1, 8000, 16,
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
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateTestAudioInputStream();
  ais->Close();
}

// Test create, open and close of an AudioInputStream without recording audio.
TEST(AudioInputTest, OpenAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateTestAudioInputStream();
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Test create, open, stop and close of an AudioInputStream without recording.
TEST(AudioInputTest, OpenStopAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateTestAudioInputStream();
  EXPECT_TRUE(ais->Open());
  ais->Stop();
  ais->Close();
}

// Test a normal recording sequence using an AudioInputStream.
TEST(AudioInputTest, Record) {
  if (!CanRunAudioTests())
    return;
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  AudioInputStream* ais = CreateTestAudioInputStream();
  EXPECT_TRUE(ais->Open());

  TestInputCallback test_callback(kSamplesPerPacket * 4);
  ais->Start(&test_callback);
  // Verify at least 500ms worth of audio was recorded, after giving sufficient
  // extra time.
  message_loop.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(), 590);
  message_loop.Run();
  EXPECT_GE(test_callback.callback_count(), 10);
  EXPECT_FALSE(test_callback.had_error());

  ais->Stop();
  ais->Close();
}

// Test a recording sequence with delays in the audio callback.
TEST(AudioInputTest, RecordWithSlowSink) {
  if (!CanRunAudioTests())
    return;
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  AudioInputStream* ais = CreateTestAudioInputStream();
  EXPECT_TRUE(ais->Open());

  // We should normally get a callback every 50ms, and a 20ms delay inside each
  // callback should not change this sequence.
  TestInputCallbackBlocking test_callback(kSamplesPerPacket * 4, 0, 20);
  ais->Start(&test_callback);
  // Verify at least 500ms worth of audio was recorded, after giving sufficient
  // extra time.
  message_loop.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(), 590);
  message_loop.Run();
  EXPECT_GE(test_callback.callback_count(), 10);
  EXPECT_FALSE(test_callback.had_error());

  ais->Stop();
  ais->Close();
}
