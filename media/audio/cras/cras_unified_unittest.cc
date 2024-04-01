// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/components/audio/cras_audio_handler.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/cras/audio_manager_chromeos.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// cras_util.h defines custom min/max macros which break compilation, so ensure
// it's not included until last.  #if avoids presubmit errors.
#if defined(USE_CRAS)
#include "media/audio/cras/cras_unified.h"
#endif

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace media {

class MockAudioManagerCras : public AudioManagerChromeOS {
 public:
  MockAudioManagerCras()
      : AudioManagerChromeOS(std::make_unique<TestAudioThread>(),
                             &fake_audio_log_factory_) {}

  // We need to override this function in order to skip the checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioOutputStream, and we don't use
  // MakeAudioOutputStream to create the stream in the tests.
  void ReleaseOutputStream(AudioOutputStream* stream) override {
    DCHECK(stream);
    delete stream;
  }

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class CrasUnifiedStreamTest : public testing::Test {
 protected:
  CrasUnifiedStreamTest() {
    chromeos::CrasAudioClient::InitializeFake();
    ash::CrasAudioHandler::InitializeForTesting();
    mock_manager_.reset(new StrictMock<MockAudioManagerCras>());
    base::RunLoop().RunUntilIdle();
  }

  ~CrasUnifiedStreamTest() override {
    mock_manager_->Shutdown();
    ash::CrasAudioHandler::Shutdown();
    chromeos::CrasAudioClient::Shutdown();
  }

  CrasUnifiedStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  CrasUnifiedStream* CreateStream(ChannelLayout layout,
                                  int32_t samples_per_packet) {
    AudioParameters params(kTestFormat, layout, kTestSampleRate,
                           samples_per_packet);
    return new CrasUnifiedStream(params, mock_manager_.get(),
                                 AudioDeviceDescription::kDefaultDeviceId);
  }

  MockAudioManagerCras& mock_manager() {
    return *(mock_manager_.get());
  }

  static const ChannelLayout kTestChannelLayout;
  static const int kTestSampleRate;
  static const AudioParameters::Format kTestFormat;
  static const uint32_t kTestFramesPerPacket;

  base::TestMessageLoop message_loop_;
  std::unique_ptr<StrictMock<MockAudioManagerCras>> mock_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasUnifiedStreamTest);
};

const ChannelLayout CrasUnifiedStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const int CrasUnifiedStreamTest::kTestSampleRate =
    AudioParameters::kAudioCDSampleRate;
const AudioParameters::Format CrasUnifiedStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const uint32_t CrasUnifiedStreamTest::kTestFramesPerPacket = 1000;

TEST_F(CrasUnifiedStreamTest, ConstructedState) {
  CrasUnifiedStream* test_stream = CreateStream(kTestChannelLayout);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();

  // Should support mono.
  test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();

  // Should support multi-channel.
  test_stream = CreateStream(CHANNEL_LAYOUT_SURROUND);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();

  // Bad sample rate.
  AudioParameters bad_rate_params(kTestFormat, kTestChannelLayout, 0,
                                  kTestFramesPerPacket);
  test_stream = new CrasUnifiedStream(bad_rate_params, mock_manager_.get(),
                                      AudioDeviceDescription::kDefaultDeviceId);
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasUnifiedStreamTest, RenderFrames) {
  CrasUnifiedStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  MockAudioSourceCallback mock_callback;

  ASSERT_TRUE(test_stream->Open());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(mock_callback, OnMoreData(_, _, 0, _))
      .WillRepeatedly(
          DoAll(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal),
                Return(kTestFramesPerPacket)));

  test_stream->Start(&mock_callback);

  // Wait for samples to be captured.
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));

  test_stream->Stop();

  test_stream->Close();
}

}  // namespace media
