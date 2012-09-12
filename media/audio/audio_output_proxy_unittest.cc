// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/audio_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(dalecurtis): Temporarily disabled while switching pipeline to use float,
// http://crbug.com/114700
#if defined(ENABLE_AUDIO_MIXER)
#include "media/audio/audio_output_mixer.h"
#endif

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArrayArgument;
using media::AudioBus;
using media::AudioBuffersState;
using media::AudioInputStream;
using media::AudioManager;
using media::AudioOutputDispatcher;
using media::AudioOutputProxy;
using media::AudioOutputStream;
using media::AudioParameters;

namespace {

static const int kTestCloseDelayMs = 100;

// Used in the test where we don't want a stream to be closed unexpectedly.
static const int kTestBigCloseDelaySeconds = 1000;

// Delay between callbacks to AudioSourceCallback::OnMoreData.
static const int kOnMoreDataCallbackDelayMs = 10;

// Let start run long enough for many OnMoreData callbacks to occur.
static const int kStartRunTimeMs = kOnMoreDataCallbackDelayMs * 10;

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
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD0(GetAudioInputDeviceModel, string16());
  MOCK_METHOD1(MakeAudioOutputStream, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD1(MakeAudioOutputStreamProxy, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD2(MakeAudioInputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));
  MOCK_METHOD0(CanShowAudioInputSettings, bool());
  MOCK_METHOD0(ShowAudioInputSettings, void());
  MOCK_METHOD0(GetMessageLoop, scoped_refptr<base::MessageLoopProxy>());
  MOCK_METHOD1(GetAudioInputDeviceNames, void(
      media::AudioDeviceNames* device_name));
  MOCK_METHOD0(IsRecordingInProcess, bool());
};

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  int OnMoreData(AudioBus* audio_bus, AudioBuffersState buffers_state) {
    audio_bus->Zero();
    return audio_bus->frames();
  }
  MOCK_METHOD3(OnMoreIOData, int(AudioBus* source,
                                 AudioBus* dest,
                                 AudioBuffersState buffers_state));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

// Simple class for repeatedly calling OnMoreData() to expose shutdown issues
// with AudioSourceCallback users.
class AudioThreadRunner : public base::DelegateSimpleThread::Delegate {
 public:
  AudioThreadRunner(AudioOutputStream::AudioSourceCallback* callback,
                   base::TimeDelta delay, const AudioParameters& params)
      : delay_(delay),
        callback_(callback),
        bus_(media::AudioBus::Create(params)),
        running_(true) {
    CHECK(delay_ > base::TimeDelta());
  }

  virtual void Run() {
    while (true) {
      {
        base::AutoLock auto_lock(lock_);
        if (!running_)
          return;
      }
      base::PlatformThread::Sleep(delay_);
      callback_->OnMoreData(bus_.get(), AudioBuffersState());
    }
  }

  void Stop() {
    base::AutoLock auto_lock(lock_);
    running_ = false;
  }

 private:
  base::TimeDelta delay_;
  AudioOutputStream::AudioSourceCallback* callback_;
  scoped_ptr<media::AudioBus> bus_;
  bool running_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AudioThreadRunner);
};

}  // namespace

namespace media {

class AudioOutputProxyTest : public testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_CALL(manager_, GetMessageLoop())
        .WillRepeatedly(Return(message_loop_.message_loop_proxy()));
    InitDispatcher(base::TimeDelta::FromMilliseconds(kTestCloseDelayMs));
  }

  virtual void TearDown() {
    // All paused proxies should have been closed at this point.
    EXPECT_EQ(0u, dispatcher_impl_->paused_proxies_);

    // This is necessary to free all proxy objects that have been
    // closed by the test.
    message_loop_.RunAllPending();
  }

  virtual void InitDispatcher(base::TimeDelta close_delay) {
    params_ = AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                              CHANNEL_LAYOUT_STEREO, 44100, 16, 1024);
    dispatcher_impl_ = new AudioOutputDispatcherImpl(&manager(),
                                                     params_,
                                                     close_delay);
#if defined(ENABLE_AUDIO_MIXER)
    mixer_ = new AudioOutputMixer(&manager(), params_, close_delay);
#endif

    // Necessary to know how long the dispatcher will wait before posting
    // StopStreamTask.
    pause_delay_ = dispatcher_impl_->pause_delay_;
  }

  virtual void OnStart() {}

  MockAudioManager& manager() {
    return manager_;
  }

  // Wait for the close timer to fire.
  void WaitForCloseTimer(const int timer_delay_ms) {
    message_loop_.RunAllPending();  // OpenTask() may reset the timer.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(timer_delay_ms) * 2);
    message_loop_.RunAllPending();
  }

  // Methods that do actual tests.
  void OpenAndClose(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, Close())
        .Times(1);

    AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy->Open());
    proxy->Close();
    WaitForCloseTimer(kTestCloseDelayMs);
  }

  // Create a stream, and then calls Start() and Stop().
  void StartAndStop(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
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

    AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy->Open());

    proxy->Start(&callback_);
    OnStart();
    proxy->Stop();

    proxy->Close();
    WaitForCloseTimer(kTestCloseDelayMs);
  }

  // Verify that the stream is closed after Stop is called.
  void CloseAfterStop(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
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

    AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy->Open());

    proxy->Start(&callback_);
    OnStart();
    proxy->Stop();

    // Wait for StopStream() to post StopStreamTask().
    base::PlatformThread::Sleep(pause_delay_ * 2);
    WaitForCloseTimer(kTestCloseDelayMs);

    // Verify expectation before calling Close().
    Mock::VerifyAndClear(&stream);

    proxy->Close();
  }

  // Create two streams, but don't start them. Only one device must be open.
  void TwoStreams(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, Close())
        .Times(1);

    AudioOutputProxy* proxy1 = new AudioOutputProxy(dispatcher);
    AudioOutputProxy* proxy2 = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy1->Open());
    EXPECT_TRUE(proxy2->Open());
    proxy1->Close();
    proxy2->Close();
    WaitForCloseTimer(kTestCloseDelayMs);
  }

  // Open() method failed.
  void OpenFailed(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(false));
    EXPECT_CALL(stream, Close())
        .Times(1);

    AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher);
    EXPECT_FALSE(proxy->Open());
    proxy->Close();
    WaitForCloseTimer(kTestCloseDelayMs);
  }

  void CreateAndWait(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, Close())
        .Times(1);

    AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy->Open());

    // Simulate a delay.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kTestCloseDelayMs) * 2);
    message_loop_.RunAllPending();

    // Verify expectation before calling Close().
    Mock::VerifyAndClear(&stream);

    proxy->Close();
  }

  void TwoStreams_OnePlaying(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream1;
    MockAudioOutputStream stream2;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
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

    AudioOutputProxy* proxy1 = new AudioOutputProxy(dispatcher);
    AudioOutputProxy* proxy2 = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy1->Open());
    EXPECT_TRUE(proxy2->Open());

    proxy1->Start(&callback_);
    message_loop_.RunAllPending();
    OnStart();
    proxy1->Stop();

    proxy1->Close();
    proxy2->Close();
  }

  void TwoStreams_BothPlaying(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream1;
    MockAudioOutputStream stream2;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
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

    AudioOutputProxy* proxy1 = new AudioOutputProxy(dispatcher);
    AudioOutputProxy* proxy2 = new AudioOutputProxy(dispatcher);
    EXPECT_TRUE(proxy1->Open());
    EXPECT_TRUE(proxy2->Open());

    proxy1->Start(&callback_);
    proxy2->Start(&callback_);
    OnStart();
    proxy1->Stop();
    proxy2->Stop();

    proxy1->Close();
    proxy2->Close();
  }

  void StartFailed(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream;

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, Close())
        .Times(1);

    AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_impl_);
    EXPECT_TRUE(proxy->Open());

    // Simulate a delay.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kTestCloseDelayMs) * 2);
    message_loop_.RunAllPending();

    // Verify expectation before calling Close().
    Mock::VerifyAndClear(&stream);

    // |stream| is closed at this point. Start() should reopen it again.
    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(reinterpret_cast<AudioOutputStream*>(NULL)));

    EXPECT_CALL(callback_, OnError(_, _))
        .Times(1);

    proxy->Start(&callback_);

    Mock::VerifyAndClear(&callback_);

    proxy->Close();
  }

  MessageLoop message_loop_;
  scoped_refptr<AudioOutputDispatcherImpl> dispatcher_impl_;
#if defined(ENABLE_AUDIO_MIXER)
  scoped_refptr<AudioOutputMixer> mixer_;
#endif
  base::TimeDelta pause_delay_;
  MockAudioManager manager_;
  MockAudioSourceCallback callback_;
  AudioParameters params_;
};

class AudioOutputResamplerTest : public AudioOutputProxyTest {
 public:
  virtual void TearDown() {
    AudioOutputProxyTest::TearDown();
    ShutdownAudioThread();
  }

  void StartAudioThread() {
    audio_thread_runner_.reset(new AudioThreadRunner(
        resampler_,
        base::TimeDelta::FromMilliseconds(kOnMoreDataCallbackDelayMs),
        params_));
    audio_thread_.reset(new base::DelegateSimpleThread(
        audio_thread_runner_.get(), "AudioThreadRunner"));
    audio_thread_->Start();
  }

  void ShutdownAudioThread() {
    if (!audio_thread_.get()) {
      ASSERT_FALSE(audio_thread_runner_.get());
      return;
    }
    ASSERT_TRUE(audio_thread_runner_.get());
    audio_thread_runner_->Stop();
    audio_thread_->Join();
  }

  virtual void InitDispatcher(base::TimeDelta close_delay) {
    InitDispatcher(close_delay, AudioParameters::AUDIO_PCM_LINEAR);
  }

  virtual void InitDispatcher(base::TimeDelta close_delay,
                              AudioParameters::Format output_format) {
    AudioOutputProxyTest::InitDispatcher(close_delay);
    // Attempt shutdown of audio thread in case InitDispatcher() was called
    // previously.
    ShutdownAudioThread();
    resampler_params_ = AudioParameters(
        output_format, CHANNEL_LAYOUT_STEREO, 48000, 16, 128);
    resampler_ = new AudioOutputResampler(
        &manager(), params_, resampler_params_, close_delay);
    StartAudioThread();
  }

  virtual void OnStart() {
    // Let start run for a bit.
    message_loop_.RunAllPending();
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kStartRunTimeMs));
  }

 protected:
  AudioParameters resampler_params_;
  scoped_refptr<AudioOutputResampler> resampler_;
  scoped_ptr<AudioThreadRunner> audio_thread_runner_;
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;
};

TEST_F(AudioOutputProxyTest, CreateAndClose) {
  AudioOutputProxy* proxy = new AudioOutputProxy(dispatcher_impl_);
  proxy->Close();
}

#if defined(ENABLE_AUDIO_MIXER)
TEST_F(AudioOutputProxyTest, CreateAndClose_Mixer) {
  AudioOutputProxy* proxy = new AudioOutputProxy(mixer_);
  proxy->Close();
}
#endif

TEST_F(AudioOutputResamplerTest, CreateAndClose) {
  AudioOutputProxy* proxy = new AudioOutputProxy(resampler_);
  proxy->Close();
}

TEST_F(AudioOutputProxyTest, OpenAndClose) {
  OpenAndClose(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
TEST_F(AudioOutputProxyTest, OpenAndClose_Mixer) {
  OpenAndClose(mixer_);
}
#endif

TEST_F(AudioOutputResamplerTest, OpenAndClose) {
  OpenAndClose(resampler_);
}

// Create a stream, and verify that it is closed after kTestCloseDelayMs.
// if it doesn't start playing.
TEST_F(AudioOutputProxyTest, CreateAndWait) {
  CreateAndWait(dispatcher_impl_);
}

// Create a stream, and verify that it is closed after kTestCloseDelayMs.
// if it doesn't start playing.
TEST_F(AudioOutputResamplerTest, CreateAndWait) {
  CreateAndWait(resampler_);
}

TEST_F(AudioOutputProxyTest, StartAndStop) {
  StartAndStop(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
TEST_F(AudioOutputProxyTest, StartAndStop_Mixer) {
  StartAndStop(mixer_);
}
#endif

TEST_F(AudioOutputResamplerTest, StartAndStop) {
  StartAndStop(resampler_);
}

TEST_F(AudioOutputProxyTest, CloseAfterStop) {
  CloseAfterStop(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
TEST_F(AudioOutputProxyTest, CloseAfterStop_Mixer) {
  CloseAfterStop(mixer_);
}
#endif

TEST_F(AudioOutputResamplerTest, CloseAfterStop) {
  CloseAfterStop(resampler_);
}

TEST_F(AudioOutputProxyTest, TwoStreams) {
  TwoStreams(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
TEST_F(AudioOutputProxyTest, TwoStreams_Mixer) {
  TwoStreams(mixer_);
}
#endif

TEST_F(AudioOutputResamplerTest, TwoStreams) {
  TwoStreams(resampler_);
}

// Two streams: verify that second stream is allocated when the first
// starts playing.
TEST_F(AudioOutputProxyTest, TwoStreams_OnePlaying) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestBigCloseDelaySeconds));
  TwoStreams_OnePlaying(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
// Two streams: verify that only one device will be created.
TEST_F(AudioOutputProxyTest, TwoStreams_OnePlaying_Mixer) {
  MockAudioOutputStream stream;

  InitDispatcher(base::TimeDelta::FromMilliseconds(kTestCloseDelayMs));

  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
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

  AudioOutputProxy* proxy1 = new AudioOutputProxy(mixer_);
  AudioOutputProxy* proxy2 = new AudioOutputProxy(mixer_);
  EXPECT_TRUE(proxy1->Open());
  EXPECT_TRUE(proxy2->Open());

  proxy1->Start(&callback_);
  proxy1->Stop();

  proxy1->Close();
  proxy2->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}
#endif

TEST_F(AudioOutputResamplerTest, TwoStreams_OnePlaying) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestBigCloseDelaySeconds));
  TwoStreams_OnePlaying(resampler_);
}

// Two streams, both are playing. Dispatcher should not open a third stream.
TEST_F(AudioOutputProxyTest, TwoStreams_BothPlaying) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestBigCloseDelaySeconds));
  TwoStreams_BothPlaying(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
// Two streams, both are playing. Still have to use single device.
// Also verifies that every proxy stream gets its own pending_bytes.
TEST_F(AudioOutputProxyTest, TwoStreams_BothPlaying_Mixer) {
  MockAudioOutputStream stream;

  InitDispatcher(base::TimeDelta::FromMilliseconds(kTestCloseDelayMs));

  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
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

  AudioOutputProxy* proxy1 = new AudioOutputProxy(mixer_);
  AudioOutputProxy* proxy2 = new AudioOutputProxy(mixer_);
  EXPECT_TRUE(proxy1->Open());
  EXPECT_TRUE(proxy2->Open());

  proxy1->Start(&callback_);

  // Mute the proxy. Resulting stream should still have correct length.
  proxy1->SetVolume(0.0);

  uint8 zeroes[4] = {0, 0, 0, 0};
  uint8 buf1[4] = {0};
  EXPECT_CALL(callback_,
      OnMoreData(NotNull(), 4,
                 AllOf(Field(&AudioBuffersState::pending_bytes, 0),
                       Field(&AudioBuffersState::hardware_delay_bytes, 0))))
      .WillOnce(DoAll(SetArrayArgument<0>(zeroes, zeroes + sizeof(zeroes)),
                      Return(4)));
  mixer_->OnMoreData(buf1, sizeof(buf1), AudioBuffersState(0, 0));
  proxy2->Start(&callback_);
  uint8 buf2[4] = {0};
  EXPECT_CALL(callback_,
      OnMoreData(NotNull(), 4,
                 AllOf(Field(&AudioBuffersState::pending_bytes, 4),
                       Field(&AudioBuffersState::hardware_delay_bytes, 0))))
      .WillOnce(DoAll(SetArrayArgument<0>(zeroes, zeroes + sizeof(zeroes)),
                      Return(4)));
  EXPECT_CALL(callback_,
      OnMoreData(NotNull(), 4,
                 AllOf(Field(&AudioBuffersState::pending_bytes, 0),
                       Field(&AudioBuffersState::hardware_delay_bytes, 0))))
      .WillOnce(DoAll(SetArrayArgument<0>(zeroes, zeroes + sizeof(zeroes)),
                      Return(4)));
  mixer_->OnMoreData(buf2, sizeof(buf2), AudioBuffersState(4, 0));
  proxy1->Stop();
  proxy2->Stop();

  proxy1->Close();
  proxy2->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}
#endif

TEST_F(AudioOutputResamplerTest, TwoStreams_BothPlaying) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestBigCloseDelaySeconds));
  TwoStreams_BothPlaying(resampler_);
}

TEST_F(AudioOutputProxyTest, OpenFailed) {
  OpenFailed(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
TEST_F(AudioOutputProxyTest, OpenFailed_Mixer) {
  OpenFailed(mixer_);
}
#endif

TEST_F(AudioOutputResamplerTest, OpenFailed) {
  OpenFailed(resampler_);
}

// Start() method failed.
TEST_F(AudioOutputProxyTest, StartFailed) {
  StartFailed(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
// Start() method failed.
TEST_F(AudioOutputProxyTest, StartFailed_Mixer) {
  MockAudioOutputStream stream;

  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Close())
      .Times(1);
  EXPECT_CALL(stream, Start(_))
      .Times(1);
  EXPECT_CALL(stream, SetVolume(_))
      .Times(1);
  EXPECT_CALL(stream, Stop())
      .Times(1);

  AudioOutputProxy* proxy1 = new AudioOutputProxy(mixer_);
  AudioOutputProxy* proxy2 = new AudioOutputProxy(mixer_);
  EXPECT_TRUE(proxy1->Open());
  EXPECT_TRUE(proxy2->Open());
  proxy1->Start(&callback_);
  proxy1->Stop();
  proxy1->Close();
  WaitForCloseTimer(kTestCloseDelayMs);

  // Verify expectation before continueing.
  Mock::VerifyAndClear(&stream);

  // |stream| is closed at this point. Start() should reopen it again.
  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .WillOnce(Return(reinterpret_cast<AudioOutputStream*>(NULL)));

  EXPECT_CALL(callback_, OnError(_, _))
      .Times(1);

  proxy2->Start(&callback_);

  Mock::VerifyAndClear(&callback_);

  proxy2->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}
#endif

TEST_F(AudioOutputResamplerTest, StartFailed) {
  StartFailed(resampler_);
}

// Simulate AudioOutputStream::Create() failure with a low latency stream and
// ensure AudioOutputResampler falls back to the high latency path.
TEST_F(AudioOutputResamplerTest, LowLatencyCreateFailedFallback) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestCloseDelayMs),
                 AudioParameters::AUDIO_PCM_LOW_LATENCY);

  MockAudioOutputStream stream;
  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .Times(2)
      .WillOnce(Return(static_cast<AudioOutputStream*>(NULL)))
      .WillRepeatedly(Return(&stream));
  EXPECT_CALL(stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(resampler_);
  EXPECT_TRUE(proxy->Open());
  proxy->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}

// Simulate AudioOutputStream::Open() failure with a low latency stream and
// ensure AudioOutputResampler falls back to the high latency path.
TEST_F(AudioOutputResamplerTest, LowLatencyOpenFailedFallback) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestCloseDelayMs),
                 AudioParameters::AUDIO_PCM_LOW_LATENCY);

  MockAudioOutputStream failed_stream;
  MockAudioOutputStream okay_stream;
  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .Times(2)
      .WillOnce(Return(&failed_stream))
      .WillRepeatedly(Return(&okay_stream));
  EXPECT_CALL(failed_stream, Open())
      .WillOnce(Return(false));
  EXPECT_CALL(failed_stream, Close())
      .Times(1);
  EXPECT_CALL(okay_stream, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(okay_stream, Close())
      .Times(1);

  AudioOutputProxy* proxy = new AudioOutputProxy(resampler_);
  EXPECT_TRUE(proxy->Open());
  proxy->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}

// Simulate failures to open both the low latency and the fallback high latency
// stream and ensure AudioOutputResampler terminates normally.
TEST_F(AudioOutputResamplerTest, LowLatencyFallbackFailed) {
  InitDispatcher(base::TimeDelta::FromSeconds(kTestCloseDelayMs),
                 AudioParameters::AUDIO_PCM_LOW_LATENCY);

  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .Times(2)
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));

  AudioOutputProxy* proxy = new AudioOutputProxy(resampler_);
  EXPECT_FALSE(proxy->Open());
  proxy->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}

}  // namespace media
