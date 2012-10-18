// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FakeAudioOutputStreamTest : public testing::Test {
 public:
  FakeAudioOutputStreamTest()
      : audio_manager_(AudioManager::Create()),
        params_(
            AudioParameters::AUDIO_FAKE, CHANNEL_LAYOUT_STEREO, 8000, 8, 128),
        source_(params_.channels(), 200.0, params_.sample_rate()),
        done_(false, false) {
    stream_ = audio_manager_->MakeAudioOutputStream(AudioParameters(params_));
    CHECK(stream_);
  }

  virtual ~FakeAudioOutputStreamTest() {}

  void RunOnAudioThread() {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    ASSERT_TRUE(stream_->Open());
    stream_->Start(&source_);
    // Start() should immediately post a task to run the source callback, so we
    // should end up with only a single callback being run.
    audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
        &FakeAudioOutputStreamTest::EndTest, base::Unretained(this)));
  }

  void EndTest() {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    stream_->Stop();
    stream_->Close();
    EXPECT_EQ(1, source_.callbacks());
    EXPECT_EQ(0, source_.errors());
    done_.Signal();
  }

 protected:
  scoped_ptr<AudioManager> audio_manager_;
  AudioParameters params_;
  AudioOutputStream* stream_;
  SineWaveAudioSource source_;
  base::WaitableEvent done_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputStreamTest);
};

// Ensure the fake audio stream runs on the audio thread and handles fires
// callbacks to the AudioSourceCallback.
TEST_F(FakeAudioOutputStreamTest, FakeStreamBasicCallback) {
  audio_manager_->GetMessageLoop()->PostTask(FROM_HERE, base::Bind(
      &FakeAudioOutputStreamTest::RunOnAudioThread, base::Unretained(this)));
  done_.Wait();
}

}  // namespace media
