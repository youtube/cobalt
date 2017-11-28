// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/audio/linux/audio_manager_linux.h"
#include "media/audio/linux/cras_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace media {

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD2(OnMoreData, int(AudioBus* audio_bus,
                               AudioBuffersState buffers_state));
  MOCK_METHOD3(OnMoreIOData, int(AudioBus* source,
                                 AudioBus* dest,
                                 AudioBuffersState buffers_state));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

class MockAudioManagerLinux : public AudioManagerLinux {
 public:
  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD1(MakeLinearOutputStream, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD1(MakeLowLatencyOutputStream, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD2(MakeLinearOutputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));
  MOCK_METHOD2(MakeLowLatencyInputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));

  // We need to override this function in order to skip the checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioOutputStream, and we don't use
  // MakeAudioOutputStream to create the stream in the tests.
  virtual void ReleaseOutputStream(AudioOutputStream* stream) override {
    DCHECK(stream);
    delete stream;
  }

  // We don't mock this method since all tests will do the same thing
  // and use the current message loop.
  virtual scoped_refptr<base::MessageLoopProxy> GetMessageLoop() override {
    return MessageLoop::current()->message_loop_proxy();
  }
};

class CrasOutputStreamTest : public testing::Test {
 protected:
  CrasOutputStreamTest() {
    mock_manager_.reset(new StrictMock<MockAudioManagerLinux>());
  }

  virtual ~CrasOutputStreamTest() {
  }

  CrasOutputStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  CrasOutputStream* CreateStream(ChannelLayout layout,
                                 int32 samples_per_packet) {
    AudioParameters params(kTestFormat, layout, kTestSampleRate,
                           kTestBitsPerSample, samples_per_packet);
    return new CrasOutputStream(params,
                                mock_manager_.get());
  }

  MockAudioManagerLinux& mock_manager() {
    return *(mock_manager_.get());
  }

  static const ChannelLayout kTestChannelLayout;
  static const int kTestSampleRate;
  static const int kTestBitsPerSample;
  static const int kTestBytesPerFrame;
  static const AudioParameters::Format kTestFormat;
  static const uint32 kTestFramesPerPacket;
  static const uint32 kTestPacketSize;
  static struct cras_audio_format* const kFakeAudioFormat;
  static struct cras_stream_params* const kFakeStreamParams;
  static struct cras_client* const kFakeClient;

  scoped_ptr<StrictMock<MockAudioManagerLinux> > mock_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasOutputStreamTest);
};

const ChannelLayout CrasOutputStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const int CrasOutputStreamTest::kTestSampleRate =
    AudioParameters::kAudioCDSampleRate;
const int CrasOutputStreamTest::kTestBitsPerSample = 16;
const int CrasOutputStreamTest::kTestBytesPerFrame =
    CrasOutputStreamTest::kTestBitsPerSample / 8 *
    ChannelLayoutToChannelCount(CrasOutputStreamTest::kTestChannelLayout);
const AudioParameters::Format CrasOutputStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const uint32 CrasOutputStreamTest::kTestFramesPerPacket = 1000;
const uint32 CrasOutputStreamTest::kTestPacketSize =
    CrasOutputStreamTest::kTestFramesPerPacket *
    CrasOutputStreamTest::kTestBytesPerFrame;
struct cras_audio_format* const CrasOutputStreamTest::kFakeAudioFormat =
    reinterpret_cast<struct cras_audio_format*>(1);
struct cras_stream_params* const CrasOutputStreamTest::kFakeStreamParams =
    reinterpret_cast<struct cras_stream_params*>(1);
struct cras_client* const CrasOutputStreamTest::kFakeClient =
    reinterpret_cast<struct cras_client*>(1);

TEST_F(CrasOutputStreamTest, ConstructedState) {
  // Should support mono.
  CrasOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_EQ(CrasOutputStream::kCreated, test_stream->state());
  test_stream->Close();

  // Should support stereo.
  test_stream = CreateStream(CHANNEL_LAYOUT_SURROUND);
  EXPECT_EQ(CrasOutputStream::kCreated, test_stream->state());
  test_stream->Close();

  // Bad bits per sample.
  AudioParameters bad_bps_params(kTestFormat, kTestChannelLayout,
                                 kTestSampleRate, kTestBitsPerSample - 1,
                                 kTestFramesPerPacket);
  test_stream = new CrasOutputStream(bad_bps_params, mock_manager_.get());
  EXPECT_EQ(CrasOutputStream::kInError, test_stream->state());
  test_stream->Close();

  // Bad format.
  AudioParameters bad_format_params(AudioParameters::AUDIO_LAST_FORMAT,
                                    kTestChannelLayout, kTestSampleRate,
                                    kTestBitsPerSample, kTestFramesPerPacket);
  test_stream = new CrasOutputStream(bad_format_params, mock_manager_.get());
  EXPECT_EQ(CrasOutputStream::kInError, test_stream->state());
  test_stream->Close();

  // Bad sample rate.
  AudioParameters bad_rate_params(kTestFormat, kTestChannelLayout,
                                  0, kTestBitsPerSample, kTestFramesPerPacket);
  test_stream = new CrasOutputStream(bad_rate_params, mock_manager_.get());
  EXPECT_EQ(CrasOutputStream::kInError, test_stream->state());
  test_stream->Close();
}

TEST_F(CrasOutputStreamTest, OpenClose) {
  CrasOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  // Open the stream.
  ASSERT_TRUE(test_stream->Open());
  EXPECT_EQ(CrasOutputStream::kIsOpened, test_stream->state());

  // Close the stream.
  test_stream->Close();
}

TEST_F(CrasOutputStreamTest, StartFailBeforeOpen) {
  CrasOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  MockAudioSourceCallback mock_callback;

  test_stream->Start(&mock_callback);
  EXPECT_EQ(CrasOutputStream::kInError, test_stream->state());
}

TEST_F(CrasOutputStreamTest, StartStop) {
  CrasOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  MockAudioSourceCallback mock_callback;

  // Open the stream.
  ASSERT_TRUE(test_stream->Open());
  EXPECT_EQ(CrasOutputStream::kIsOpened, test_stream->state());

  // Start.
  test_stream->Start(&mock_callback);
  EXPECT_EQ(CrasOutputStream::kIsPlaying, test_stream->state());

  // Stop.
  test_stream->Stop();
  EXPECT_EQ(CrasOutputStream::kIsStopped, test_stream->state());

  // Close the stream.
  test_stream->Close();
}

TEST_F(CrasOutputStreamTest, RenderFrames) {
  CrasOutputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  MockAudioSourceCallback mock_callback;

  // Open the stream.
  ASSERT_TRUE(test_stream->Open());
  EXPECT_EQ(CrasOutputStream::kIsOpened, test_stream->state());

  // Render Callback.
  EXPECT_CALL(mock_callback, OnMoreData(_, _))
      .WillRepeatedly(Return(kTestFramesPerPacket));

  // Start.
  test_stream->Start(&mock_callback);
  EXPECT_EQ(CrasOutputStream::kIsPlaying, test_stream->state());

  // Stop.
  test_stream->Stop();
  EXPECT_EQ(CrasOutputStream::kIsStopped, test_stream->state());

  // Close the stream.
  test_stream->Close();
}

}  // namespace media
