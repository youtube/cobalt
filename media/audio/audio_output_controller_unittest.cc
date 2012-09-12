// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/environment.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_output_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(vrk): These tests need to be rewritten! (crbug.com/112500)

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kSamplesPerPacket = kSampleRate / 10;
static const int kHardwareBufferSize = kSamplesPerPacket *
    ChannelLayoutToChannelCount(kChannelLayout) * kBitsPerSample / 8;

class MockAudioOutputControllerEventHandler
    : public AudioOutputController::EventHandler {
 public:
  MockAudioOutputControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioOutputController* controller));
  MOCK_METHOD1(OnPlaying, void(AudioOutputController* controller));
  MOCK_METHOD1(OnPaused, void(AudioOutputController* controller));
  MOCK_METHOD2(OnError, void(AudioOutputController* controller,
                             int error_code));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerEventHandler);
};

class MockAudioOutputControllerSyncReader
    : public AudioOutputController::SyncReader {
 public:
  MockAudioOutputControllerSyncReader() {}

  MOCK_METHOD1(UpdatePendingBytes, void(uint32 bytes));
  MOCK_METHOD2(Read, int(AudioBus* source, AudioBus* dest));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(DataReady, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioOutputControllerSyncReader);
};

ACTION_P(SignalEvent, event) {
  event->Signal();
}

// Custom action to clear a memory buffer.
ACTION(ClearBuffer) {
  arg1->Zero();
}

// Closes AudioOutputController synchronously.
static void CloseAudioController(AudioOutputController* controller) {
  controller->Close(MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
}

class AudioOutputControllerTest : public testing::Test {
 public:
  AudioOutputControllerTest() {}
  virtual ~AudioOutputControllerTest() {}

 protected:
  MessageLoopForIO message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioOutputControllerTest);
};

TEST_F(AudioOutputControllerTest, CreateAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!audio_manager->HasAudioOutputDevices())
    return;

  MockAudioOutputControllerEventHandler event_handler;

  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(1);

  MockAudioOutputControllerSyncReader sync_reader;
  EXPECT_CALL(sync_reader, Close());

  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
  scoped_refptr<AudioOutputController> controller =
      AudioOutputController::Create(
          audio_manager.get(), &event_handler, params, &sync_reader);
  ASSERT_TRUE(controller.get());

  // Close the controller immediately.
  CloseAudioController(controller);
}

TEST_F(AudioOutputControllerTest, PlayPauseClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!audio_manager->HasAudioOutputDevices())
    return;

  MockAudioOutputControllerEventHandler event_handler;
  base::WaitableEvent event(false, false);
  base::WaitableEvent pause_event(false, false);

  // If OnCreated is called then signal the event.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  // OnPlaying() will be called only once.
  EXPECT_CALL(event_handler, OnPlaying(NotNull()));

  MockAudioOutputControllerSyncReader sync_reader;
  EXPECT_CALL(sync_reader, UpdatePendingBytes(_))
      .Times(AtLeast(2));
  EXPECT_CALL(sync_reader, Read(_, _))
      .WillRepeatedly(DoAll(ClearBuffer(), SignalEvent(&event),
                            Return(4)));
  EXPECT_CALL(sync_reader, DataReady())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(event_handler, OnPaused(NotNull()))
      .WillOnce(InvokeWithoutArgs(&pause_event, &base::WaitableEvent::Signal));
  EXPECT_CALL(sync_reader, Close());

  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
  scoped_refptr<AudioOutputController> controller =
      AudioOutputController::Create(
          audio_manager.get(), &event_handler, params, &sync_reader);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to be called.
  event.Wait();

  ASSERT_FALSE(pause_event.IsSignaled());
  controller->Play();
  controller->Pause();
  pause_event.Wait();

  // Now stop the controller.
  CloseAudioController(controller);
}

TEST_F(AudioOutputControllerTest, HardwareBufferTooLarge) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!audio_manager->HasAudioOutputDevices())
    return;

  // Create an audio device with a very large hardware buffer size.
  MockAudioOutputControllerEventHandler event_handler;

  MockAudioOutputControllerSyncReader sync_reader;
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kSampleRate, kBitsPerSample,
                         kSamplesPerPacket * 1000);
  scoped_refptr<AudioOutputController> controller =
      AudioOutputController::Create(
          audio_manager.get(), &event_handler, params, &sync_reader);

  // Use assert because we don't stop the device and assume we can't
  // create one.
  ASSERT_FALSE(controller);
}

TEST_F(AudioOutputControllerTest, PlayPausePlayClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!audio_manager->HasAudioOutputDevices())
    return;

  MockAudioOutputControllerEventHandler event_handler;
  base::WaitableEvent event(false, false);
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  // OnPlaying() will be called only once.
  base::WaitableEvent play_event(false, false);
  EXPECT_CALL(event_handler, OnPlaying(NotNull()))
      .WillOnce(InvokeWithoutArgs(&play_event, &base::WaitableEvent::Signal));

  // OnPaused() should never be called since the pause during kStarting is
  // dropped when the second play comes in.
  EXPECT_CALL(event_handler, OnPaused(NotNull()))
      .Times(0);

  MockAudioOutputControllerSyncReader sync_reader;
  EXPECT_CALL(sync_reader, UpdatePendingBytes(_))
      .Times(AtLeast(1));
  EXPECT_CALL(sync_reader, Read(_, _))
      .WillRepeatedly(DoAll(ClearBuffer(), SignalEvent(&event), Return(4)));
  EXPECT_CALL(sync_reader, DataReady())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(sync_reader, Close());

  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
  scoped_refptr<AudioOutputController> controller =
      AudioOutputController::Create(
          audio_manager.get(), &event_handler, params, &sync_reader);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to be called.
  event.Wait();

  ASSERT_FALSE(play_event.IsSignaled());
  controller->Play();
  controller->Pause();
  controller->Play();
  play_event.Wait();

  // Now stop the controller.
  CloseAudioController(controller);
}

}  // namespace media
