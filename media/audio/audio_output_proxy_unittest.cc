// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "media/audio/audio_output_dispatcher_impl.h"
#include "media/audio/audio_output_proxy.h"
#include "media/audio/audio_output_resampler.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
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

class MockAudioOutputStream : public AudioOutputStream {
 public:
  explicit MockAudioOutputStream(const AudioParameters& params)
      : start_called_(false),
        stop_called_(false),
        params_(params) {
  }

  void Start(AudioSourceCallback* callback) {
    start_called_ = true;
    audio_thread_runner_.reset(new AudioThreadRunner(
        callback,
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
    audio_thread_runner_.reset();
    audio_thread_.reset();
  }

  void Stop() {
    stop_called_ = true;
    ShutdownAudioThread();
  }

  ~MockAudioOutputStream() {
    ShutdownAudioThread();
  }

  bool start_called() { return start_called_; }
  bool stop_called() { return stop_called_; }

  MOCK_METHOD0(Open, bool());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));
  MOCK_METHOD0(Close, void());

 private:
  bool start_called_;
  bool stop_called_;
  AudioParameters params_;
  scoped_ptr<AudioThreadRunner> audio_thread_runner_;
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;
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
  int OnMoreIOData(AudioBus* source, AudioBus* dest,
                   AudioBuffersState buffers_state) {
    return OnMoreData(dest, buffers_state);
  }
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
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
    MockAudioOutputStream stream(params_);

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
    MockAudioOutputStream stream(params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, SetVolume(_))
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
    EXPECT_TRUE(stream.stop_called());
    EXPECT_TRUE(stream.start_called());
  }

  // Verify that the stream is closed after Stop is called.
  void CloseAfterStop(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream));
    EXPECT_CALL(stream, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream, SetVolume(_))
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
    EXPECT_TRUE(stream.stop_called());
    EXPECT_TRUE(stream.start_called());
  }

  // Create two streams, but don't start them. Only one device must be open.
  void TwoStreams(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(params_);

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
    EXPECT_FALSE(stream.stop_called());
    EXPECT_FALSE(stream.start_called());
  }

  // Open() method failed.
  void OpenFailed(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(params_);

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
    EXPECT_FALSE(stream.stop_called());
    EXPECT_FALSE(stream.start_called());
  }

  void CreateAndWait(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(params_);

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
    EXPECT_FALSE(stream.stop_called());
    EXPECT_FALSE(stream.start_called());
  }

  void TwoStreams_OnePlaying(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream1(params_);
    MockAudioOutputStream stream2(params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream1))
        .WillOnce(Return(&stream2));

    EXPECT_CALL(stream1, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream1, SetVolume(_))
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
    EXPECT_TRUE(stream1.stop_called());
    EXPECT_TRUE(stream1.start_called());
    EXPECT_FALSE(stream2.stop_called());
    EXPECT_FALSE(stream2.start_called());
  }

  void TwoStreams_BothPlaying(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream1(params_);
    MockAudioOutputStream stream2(params_);

    EXPECT_CALL(manager(), MakeAudioOutputStream(_))
        .WillOnce(Return(&stream1))
        .WillOnce(Return(&stream2));

    EXPECT_CALL(stream1, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream1, SetVolume(_))
        .Times(1);
    EXPECT_CALL(stream1, Close())
        .Times(1);

    EXPECT_CALL(stream2, Open())
        .WillOnce(Return(true));
    EXPECT_CALL(stream2, SetVolume(_))
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
    EXPECT_TRUE(stream1.stop_called());
    EXPECT_TRUE(stream1.start_called());
    EXPECT_TRUE(stream2.stop_called());
    EXPECT_TRUE(stream2.start_called());
  }

  void StartFailed(AudioOutputDispatcher* dispatcher) {
    MockAudioOutputStream stream(params_);

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
  }


  virtual void InitDispatcher(base::TimeDelta close_delay) {
    AudioOutputProxyTest::InitDispatcher(close_delay);
    // Attempt shutdown of audio thread in case InitDispatcher() was called
    // previously.
    resampler_params_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
        48000, 16, 128);
    resampler_ = new AudioOutputResampler(
        &manager(), params_, resampler_params_, close_delay);
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
  MockAudioOutputStream stream(params_);

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
  MockAudioOutputStream stream(params_);

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
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableAudioFallback);
  OpenFailed(resampler_);
}

// Start() method failed.
TEST_F(AudioOutputProxyTest, StartFailed) {
  StartFailed(dispatcher_impl_);
}

#if defined(ENABLE_AUDIO_MIXER)
// Start() method failed.
TEST_F(AudioOutputProxyTest, StartFailed_Mixer) {
  MockAudioOutputStream stream(params_);

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
  MockAudioOutputStream stream(params_);
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
  MockAudioOutputStream failed_stream(params_);
  MockAudioOutputStream okay_stream(params_);
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
  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .Times(2)
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));

  AudioOutputProxy* proxy = new AudioOutputProxy(resampler_);
  EXPECT_FALSE(proxy->Open());
  proxy->Close();
  WaitForCloseTimer(kTestCloseDelayMs);
}

// Simulate an eventual OpenStream() failure; i.e. successful OpenStream() calls
// eventually followed by one which fails; root cause of http://crbug.com/150619
TEST_F(AudioOutputResamplerTest, LowLatencyOpenEventuallyFails) {
  MockAudioOutputStream stream1(params_);
  MockAudioOutputStream stream2(params_);
  MockAudioOutputStream stream3(params_);

  // Setup the mock such that all three streams are successfully created.
  EXPECT_CALL(manager(), MakeAudioOutputStream(_))
      .WillOnce(Return(&stream1))
      .WillOnce(Return(&stream2))
      .WillOnce(Return(&stream3))
      .WillRepeatedly(Return(static_cast<AudioOutputStream*>(NULL)));

  // Stream1 should be able to successfully open and start.
  EXPECT_CALL(stream1, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream1, Close())
      .Times(1);
  EXPECT_CALL(stream1, SetVolume(_))
      .Times(1);

  // Stream2 should also be able to successfully open and start.
  EXPECT_CALL(stream2, Open())
      .WillOnce(Return(true));
  EXPECT_CALL(stream2, Close())
      .Times(1);
  EXPECT_CALL(stream2, SetVolume(_))
      .Times(1);

  // Stream3 should fail on Open() (yet still be closed since
  // MakeAudioOutputStream returned a valid AudioOutputStream object).
  EXPECT_CALL(stream3, Open())
      .WillOnce(Return(false));
  EXPECT_CALL(stream3, Close())
      .Times(1);

  // Open and start the first proxy and stream.
  AudioOutputProxy* proxy1 = new AudioOutputProxy(resampler_);
  EXPECT_TRUE(proxy1->Open());
  proxy1->Start(&callback_);
  OnStart();

  // Open and start the second proxy and stream.
  AudioOutputProxy* proxy2 = new AudioOutputProxy(resampler_);
  EXPECT_TRUE(proxy2->Open());
  proxy2->Start(&callback_);
  OnStart();

  // Attempt to open the third stream which should fail.
  AudioOutputProxy* proxy3 = new AudioOutputProxy(resampler_);
  EXPECT_FALSE(proxy3->Open());

  // Perform the required Stop()/Close() shutdown dance for each proxy.  Under
  // the hood each proxy should correctly call CloseStream() if OpenStream()
  // succeeded or not.
  proxy3->Stop();
  proxy3->Close();
  proxy2->Stop();
  proxy2->Close();
  proxy1->Stop();
  proxy1->Close();

  // Wait for all of the messages to fly and then verify stream behavior.
  WaitForCloseTimer(kTestCloseDelayMs);
  EXPECT_TRUE(stream1.stop_called());
  EXPECT_TRUE(stream1.start_called());
  EXPECT_TRUE(stream2.stop_called());
  EXPECT_TRUE(stream2.start_called());
  EXPECT_FALSE(stream3.stop_called());
  EXPECT_FALSE(stream3.start_called());
}

}  // namespace media
