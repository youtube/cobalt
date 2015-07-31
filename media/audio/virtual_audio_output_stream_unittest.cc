// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "media/audio/virtual_audio_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

class MockVirtualAudioInputStream : public VirtualAudioInputStream {
 public:
  MockVirtualAudioInputStream(AudioManagerBase* manager,
                              AudioParameters params,
                              base::MessageLoopProxy* message_loop)
      : VirtualAudioInputStream(manager, params, message_loop) {}
  ~MockVirtualAudioInputStream() {}

  MOCK_METHOD2(AddOutputStream, void(VirtualAudioOutputStream* stream,
                                     const AudioParameters& output_params));
  MOCK_METHOD2(RemoveOutputStream, void(VirtualAudioOutputStream* stream,
                                        const AudioParameters& output_params));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVirtualAudioInputStream);
};

class MockAudioDeviceListener : public AudioManager::AudioDeviceListener {
 public:
  MOCK_METHOD0(OnDeviceChange, void());
};

class VirtualAudioOutputStreamTest : public testing::Test {
 public:
  void ListenAndCreateVirtualOnAudioThread(
      AudioManager* manager, AudioManager::AudioDeviceListener* listener) {
    manager->AddOutputDeviceChangeListener(listener);

    AudioParameters params(
        AudioParameters::AUDIO_VIRTUAL, CHANNEL_LAYOUT_MONO, 8000, 8, 128);
    AudioInputStream* stream = manager->MakeAudioInputStream(params, "1");
    stream->Close();
    signal_.Signal();
  }

  void RemoveListenerOnAudioThread(
      AudioManager* manager, AudioManager::AudioDeviceListener* listener) {
    manager->RemoveOutputDeviceChangeListener(listener);
    signal_.Signal();
  }

 protected:
  VirtualAudioOutputStreamTest() : signal_(false, false) {}

  base::WaitableEvent signal_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualAudioOutputStreamTest);
};

TEST_F(VirtualAudioOutputStreamTest, StartStopStartStop) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());

  MessageLoop message_loop;

  AudioParameters params(
      AudioParameters::AUDIO_VIRTUAL, CHANNEL_LAYOUT_MONO, 8000, 8, 128);
  AudioParameters output_params(
      AudioParameters::AUDIO_PCM_LINEAR, CHANNEL_LAYOUT_MONO, 8000, 8, 128);

  MockVirtualAudioInputStream input_stream(
      static_cast<AudioManagerBase*>(audio_manager.get()),
      params,
      message_loop.message_loop_proxy());

  EXPECT_CALL(input_stream, AddOutputStream(_, _)).Times(2);
  EXPECT_CALL(input_stream, RemoveOutputStream(_, _)).Times(2);

  scoped_ptr<VirtualAudioOutputStream> output_stream(
      VirtualAudioOutputStream::MakeStream(
          static_cast<AudioManagerBase*>(audio_manager.get()),
          output_params,
          message_loop.message_loop_proxy(),
          &input_stream));

  SineWaveAudioSource source(CHANNEL_LAYOUT_STEREO, 200.0, 128);
  output_stream->Start(&source);
  output_stream->Stop();
  output_stream->Start(&source);
  output_stream->Stop();
  // Can't Close() here because we didn't create this output stream is not owned
  // by the audio manager.
}

// Tests that we get notifications to reattach output streams when we create a
// VirtualAudioInputStream.
TEST_F(VirtualAudioOutputStreamTest, OutputStreamsNotified) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());

  MockAudioDeviceListener mock_listener;
  EXPECT_CALL(mock_listener, OnDeviceChange()).Times(2);

  audio_manager->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioOutputStreamTest::ListenAndCreateVirtualOnAudioThread,
          base::Unretained(this),
          audio_manager.get(),
          &mock_listener));

  signal_.Wait();

  audio_manager->GetMessageLoop()->PostTask(
      FROM_HERE, base::Bind(
          &VirtualAudioOutputStreamTest::RemoveListenerOnAudioThread,
          base::Unretained(this),
          audio_manager.get(),
          &mock_listener));

  signal_.Wait();
}

}  // namespace media
