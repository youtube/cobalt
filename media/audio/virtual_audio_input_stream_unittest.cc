// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockInputCallback() {}
  virtual void OnData(AudioInputStream* stream, const uint8* data,
                      uint32 size, uint32 hardware_delay_bytes,
                      double volume) {}
  virtual void OnClose(AudioInputStream* stream) {}
  virtual void OnError(AudioInputStream* stream, int code) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputCallback);
};

class VirtualAudioInputStreamTest : public testing::Test {
 public:
  VirtualAudioInputStreamTest()
      : audio_manager_(AudioManager::Create()),
        params_(
            AudioParameters::AUDIO_VIRTUAL,CHANNEL_LAYOUT_MONO, 8000, 8, 128),
        output_params_(
            AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO, 8000, 8,
            128),
        stream_(NULL),
        source_(CHANNEL_LAYOUT_STEREO, 200.0, 128),
        done_(false, false) {
  }

  void StartStreamAndRunTestsOnAudioThread(int num_output_streams,
                                           int num_callback_iterations,
                                           int num_streams_removed_per_round,
                                           int num_expected_source_callbacks) {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    stream_->Open();
    stream_->Start(&input_callback_);
    AddStreamsAndDoCallbacks(num_output_streams,
                             num_callback_iterations,
                             num_streams_removed_per_round,
                             num_expected_source_callbacks);
  }

  void AddStreamsAndDoCallbacks(int num_output_streams,
                                int num_callback_iterations,
                                int num_streams_removed_per_round,
                                int num_expected_source_callbacks) {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());

    for (int i = 0; i < num_output_streams; ++i) {
      AudioOutputStream* output_stream =
          audio_manager_->MakeAudioOutputStream(output_params_);
      DCHECK(output_stream);
      output_streams_.push_back(output_stream);

      output_stream->Open();
      output_stream->Start(&source_);
    }

    if (num_output_streams == 0 && num_streams_removed_per_round > 0) {
      AudioOutputStream* output_stream = output_streams_.back();
      output_streams_.pop_back();
      output_stream->Stop();
      output_stream->Close();
    }

    if (num_callback_iterations > 0) {
      // Force the next callback to be immediate.
      stream_->buffer_duration_ms_ = base::TimeDelta();
      audio_manager_->GetMessageLoop()->PostTask(
          FROM_HERE, base::Bind(
              &VirtualAudioInputStreamTest::AddStreamsAndDoCallbacks,
              base::Unretained(this),
              0,
              --num_callback_iterations,
              num_streams_removed_per_round,
              num_expected_source_callbacks));
    } else {
      // Finish the test.
      EXPECT_EQ(num_expected_source_callbacks, source_.callbacks());
      EXPECT_EQ(0, source_.errors());

      for (std::vector<AudioOutputStream*>::iterator it =
           output_streams_.begin(); it != output_streams_.end(); ++it)
        (*it)->Stop();

      stream_->Stop();

      audio_manager_->GetMessageLoop()->PostTask(
          FROM_HERE, base::Bind(&VirtualAudioInputStreamTest::EndTest,
                                base::Unretained(this)));
    }
  }

  void OpenAndCloseOnAudioThread() {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    stream_->Open();

    // Create 2 output streams, which we just open and close without starting.
    const int num_output_stream = 2;

    for (int i = 0; i < num_output_stream; ++i) {
      AudioOutputStream* output_stream =
          audio_manager_->MakeAudioOutputStream(output_params_);
      DCHECK(output_stream);
      output_streams_.push_back(output_stream);

      output_stream->Open();
    }

    audio_manager_->GetMessageLoop()->PostTask(
        FROM_HERE, base::Bind(&VirtualAudioInputStreamTest::EndTest,
                              base::Unretained(this)));
  }

  void StartStopOnAudioThread(int num_output_streams,
                              int num_callback_iterations,
                              int num_expected_source_callbacks) {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
    stream_->Open();
    stream_->Start(&input_callback_);
    StartStopCallback(true, num_output_streams, num_callback_iterations,
                      num_expected_source_callbacks);
  }

  void StartStopCallback(bool init,
                         int num_output_streams,
                         int num_callback_iterations,
                         int num_expected_source_callbacks) {
    ASSERT_TRUE(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());

    if (init) {
      for (int i = 0; i < num_output_streams; ++i) {
        AudioOutputStream* output_stream =
            audio_manager_->MakeAudioOutputStream(output_params_);
        DCHECK(output_stream);
        output_streams_.push_back(output_stream);

        output_stream->Open();
        output_stream->Start(&source_);
      }

      // Start with an odd iteration number so we call Stop() first below.
      DCHECK_NE(0, num_callback_iterations % 2);
    }

    // Start or stop half the streams.
    for (int i = 0; i < num_output_streams / 2; ++i) {
      if (num_callback_iterations % 2 != 0)
        output_streams_[i]->Stop();
      else
        output_streams_[i]->Start(&source_);
    }

    if (num_callback_iterations > 0) {
      // Force the next callback to be immediate.
      stream_->buffer_duration_ms_ = base::TimeDelta::FromMilliseconds(0);
      audio_manager_->GetMessageLoop()->PostTask(
          FROM_HERE, base::Bind(
              &VirtualAudioInputStreamTest::StartStopCallback,
              base::Unretained(this),
              false,
              num_output_streams,
              --num_callback_iterations,
              num_expected_source_callbacks));
    } else {
      // Finish the test.
      EXPECT_EQ(num_expected_source_callbacks, source_.callbacks());
      EXPECT_EQ(0, source_.errors());

      for (std::vector<AudioOutputStream*>::iterator it =
           output_streams_.begin(); it != output_streams_.end(); ++it)
        (*it)->Stop();

      stream_->Stop();

      audio_manager_->GetMessageLoop()->PostTask(FROM_HERE,
          base::Bind(&VirtualAudioInputStreamTest::EndTest,
                 base::Unretained(this)));
    }
  }

  void EndTest() {
    for (std::vector<AudioOutputStream*>::iterator it =
         output_streams_.begin(); it != output_streams_.end(); ++it)
      (*it)->Close();

    stream_->Close();

    done_.Signal();
  }

 protected:
  scoped_ptr<AudioManager> audio_manager_;
  AudioParameters params_;
  AudioParameters output_params_;
  VirtualAudioInputStream* stream_;
  MockInputCallback input_callback_;
  std::vector<AudioOutputStream*> output_streams_;
  SineWaveAudioSource source_;
  base::WaitableEvent done_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualAudioInputStreamTest);
};

TEST_F(VirtualAudioInputStreamTest, AttachAndDriveSingleStream) {
  stream_ = static_cast<VirtualAudioInputStream*>(
      audio_manager_->MakeAudioInputStream(params_, "1"));
  DCHECK(stream_);

  const int num_output_streams = 1;
  const int num_callback_iterations = 1;
  const int num_streams_removed_per_round = 0;
  const int num_expected_source_callbacks = 1;

  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioInputStreamTest::StartStreamAndRunTestsOnAudioThread,
          base::Unretained(this),
          num_output_streams,
          num_callback_iterations,
          num_streams_removed_per_round,
          num_expected_source_callbacks));

  done_.Wait();
}

TEST_F(VirtualAudioInputStreamTest, AttachAndDriveMultipleStreams) {
  stream_ = static_cast<VirtualAudioInputStream*>(
      audio_manager_->MakeAudioInputStream(params_, "1"));
  DCHECK(stream_);

  const int num_output_streams = 5;
  const int num_callback_iterations = 5;
  const int num_streams_removed_per_round = 0;
  const int num_expected_source_callbacks = 25;

  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioInputStreamTest::StartStreamAndRunTestsOnAudioThread,
          base::Unretained(this),
          num_output_streams,
          num_callback_iterations,
          num_streams_removed_per_round,
          num_expected_source_callbacks));

  done_.Wait();
}

TEST_F(VirtualAudioInputStreamTest, AttachAndRemoveStreams) {
  stream_ = static_cast<VirtualAudioInputStream*>(
      audio_manager_->MakeAudioInputStream(params_, "1"));
  DCHECK(stream_);

  const int num_output_streams = 8;
  const int num_callback_iterations = 5;
  const int num_streams_removed_per_round = 1;
  const int num_expected_source_callbacks = 8 + 7 + 6 + 5 + 4;

  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioInputStreamTest::StartStreamAndRunTestsOnAudioThread,
          base::Unretained(this),
          num_output_streams,
          num_callback_iterations,
          num_streams_removed_per_round,
          num_expected_source_callbacks));

  done_.Wait();
}

// Opens/closes a VirtualAudioInputStream and a number of attached
// VirtualAudioOutputStreams without calling Start()/Stop().
TEST_F(VirtualAudioInputStreamTest, OpenAndClose) {
  stream_ = static_cast<VirtualAudioInputStream*>(
      audio_manager_->MakeAudioInputStream(params_, "1"));
  DCHECK(stream_);

  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioInputStreamTest::OpenAndCloseOnAudioThread,
          base::Unretained(this)));

  done_.Wait();
}

// Creates and closes and VirtualAudioInputStream.
TEST_F(VirtualAudioInputStreamTest, CreateAndClose) {
  stream_ = static_cast<VirtualAudioInputStream*>(
      audio_manager_->MakeAudioInputStream(params_, "1"));
  DCHECK(stream_);

  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(&VirtualAudioInputStreamTest::EndTest,
                            base::Unretained(this)));

  done_.Wait();
}

// Starts and stops VirtualAudioOutputStreams while attached to a
// VirtualAudioInputStream.
TEST_F(VirtualAudioInputStreamTest, AttachAndStartStopStreams) {
  stream_ = static_cast<VirtualAudioInputStream*>(
      audio_manager_->MakeAudioInputStream(params_, "1"));
  DCHECK(stream_);

  const int num_output_streams = 4;
  const int num_callback_iterations = 5;
  const int num_expected_source_callbacks = 2 + 4 + 2 + 4 + 2;

  audio_manager_->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioInputStreamTest::StartStopOnAudioThread,
          base::Unretained(this),
          num_output_streams,
          num_callback_iterations,
          num_expected_source_callbacks));

  done_.Wait();
}

}  // namespace media
