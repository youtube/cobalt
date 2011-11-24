// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

static const int kTestCloseDelayMs = 100;

// Used in the test where we don't want a stream to be closed unexpectedly.
static const int kTestBigCloseDelayMs = 1000 * 1000;

class MockAudioOutputStream : public AudioOutputStream {
 public:
  MockAudioOutputStream() {}

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(Start, void(AudioSourceCallback* callback));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());
};

class MockAudioManager : public AudioManager {
 public:
  MockAudioManager() {}

  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(Cleanup, void());
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD0(GetAudioInputDeviceModel, string16());
  MOCK_METHOD1(MakeAudioOutputStream, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD1(MakeAudioOutputStreamProxy, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD2(MakeAudioInputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));
  MOCK_METHOD0(MuteAll, void());
  MOCK_METHOD0(UnMuteAll, void());
  MOCK_METHOD0(CanShowAudioInputSettings, bool());
  MOCK_METHOD0(ShowAudioInputSettings, void());
  MOCK_METHOD0(GetMessageLoop, MessageLoop*());
  MOCK_METHOD1(GetAudioInputDeviceNames, void(
      media::AudioDeviceNames* device_name));
  MOCK_METHOD0(IsRecordingInProcess, bool());
};

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD4(OnMoreData, uint32(AudioOutputStream* stream,
                                  uint8* dest, uint32 max_size,
                                  AudioBuffersState buffers_state));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

class AudioOutputProxyTest : public testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_CALL(manager_, GetMessageLoop())
        .WillRepeatedly(Return(&message_loop_));
    InitDispatcher(kTestCloseDelayMs);
  }

  virtual void TearDown() {
    // All paused proxies should have been closed at this point.
    EXPECT_EQ(0u, dispatcher_->paused_proxies_);

    // This is necessary to free all proxy objects that have been
    // closed by the test.
    message_loop_.RunAllPending();
  }

  void InitDispatcher(int close_delay_ms) {
    AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                           CHANNEL_LAYOUT_STEREO, 44100, 16, 1024);
    dispatcher_ = new AudioOutputDispatcher(&manager_, params, close_delay_ms);

    // Necessary to know how long the dispatcher will wait before posting
    // StopStreamTask.
    pause_delay_milliseconds_ = dispatcher_->pause_delay_milliseconds_;
  }

  MessageLoop message_loop_;
  scoped_refptr<AudioOutputDispatcher> dispatcher_;
  int64 pause_delay_milliseconds_;
  MockAudioManager manager_;
  MockAudioSourceCallback callback_;
};

TEST_F(AudioOutputProxyTest, CreateAndClose) {
  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  proxy->Close();
}

TEST_F(AudioOutputProxyTest, OpenAndClose) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy->Open());
  proxy->Close();
}

// Create a stream, and verify that it is closed after kTestCloseDelayMs.
// if it doesn't start playing.
TEST_F(AudioOutputProxyTest, CreateAndWait) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy->Open());

  // Simulate a delay.
  base::PlatformThread::Sleep(kTestCloseDelayMs * 2);
  message_loop_.RunAllPending();

  // Verify expectation before calling Close().
  Mock::VerifyAndClear(&stream);

  proxy->Close();
}

// Create a stream, and then calls Start() and Stop().
TEST_F(AudioOutputProxyTest, StartAndStop) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Start(_))
      .Times(1);
  EXPECT_CALL(stream, SetVolume(_))
      .Times(1);
  EXPECT_CALL(stream, Stop())
      .Times(1);
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy->Open());

  proxy->Start(&callback_);
  proxy->Stop();

  proxy->Close();
}

// Verify that the stream is closed after Stop is called.
TEST_F(AudioOutputProxyTest, CloseAfterStop) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Start(_))
      .Times(1);
  EXPECT_CALL(stream, SetVolume(_))
      .Times(1);
  EXPECT_CALL(stream, Stop())
      .Times(1);
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy->Open());

  proxy->Start(&callback_);
  proxy->Stop();

  // Wait for StreamStopped() to post StopStreamTask().
  base::PlatformThread::Sleep(pause_delay_milliseconds_ * 2);
  message_loop_.RunAllPending();

  // Wait for the close timer to fire.
  base::PlatformThread::Sleep(kTestCloseDelayMs * 2);
  message_loop_.RunAllPending();

  // Verify expectation before calling Close().
  Mock::VerifyAndClear(&stream);

  proxy->Close();
}

// Create two streams, but don't start them. Only one device must be open.
TEST_F(AudioOutputProxyTest, TwoStreams) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy1 = new AudioOutputProxy(dispatcher_);
  AudioOutputProxy* proxy2 = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy1->Open());
  EXPECT_TRUE(proxy2->Open());
  proxy1->Close();
  proxy2->Close();
}

// Two streams: verify that second stream is allocated when the first
// starts playing.
TEST_F(AudioOutputProxyTest, TwoStreams_OnePlaying) {
  MockAudioOutputStream stream1;
  MockAudioOutputStream stream2;

  InitDispatcher(kTestBigCloseDelayMs);

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream1))
      .WillOnce(Return(&stream2));

  EXPECT_CALL(stream1, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream1, Start(_))
      .Times(1);
  EXPECT_CALL(stream1, SetVolume(_))
      .Times(1);
  EXPECT_CALL(stream1, Stop())
      .Times(1);
  EXPECT_CALL(stream1, Close())
      .Times(1);

  EXPECT_CALL(stream2, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream2, Close())
      .Times(1);

  AudioOutputProxy* proxy1 = new AudioOutputProxy(dispatcher_);
  AudioOutputProxy* proxy2 = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy1->Open());
  EXPECT_TRUE(proxy2->Open());

  proxy1->Start(&callback_);
  message_loop_.RunAllPending();
  proxy1->Stop();

  proxy1->Close();
  proxy2->Close();
}

// Two streams, both are playing. Dispatcher should not open a third stream.
TEST_F(AudioOutputProxyTest, TwoStreams_BothPlaying) {
  MockAudioOutputStream stream1;
  MockAudioOutputStream stream2;

  InitDispatcher(kTestBigCloseDelayMs);

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream1))
      .WillOnce(Return(&stream2));

  EXPECT_CALL(stream1, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream1, Start(_))
      .Times(1);
  EXPECT_CALL(stream1, SetVolume(_))
      .Times(1);
  EXPECT_CALL(stream1, Stop())
      .Times(1);
  EXPECT_CALL(stream1, Close())
      .Times(1);

  EXPECT_CALL(stream2, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream2, Start(_))
      .Times(1);
  EXPECT_CALL(stream2, SetVolume(_))
      .Times(1);
  EXPECT_CALL(stream2, Stop())
      .Times(1);
  EXPECT_CALL(stream2, Close())
      .Times(1);

  AudioOutputProxy* proxy1 = new AudioOutputProxy(dispatcher_);
  AudioOutputProxy* proxy2 = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy1->Open());
  EXPECT_TRUE(proxy2->Open());

  proxy1->Start(&callback_);
  proxy2->Start(&callback_);
  proxy1->Stop();
  proxy2->Stop();

  proxy1->Close();
  proxy2->Close();
}

// Open() method failed.
TEST_F(AudioOutputProxyTest, OpenFailed) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(false));
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  EXPECT_FALSE(proxy->Open());
  proxy->Close();
}

// Start() method failed.
TEST_F(AudioOutputProxyTest, StartFailed) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_);
  EXPECT_TRUE(proxy->Open());

  // Simulate a delay.
  base::PlatformThread::Sleep(kTestCloseDelayMs * 2);
  message_loop_.RunAllPending();

  // Verify expectation before calling Close().
  Mock::VerifyAndClear(&stream);

  // |stream| is closed at this point. Start() should reopen it again.
  EXPECT_CALL(manager_, MakeAudioOutputStream(_))
      .WillOnce(Return(reinterpret_cast<AudioOutputStream*>(NULL)));

  EXPECT_CALL(callback_, OnError(_, _))
      .Times(1);

  proxy->Start(&callback_);

  Mock::VerifyAndClear(&callback_);

  proxy->Close();
}
