// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/audio_input_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const int kChannels = 2;
static const int kSamplesPerPacket = kSampleRate / 10;

ACTION_P3(CheckCountAndSignalEvent, count, limit, event) {
  if (++*count >= limit) {
    event->Signal();
  }
}

class MockAudioInputControllerEventHandler
    : public AudioInputController::EventHandler {
 public:
  MockAudioInputControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioInputController* controller));
  MOCK_METHOD1(OnRecording, void(AudioInputController* controller));
  MOCK_METHOD2(OnError, void(AudioInputController* controller, int error_code));
  MOCK_METHOD3(OnData, void(AudioInputController* controller,
                            const uint8* data, uint32 size));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputControllerEventHandler);
};

// Test AudioInputController for create and close without recording audio.
TEST(AudioInputControllerTest, CreateAndClose) {
  MockAudioInputControllerEventHandler event_handler;
  base::WaitableEvent event(false, false);
  // If OnCreated is called then signal the event.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  AudioParameters params(AudioParameters::AUDIO_MOCK, kChannels,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(&event_handler, params);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to be called.
  event.Wait();

  controller->Close();
}

// Test a normal call sequence of create, record and close.
TEST(AudioInputControllerTest, RecordAndClose) {
  MockAudioInputControllerEventHandler event_handler;
  base::WaitableEvent event(false, false);
  int count = 0;

  // If OnCreated is called then signal the event.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  // If OnData is called enough then signal the event.
  EXPECT_CALL(event_handler, OnData(NotNull(), NotNull(), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndSignalEvent(&count, 10, &event));

  AudioParameters params(AudioParameters::AUDIO_MOCK, kChannels,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(&event_handler, params);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to be called.
  event.Wait();
  event.Reset();

  // Play and then wait for the event to be signaled.
  controller->Record();
  event.Wait();

  controller->Close();
}

// Test that AudioInputController rejects insanely large packet sizes.
TEST(AudioInputControllerTest, SamplesPerPacketTooLarge) {
  // Create an audio device with a very large packet size.
  MockAudioInputControllerEventHandler event_handler;

  AudioParameters params(AudioParameters::AUDIO_MOCK, kChannels,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket * 1000);
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      &event_handler, params);
  ASSERT_FALSE(controller);
}

// Test calling AudioInputController::Close multiple times.
TEST(AudioInputControllerTest, CloseTwice) {
  MockAudioInputControllerEventHandler event_handler;
  EXPECT_CALL(event_handler, OnCreated(NotNull()));
  AudioParameters params(AudioParameters::AUDIO_MOCK, kChannels,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);
  scoped_refptr<AudioInputController> controller =
      AudioInputController::Create(&event_handler, params);
  ASSERT_TRUE(controller.get());

  controller->Close();
  controller->Close();
}

}  // namespace media
