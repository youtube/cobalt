// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/audio/linux/audio_manager_linux.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::StrEq;

class MockAlsaWrapper : public AlsaWrapper {
 public:
  MOCK_METHOD4(PcmOpen, int(snd_pcm_t** handle, const char* name,
                            snd_pcm_stream_t stream, int mode));
  MOCK_METHOD1(PcmClose, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmPrepare, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmDrop, int(snd_pcm_t* handle));
  MOCK_METHOD2(PcmDelay, int(snd_pcm_t* handle, snd_pcm_sframes_t* delay));
  MOCK_METHOD3(PcmWritei, snd_pcm_sframes_t(snd_pcm_t* handle,
                                            const void* buffer,
                                            snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmRecover, int(snd_pcm_t* handle, int err, int silent));
  MOCK_METHOD7(PcmSetParams, int(snd_pcm_t* handle, snd_pcm_format_t format,
                                 snd_pcm_access_t access, unsigned int channels,
                                 unsigned int rate, int soft_resample,
                                 unsigned int latency));
  MOCK_METHOD1(PcmName, const char*(snd_pcm_t* handle));
  MOCK_METHOD1(PcmAvailUpdate, snd_pcm_sframes_t (snd_pcm_t* handle));

  MOCK_METHOD1(StrError, const char*(int errnum));
};

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD4(OnMoreData, size_t(AudioOutputStream* stream, void* dest,
                                  size_t max_size, int pending_bytes));
  MOCK_METHOD1(OnClose, void(AudioOutputStream* stream));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

class MockAudioManagerLinux : public AudioManagerLinux {
 public:
  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(HasAudioDevices, bool());
  MOCK_METHOD4(MakeAudioStream, AudioOutputStream*(Format format, int channels,
                                                   int sample_rate,
                                                   char bits_per_sample));
  MOCK_METHOD0(MuteAll, void());
  MOCK_METHOD0(UnMuteAll, void());

  MOCK_METHOD1(ReleaseStream, void(AlsaPcmOutputStream* stream));
};

class AlsaPcmOutputStreamTest : public testing::Test {
 protected:
  AlsaPcmOutputStreamTest()
      : packet_(kTestPacketSize + 1) {
    test_stream_ = new AlsaPcmOutputStream(kTestDeviceName,
                                           kTestFormat,
                                           kTestChannels,
                                           kTestSampleRate,
                                           kTestBitsPerSample,
                                           &mock_alsa_wrapper_,
                                           &mock_manager_,
                                           &message_loop_);

    packet_.size = kTestPacketSize;
  }

  virtual ~AlsaPcmOutputStreamTest() {
    test_stream_ = NULL;
  }

  static const int kTestChannels;
  static const int kTestSampleRate;
  static const int kTestBitsPerSample;
  static const int kTestBytesPerFrame;
  static const AudioManager::Format kTestFormat;
  static const char kTestDeviceName[];
  static const char kDummyMessage[];
  static const int kTestFramesPerPacket;
  static const size_t kTestPacketSize;
  static const int kTestFailedErrno;
  static snd_pcm_t* const kFakeHandle;

  StrictMock<MockAlsaWrapper> mock_alsa_wrapper_;
  StrictMock<MockAudioManagerLinux> mock_manager_;
  MessageLoop message_loop_;
  scoped_refptr<AlsaPcmOutputStream> test_stream_;
  AlsaPcmOutputStream::Packet packet_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AlsaPcmOutputStreamTest);
};

const int AlsaPcmOutputStreamTest::kTestChannels = 2;
const int AlsaPcmOutputStreamTest::kTestSampleRate =
    AudioManager::kAudioCDSampleRate;
const int AlsaPcmOutputStreamTest::kTestBitsPerSample = 8;
const int AlsaPcmOutputStreamTest::kTestBytesPerFrame =
    AlsaPcmOutputStreamTest::kTestBitsPerSample / 8 *
    AlsaPcmOutputStreamTest::kTestChannels;
const AudioManager::Format AlsaPcmOutputStreamTest::kTestFormat =
    AudioManager::AUDIO_PCM_LINEAR;
const char AlsaPcmOutputStreamTest::kTestDeviceName[] = "TestDevice";
const char AlsaPcmOutputStreamTest::kDummyMessage[] = "dummy";
const int AlsaPcmOutputStreamTest::kTestFramesPerPacket = 100;
const size_t AlsaPcmOutputStreamTest::kTestPacketSize =
    AlsaPcmOutputStreamTest::kTestFramesPerPacket *
    AlsaPcmOutputStreamTest::kTestBytesPerFrame;
const int AlsaPcmOutputStreamTest::kTestFailedErrno = -EACCES;
snd_pcm_t* const AlsaPcmOutputStreamTest::kFakeHandle =
    reinterpret_cast<snd_pcm_t*>(1);

TEST_F(AlsaPcmOutputStreamTest, ConstructedState) {
  EXPECT_EQ(AlsaPcmOutputStream::kCreated,
            test_stream_->shared_data_.state());

  // Should support mono.
  test_stream_ = new AlsaPcmOutputStream(kTestDeviceName,
                                         kTestFormat,
                                         1,  // Channels.
                                         kTestSampleRate,
                                         kTestBitsPerSample,
                                         &mock_alsa_wrapper_,
                                         &mock_manager_,
                                         &message_loop_);
  EXPECT_EQ(AlsaPcmOutputStream::kCreated,
            test_stream_->shared_data_.state());

  // Should not support multi-channel.
  test_stream_ = new AlsaPcmOutputStream(kTestDeviceName,
                                         kTestFormat,
                                         3,  // Channels.
                                         kTestSampleRate,
                                         kTestBitsPerSample,
                                         &mock_alsa_wrapper_,
                                         &mock_manager_,
                                         &message_loop_);
  EXPECT_EQ(AlsaPcmOutputStream::kInError,
            test_stream_->shared_data_.state());

  // Bad bits per sample.
  test_stream_ = new AlsaPcmOutputStream(kTestDeviceName,
                                         kTestFormat,
                                         kTestChannels,
                                         kTestSampleRate,
                                         kTestBitsPerSample - 1,
                                         &mock_alsa_wrapper_,
                                         &mock_manager_,
                                         &message_loop_);
  EXPECT_EQ(AlsaPcmOutputStream::kInError,
            test_stream_->shared_data_.state());

  // Bad format.
  test_stream_ = new AlsaPcmOutputStream(kTestDeviceName,
                                         AudioManager::AUDIO_PCM_DELTA,
                                         kTestChannels,
                                         kTestSampleRate,
                                         kTestBitsPerSample,
                                         &mock_alsa_wrapper_,
                                         &mock_manager_,
                                         &message_loop_);
  EXPECT_EQ(AlsaPcmOutputStream::kInError,
            test_stream_->shared_data_.state());
}

TEST_F(AlsaPcmOutputStreamTest, OpenClose) {
  int64 expected_micros = 2 *
      AlsaPcmOutputStream::FramesToMicros(kTestPacketSize / kTestBytesPerFrame,
                                          kTestSampleRate);

  // Open() call opens the playback device, sets the parameters, posts a task
  // with the resulting configuration data, and transitions the object state to
  // kIsOpened.
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmOpen(_, StrEq(kTestDeviceName),
                      SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle),
                      Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(kFakeHandle,
                           SND_PCM_FORMAT_S8,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           kTestChannels,
                           kTestSampleRate,
                           1,
                           expected_micros))
      .WillOnce(Return(0));

  // Open the stream.
  ASSERT_TRUE(test_stream_->Open(kTestPacketSize));
  message_loop_.RunAllPending();

  EXPECT_EQ(AlsaPcmOutputStream::kIsOpened,
            test_stream_->shared_data_.state());
  EXPECT_EQ(kFakeHandle, test_stream_->playback_handle_);
  EXPECT_EQ(kTestFramesPerPacket, test_stream_->frames_per_packet_);
  EXPECT_TRUE(test_stream_->packet_.get());
  EXPECT_FALSE(test_stream_->stop_stream_);

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_manager_, ReleaseStream(test_stream_.get()));
  test_stream_->Close();
  message_loop_.RunAllPending();

  EXPECT_TRUE(NULL == test_stream_->playback_handle_);
  EXPECT_FALSE(test_stream_->packet_.get());
  EXPECT_TRUE(test_stream_->stop_stream_);
}

TEST_F(AlsaPcmOutputStreamTest, PcmOpenFailed) {
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  // If open fails, the stream stays in kCreated because it has effectively had
  // no changes.
  ASSERT_FALSE(test_stream_->Open(kTestPacketSize));
  EXPECT_EQ(AlsaPcmOutputStream::kCreated,
            test_stream_->shared_data_.state());
}

TEST_F(AlsaPcmOutputStreamTest, PcmSetParamsFailed) {
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle),
                      Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  // If open fails, the stream stays in kCreated because it has effectively had
  // no changes.
  ASSERT_FALSE(test_stream_->Open(kTestPacketSize));
  EXPECT_EQ(AlsaPcmOutputStream::kCreated,
            test_stream_->shared_data_.state());
}

TEST_F(AlsaPcmOutputStreamTest, StartStop) {
  // Open() call opens the playback device, sets the parameters, posts a task
  // with the resulting configuration data, and transitions the object state to
  // kIsOpened.
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle),
                      Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(0));

  // Open the stream.
  ASSERT_TRUE(test_stream_->Open(kTestPacketSize));
  message_loop_.RunAllPending();

  // Expect Device setup.
  EXPECT_CALL(mock_alsa_wrapper_, PcmDrop(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmPrepare(kFakeHandle))
      .WillOnce(Return(0));

  // Expect the pre-roll.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(kFakeHandle, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(0), Return(0)));
  EXPECT_CALL(mock_callback,
              OnMoreData(test_stream_.get(), _, kTestPacketSize, 0))
      .Times(2)
      .WillRepeatedly(Return(kTestPacketSize));
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(kFakeHandle, _, _))
      .Times(2)
      .WillRepeatedly(Return(kTestPacketSize));

  // Expect scheduling.
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(kFakeHandle))
      .WillOnce(Return(1));

  test_stream_->Start(&mock_callback);
  message_loop_.RunAllPending();

  EXPECT_CALL(mock_manager_, ReleaseStream(test_stream_.get()));
  EXPECT_CALL(mock_callback, OnClose(test_stream_.get()));
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  test_stream_->Close();
  message_loop_.RunAllPending();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_FinishedPacket) {
  // Nothing should happen.  Don't set any expectations and Our strict mocks
  // should verify most of this.

  // Test regular used-up packet.
  packet_.used = packet_.size;
  test_stream_->WritePacket(&packet_);

  // Test empty packet.
  packet_.used = packet_.size = 0;
  test_stream_->WritePacket(&packet_);
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_NormalPacket) {
  // Write a little less than half the data.
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(_, packet_.buffer.get(), _))
      .WillOnce(Return(packet_.size / kTestBytesPerFrame / 2 - 1));

  test_stream_->WritePacket(&packet_);

  ASSERT_EQ(packet_.size / 2 - kTestBytesPerFrame, packet_.used);

  // Write the rest.
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmWritei(_, packet_.buffer.get() + packet_.used, _))
      .WillOnce(Return(packet_.size / kTestBytesPerFrame / 2 + 1));
  test_stream_->WritePacket(&packet_);
  EXPECT_EQ(packet_.size, packet_.used);
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_WriteFails) {
  // Fail due to a recoverable error and see that PcmRecover code path
  // continues normally.
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(_, _, _))
      .WillOnce(Return(-EINTR));
  EXPECT_CALL(mock_alsa_wrapper_, PcmRecover(_, _, _))
      .WillOnce(Return(packet_.size / kTestBytesPerFrame / 2 - 1));

  test_stream_->WritePacket(&packet_);

  ASSERT_EQ(packet_.size / 2 - kTestBytesPerFrame, packet_.used);

  // Fail the next write, and see that stop_stream_ is set.
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(_, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmRecover(_, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));
  test_stream_->WritePacket(&packet_);
  EXPECT_EQ(packet_.size / 2 - kTestBytesPerFrame, packet_.used);
  EXPECT_TRUE(test_stream_->stop_stream_);
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_StopStream) {
  // No expectations set on the strict mock because nothing should be called.
  test_stream_->stop_stream_ = true;
  test_stream_->WritePacket(&packet_);
  EXPECT_EQ(packet_.size, packet_.used);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket) {
  packet_.used = packet_.size;

  // Return a partially filled packet.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(1), Return(0)));
  EXPECT_CALL(mock_callback,
              OnMoreData(test_stream_.get(), packet_.buffer.get(),
                         packet_.capacity, kTestBytesPerFrame))
      .WillOnce(Return(10));

  test_stream_->shared_data_.set_source_callback(&mock_callback);
  test_stream_->BufferPacket(&packet_);

  EXPECT_EQ(0u, packet_.used);
  EXPECT_EQ(10u, packet_.size);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_UnfinishedPacket) {
  // No expectations set on the strict mock because nothing should be called.
  test_stream_->BufferPacket(&packet_);
  EXPECT_EQ(0u, packet_.used);
  EXPECT_EQ(kTestPacketSize, packet_.size);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_StopStream) {
  test_stream_->stop_stream_ = true;
  test_stream_->BufferPacket(&packet_);
  EXPECT_EQ(0u, packet_.used);
  EXPECT_EQ(0u, packet_.size);
}

TEST_F(AlsaPcmOutputStreamTest, ScheduleNextWrite) {
  test_stream_->shared_data_.TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream_->shared_data_.TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillOnce(Return(10));
  test_stream_->ScheduleNextWrite(&packet_);

  test_stream_->shared_data_.TransitionTo(AlsaPcmOutputStream::kIsClosed);
}

TEST_F(AlsaPcmOutputStreamTest, ScheduleNextWrite_StopStream) {
  test_stream_->shared_data_.TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream_->shared_data_.TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  test_stream_->stop_stream_ = true;
  test_stream_->ScheduleNextWrite(&packet_);

  // TODO(ajwong): Find a way to test whether or not another task has been
  // posted so we can verify that the Alsa code will indeed break the task
  // posting loop.

  test_stream_->shared_data_.TransitionTo(AlsaPcmOutputStream::kIsClosed);
}
