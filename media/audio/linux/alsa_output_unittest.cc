// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"
#include "media/audio/linux/audio_manager_linux.h"
#include "media/base/data_buffer.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::AtLeast;
using testing::DoAll;
using testing::Eq;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::MockFunction;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::StrEq;
using testing::Unused;

class MockAlsaWrapper : public AlsaWrapper {
 public:
  MOCK_METHOD3(DeviceNameHint, int(int card,
                                   const char* iface,
                                   void*** hints));
  MOCK_METHOD2(DeviceNameGetHint, char*(const void* hint, const char* id));
  MOCK_METHOD1(DeviceNameFreeHint, int(void** hints));

  MOCK_METHOD4(PcmOpen, int(snd_pcm_t** handle, const char* name,
                            snd_pcm_stream_t stream, int mode));
  MOCK_METHOD1(PcmClose, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmPrepare, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmDrop, int(snd_pcm_t* handle));
  MOCK_METHOD2(PcmDelay, int(snd_pcm_t* handle, snd_pcm_sframes_t* delay));
  MOCK_METHOD3(PcmWritei, snd_pcm_sframes_t(snd_pcm_t* handle,
                                            const void* buffer,
                                            snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmReadi, snd_pcm_sframes_t(snd_pcm_t* handle,
                                           void* buffer,
                                           snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmRecover, int(snd_pcm_t* handle, int err, int silent));
  MOCK_METHOD7(PcmSetParams, int(snd_pcm_t* handle, snd_pcm_format_t format,
                                 snd_pcm_access_t access, unsigned int channels,
                                 unsigned int rate, int soft_resample,
                                 unsigned int latency));
  MOCK_METHOD3(PcmGetParams, int(snd_pcm_t* handle,
                                 snd_pcm_uframes_t* buffer_size,
                                 snd_pcm_uframes_t* period_size));
  MOCK_METHOD1(PcmName, const char*(snd_pcm_t* handle));
  MOCK_METHOD1(PcmAvailUpdate, snd_pcm_sframes_t(snd_pcm_t* handle));
  MOCK_METHOD1(PcmState, snd_pcm_state_t(snd_pcm_t* handle));
  MOCK_METHOD1(PcmStart, int(snd_pcm_t* handle));

  MOCK_METHOD1(StrError, const char*(int errnum));
};

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD4(OnMoreData, uint32(AudioOutputStream* stream,
                                  uint8* dest, uint32 max_size,
                                  AudioBuffersState buffers_state));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

class MockAudioManagerLinux : public AudioManagerLinux {
 public:
  MOCK_METHOD0(Init, void());
  MOCK_METHOD0(HasAudioOutputDevices, bool());
  MOCK_METHOD0(HasAudioInputDevices, bool());
  MOCK_METHOD1(MakeAudioOutputStream, AudioOutputStream*(
      const AudioParameters& params));
  MOCK_METHOD2(MakeAudioInputStream, AudioInputStream*(
      const AudioParameters& params, const std::string& device_id));
  MOCK_METHOD0(MuteAll, void());
  MOCK_METHOD0(UnMuteAll, void());

  MOCK_METHOD1(ReleaseOutputStream, void(AudioOutputStream* stream));
};

class AlsaPcmOutputStreamTest : public testing::Test {
 protected:
  AlsaPcmOutputStreamTest() {
    test_stream_.reset(CreateStream(kTestChannelLayout));
  }

  virtual ~AlsaPcmOutputStreamTest() {
    test_stream_.reset(NULL);
  }

  AlsaPcmOutputStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  AlsaPcmOutputStream* CreateStream(ChannelLayout layout,
                                    int32 samples_per_packet) {
    AudioParameters params(kTestFormat, layout, kTestSampleRate,
                           kTestBitsPerSample, samples_per_packet);
    return new AlsaPcmOutputStream(kTestDeviceName,
                                   params,
                                   &mock_alsa_wrapper_,
                                   &mock_manager_,
                                   &message_loop_);
  }

  // Helper function to malloc the string returned by DeviceNameHint for NAME.
  static char* EchoHint(const void* name, Unused) {
    return strdup(static_cast<const char*>(name));
  }

  // Helper function to malloc the string returned by DeviceNameHint for IOID.
  static char* OutputHint(Unused, Unused) {
    return strdup("Output");
  }

  // Helper function to initialize |test_stream_->buffer_|. Must be called
  // in all tests that use buffer_ without opening the stream.
  void InitBuffer() {
    packet_ = new media::DataBuffer(kTestPacketSize);
    packet_->SetDataSize(kTestPacketSize);
    test_stream_->buffer_.reset(new media::SeekableBuffer(0, kTestPacketSize));
    test_stream_->buffer_->Append(packet_.get());
  }

  static const ChannelLayout kTestChannelLayout;
  static const int kTestSampleRate;
  static const int kTestBitsPerSample;
  static const int kTestBytesPerFrame;
  static const AudioParameters::Format kTestFormat;
  static const char kTestDeviceName[];
  static const char kDummyMessage[];
  static const uint32 kTestFramesPerPacket;
  static const uint32 kTestPacketSize;
  static const int kTestFailedErrno;
  static snd_pcm_t* const kFakeHandle;

  // Used to simulate DeviceNameHint.
  static char kSurround40[];
  static char kSurround41[];
  static char kSurround50[];
  static char kSurround51[];
  static char kSurround70[];
  static char kSurround71[];
  static void* kFakeHints[];

  StrictMock<MockAlsaWrapper> mock_alsa_wrapper_;
  StrictMock<MockAudioManagerLinux> mock_manager_;
  MessageLoop message_loop_;
  scoped_ptr<AlsaPcmOutputStream> test_stream_;
  scoped_refptr<media::DataBuffer> packet_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AlsaPcmOutputStreamTest);
};

const ChannelLayout AlsaPcmOutputStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const int AlsaPcmOutputStreamTest::kTestSampleRate =
    AudioParameters::kAudioCDSampleRate;
const int AlsaPcmOutputStreamTest::kTestBitsPerSample = 8;
const int AlsaPcmOutputStreamTest::kTestBytesPerFrame =
    AlsaPcmOutputStreamTest::kTestBitsPerSample / 8 *
    ChannelLayoutToChannelCount(AlsaPcmOutputStreamTest::kTestChannelLayout);
const AudioParameters::Format AlsaPcmOutputStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const char AlsaPcmOutputStreamTest::kTestDeviceName[] = "TestDevice";
const char AlsaPcmOutputStreamTest::kDummyMessage[] = "dummy";
const uint32 AlsaPcmOutputStreamTest::kTestFramesPerPacket = 1000;
const uint32 AlsaPcmOutputStreamTest::kTestPacketSize =
    AlsaPcmOutputStreamTest::kTestFramesPerPacket *
    AlsaPcmOutputStreamTest::kTestBytesPerFrame;
const int AlsaPcmOutputStreamTest::kTestFailedErrno = -EACCES;
snd_pcm_t* const AlsaPcmOutputStreamTest::kFakeHandle =
    reinterpret_cast<snd_pcm_t*>(1);

char AlsaPcmOutputStreamTest::kSurround40[] = "surround40:CARD=foo,DEV=0";
char AlsaPcmOutputStreamTest::kSurround41[] = "surround41:CARD=foo,DEV=0";
char AlsaPcmOutputStreamTest::kSurround50[] = "surround50:CARD=foo,DEV=0";
char AlsaPcmOutputStreamTest::kSurround51[] = "surround51:CARD=foo,DEV=0";
char AlsaPcmOutputStreamTest::kSurround70[] = "surround70:CARD=foo,DEV=0";
char AlsaPcmOutputStreamTest::kSurround71[] = "surround71:CARD=foo,DEV=0";
void* AlsaPcmOutputStreamTest::kFakeHints[] = {
    kSurround40, kSurround41, kSurround50, kSurround51,
    kSurround70, kSurround71, NULL };

TEST_F(AlsaPcmOutputStreamTest, ConstructedState) {
  EXPECT_EQ(AlsaPcmOutputStream::kCreated, test_stream_->state());

  // Should support mono.
  test_stream_.reset(CreateStream(CHANNEL_LAYOUT_MONO));
  EXPECT_EQ(AlsaPcmOutputStream::kCreated, test_stream_->state());

  // Should support multi-channel.
  test_stream_.reset(CreateStream(CHANNEL_LAYOUT_SURROUND));
  EXPECT_EQ(AlsaPcmOutputStream::kCreated, test_stream_->state());

  // Bad bits per sample.
  AudioParameters bad_bps_params(kTestFormat, kTestChannelLayout,
                                 kTestSampleRate, kTestBitsPerSample - 1,
                                 kTestFramesPerPacket);
  test_stream_.reset(new AlsaPcmOutputStream(kTestDeviceName,
                                             bad_bps_params,
                                             &mock_alsa_wrapper_,
                                             &mock_manager_,
                                             &message_loop_));
  EXPECT_EQ(AlsaPcmOutputStream::kInError, test_stream_->state());

  // Bad format.
  AudioParameters bad_format_params(
      AudioParameters::AUDIO_LAST_FORMAT, kTestChannelLayout, kTestSampleRate,
      kTestBitsPerSample, kTestFramesPerPacket);
  test_stream_.reset(new AlsaPcmOutputStream(kTestDeviceName,
                                             bad_format_params,
                                             &mock_alsa_wrapper_,
                                             &mock_manager_,
                                             &message_loop_));
  EXPECT_EQ(AlsaPcmOutputStream::kInError, test_stream_->state());
}

TEST_F(AlsaPcmOutputStreamTest, LatencyFloor) {
  const double kMicrosPerFrame =
      static_cast<double>(1000000) / kTestSampleRate;
  const double kPacketFramesInMinLatency =
      AlsaPcmOutputStream::kMinLatencyMicros / kMicrosPerFrame / 2.0;

  // Test that packets which would cause a latency under less than
  // AlsaPcmOutputStream::kMinLatencyMicros will get clipped to
  // AlsaPcmOutputStream::kMinLatencyMicros,
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle),
                      Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(_, _, _, _, _, _,
                           AlsaPcmOutputStream::kMinLatencyMicros))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kTestFramesPerPacket),
                      SetArgumentPointee<2>(kTestFramesPerPacket / 2),
                      Return(0)));

  test_stream_.reset(CreateStream(kTestChannelLayout,
                                  kPacketFramesInMinLatency));
  ASSERT_TRUE(test_stream_->Open());
  message_loop_.RunAllPending();

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle)).WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(kFakeHandle))
      .WillOnce(Return(kTestDeviceName));
  EXPECT_CALL(mock_manager_, ReleaseOutputStream(test_stream_.get()));
  test_stream_->Close();
  message_loop_.RunAllPending();

  Mock::VerifyAndClear(&mock_alsa_wrapper_);
  Mock::VerifyAndClear(&mock_manager_);

  // Test that having more packets ends up with a latency based on packet size.
  const int kOverMinLatencyPacketSize = kPacketFramesInMinLatency + 1;
  int64 expected_micros = 2 * AlsaPcmOutputStream::FramesToMicros(
      kOverMinLatencyPacketSize, kTestSampleRate);

  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(_, _, _, _, _, _, expected_micros))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kTestFramesPerPacket),
                      SetArgumentPointee<2>(kTestFramesPerPacket / 2),
                      Return(0)));

  test_stream_.reset(CreateStream(kTestChannelLayout,
                                  kOverMinLatencyPacketSize));
  ASSERT_TRUE(test_stream_->Open());
  message_loop_.RunAllPending();

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(kFakeHandle))
      .WillOnce(Return(kTestDeviceName));
  EXPECT_CALL(mock_manager_, ReleaseOutputStream(test_stream_.get()));
  test_stream_->Close();
  message_loop_.RunAllPending();

  Mock::VerifyAndClear(&mock_alsa_wrapper_);
  Mock::VerifyAndClear(&mock_manager_);
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
                           SND_PCM_FORMAT_U8,
                           SND_PCM_ACCESS_RW_INTERLEAVED,
                           ChannelLayoutToChannelCount(kTestChannelLayout),
                           kTestSampleRate,
                           1,
                           expected_micros))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(kFakeHandle, _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kTestFramesPerPacket),
                      SetArgumentPointee<2>(kTestFramesPerPacket / 2),
                      Return(0)));

  // Open the stream.
  ASSERT_TRUE(test_stream_->Open());
  message_loop_.RunAllPending();

  EXPECT_EQ(AlsaPcmOutputStream::kIsOpened, test_stream_->state());
  EXPECT_EQ(kFakeHandle, test_stream_->playback_handle_);
  EXPECT_EQ(kTestFramesPerPacket, test_stream_->frames_per_packet_);
  EXPECT_TRUE(test_stream_->buffer_.get());
  EXPECT_FALSE(test_stream_->stop_stream_);

  // Now close it and test that everything was released.
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(kFakeHandle))
      .WillOnce(Return(kTestDeviceName));
  EXPECT_CALL(mock_manager_, ReleaseOutputStream(test_stream_.get()));
  test_stream_->Close();
  message_loop_.RunAllPending();

  EXPECT_TRUE(test_stream_->playback_handle_ == NULL);
  EXPECT_FALSE(test_stream_->buffer_.get());
  EXPECT_TRUE(test_stream_->stop_stream_);
}

TEST_F(AlsaPcmOutputStreamTest, PcmOpenFailed) {
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  // Open still succeeds since PcmOpen is delegated to another thread.
  ASSERT_TRUE(test_stream_->Open());
  ASSERT_EQ(AlsaPcmOutputStream::kIsOpened, test_stream_->state());
  ASSERT_FALSE(test_stream_->stop_stream_);
  message_loop_.RunAllPending();

  // Ensure internal state is set for a no-op stream if PcmOpen() failes.
  EXPECT_EQ(AlsaPcmOutputStream::kIsOpened, test_stream_->state());
  EXPECT_TRUE(test_stream_->stop_stream_);
  EXPECT_TRUE(test_stream_->playback_handle_ == NULL);
  EXPECT_FALSE(test_stream_->buffer_.get());

  // Close the stream since we opened it to make destruction happy.
  EXPECT_CALL(mock_manager_, ReleaseOutputStream(test_stream_.get()));
  test_stream_->Close();
  message_loop_.RunAllPending();
}

TEST_F(AlsaPcmOutputStreamTest, PcmSetParamsFailed) {
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle),
                      Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(kFakeHandle))
      .WillOnce(Return(kTestDeviceName));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  // If open fails, the stream stays in kCreated because it has effectively had
  // no changes.
  ASSERT_TRUE(test_stream_->Open());
  EXPECT_EQ(AlsaPcmOutputStream::kIsOpened, test_stream_->state());
  ASSERT_FALSE(test_stream_->stop_stream_);
  message_loop_.RunAllPending();

  // Ensure internal state is set for a no-op stream if PcmSetParams() failes.
  EXPECT_EQ(AlsaPcmOutputStream::kIsOpened, test_stream_->state());
  EXPECT_TRUE(test_stream_->stop_stream_);
  EXPECT_TRUE(test_stream_->playback_handle_ == NULL);
  EXPECT_FALSE(test_stream_->buffer_.get());

  // Close the stream since we opened it to make destruction happy.
  EXPECT_CALL(mock_manager_, ReleaseOutputStream(test_stream_.get()));
  test_stream_->Close();
  message_loop_.RunAllPending();
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
  EXPECT_CALL(mock_alsa_wrapper_, PcmGetParams(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kTestFramesPerPacket),
                      SetArgumentPointee<2>(kTestFramesPerPacket / 2),
                      Return(0)));

  // Open the stream.
  ASSERT_TRUE(test_stream_->Open());
  message_loop_.RunAllPending();

  // Expect Device setup.
  EXPECT_CALL(mock_alsa_wrapper_, PcmDrop(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmPrepare(kFakeHandle))
      .WillOnce(Return(0));

  // Expect the pre-roll.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(kFakeHandle))
      .Times(3)
      .WillRepeatedly(Return(SND_PCM_STATE_RUNNING));
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(kFakeHandle, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(0), Return(0)));
  EXPECT_CALL(mock_callback,
              OnMoreData(test_stream_.get(), _, kTestPacketSize, _))
      .Times(2)
      .WillOnce(Return(kTestPacketSize))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(kFakeHandle, _, _))
      .WillOnce(Return(kTestFramesPerPacket));

  // Expect scheduling.
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(kFakeHandle))
      .Times(AtLeast(3))
      .WillOnce(Return(kTestFramesPerPacket))  // Buffer is empty.
      .WillOnce(Return(kTestFramesPerPacket))
      .WillRepeatedly(DoAll(InvokeWithoutArgs(&message_loop_,
                                              &MessageLoop::QuitNow),
                                              Return(0)));  // Buffer is full.

  test_stream_->Start(&mock_callback);
  message_loop_.RunAllPending();

  EXPECT_CALL(mock_manager_, ReleaseOutputStream(test_stream_.get()));
  EXPECT_CALL(mock_alsa_wrapper_, PcmClose(kFakeHandle))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, PcmName(kFakeHandle))
      .WillOnce(Return(kTestDeviceName));
  test_stream_->Close();
  message_loop_.RunAllPending();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_FinishedPacket) {
  InitBuffer();

  // Nothing should happen.  Don't set any expectations and Our strict mocks
  // should verify most of this.

  // Test empty buffer.
  test_stream_->buffer_->Clear();
  test_stream_->WritePacket();
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_NormalPacket) {
  InitBuffer();

  // Write a little less than half the data.
  int written = packet_->GetDataSize() / kTestBytesPerFrame / 2 - 1;
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(_, packet_->GetData(), _))
      .WillOnce(Return(written));

  test_stream_->WritePacket();

  ASSERT_EQ(test_stream_->buffer_->forward_bytes(),
            packet_->GetDataSize() - written * kTestBytesPerFrame);

  // Write the rest.
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmWritei(_, packet_->GetData() + written * kTestBytesPerFrame,
                        _))
      .WillOnce(Return(packet_->GetDataSize() / kTestBytesPerFrame - written));
  test_stream_->WritePacket();
  EXPECT_EQ(0u, test_stream_->buffer_->forward_bytes());
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_WriteFails) {
  InitBuffer();

  // Fail due to a recoverable error and see that PcmRecover code path
  // continues normally.
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(_, _, _))
      .WillOnce(Return(-EINTR));
  EXPECT_CALL(mock_alsa_wrapper_, PcmRecover(_, _, _))
      .WillOnce(Return(packet_->GetDataSize() / kTestBytesPerFrame / 2 - 1));

  test_stream_->WritePacket();

  ASSERT_EQ(test_stream_->buffer_->forward_bytes(),
            packet_->GetDataSize() / 2 + kTestBytesPerFrame);

  // Fail the next write, and see that stop_stream_ is set.
  EXPECT_CALL(mock_alsa_wrapper_, PcmWritei(_, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmRecover(_, _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));
  test_stream_->WritePacket();
  EXPECT_EQ(test_stream_->buffer_->forward_bytes(),
            packet_->GetDataSize() / 2 + kTestBytesPerFrame);
  EXPECT_TRUE(test_stream_->stop_stream_);
}

TEST_F(AlsaPcmOutputStreamTest, WritePacket_StopStream) {
  InitBuffer();

  // No expectations set on the strict mock because nothing should be called.
  test_stream_->stop_stream_ = true;
  test_stream_->WritePacket();
  EXPECT_EQ(0u, test_stream_->buffer_->forward_bytes());
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket) {
  InitBuffer();
  test_stream_->buffer_->Clear();

  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(_))
      .WillOnce(Return(SND_PCM_STATE_RUNNING));
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(1), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillRepeatedly(Return(0));  // Buffer is full.

  // Return a partially filled packet.
  EXPECT_CALL(mock_callback,
              OnMoreData(test_stream_.get(), _, _, _))
      .WillOnce(Return(10));

  bool source_exhausted;
  test_stream_->set_source_callback(&mock_callback);
  test_stream_->packet_size_ = kTestPacketSize;
  test_stream_->BufferPacket(&source_exhausted);

  EXPECT_EQ(10u, test_stream_->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_Negative) {
  InitBuffer();
  test_stream_->buffer_->Clear();

  // Simulate where the underrun has occurred right after checking the delay.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(_))
      .WillOnce(Return(SND_PCM_STATE_RUNNING));
  EXPECT_CALL(mock_alsa_wrapper_, PcmDelay(_, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(-1), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillRepeatedly(Return(0));  // Buffer is full.
  EXPECT_CALL(mock_callback,
              OnMoreData(test_stream_.get(), _, _, _))
      .WillOnce(Return(10));

  bool source_exhausted;
  test_stream_->set_source_callback(&mock_callback);
  test_stream_->packet_size_ = kTestPacketSize;
  test_stream_->BufferPacket(&source_exhausted);

  EXPECT_EQ(10u, test_stream_->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_Underrun) {
  InitBuffer();
  test_stream_->buffer_->Clear();

  // If ALSA has underrun then we should assume a delay of zero.
  MockAudioSourceCallback mock_callback;
  EXPECT_CALL(mock_alsa_wrapper_, PcmState(_))
     .WillOnce(Return(SND_PCM_STATE_XRUN));
  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillRepeatedly(Return(0));  // Buffer is full.
  EXPECT_CALL(mock_callback,
              OnMoreData(test_stream_.get(), _, _, AllOf(
                  Field(&AudioBuffersState::pending_bytes, 0),
                  Field(&AudioBuffersState::hardware_delay_bytes, 0))))
      .WillOnce(Return(10));

  bool source_exhausted;
  test_stream_->set_source_callback(&mock_callback);
  test_stream_->packet_size_ = kTestPacketSize;
  test_stream_->BufferPacket(&source_exhausted);

  EXPECT_EQ(10u, test_stream_->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_FullBuffer) {
  InitBuffer();
  // No expectations set on the strict mock because nothing should be called.
  bool source_exhausted;
  test_stream_->packet_size_ = kTestPacketSize;
  test_stream_->BufferPacket(&source_exhausted);
  EXPECT_EQ(kTestPacketSize, test_stream_->buffer_->forward_bytes());
  EXPECT_FALSE(source_exhausted);
}

TEST_F(AlsaPcmOutputStreamTest, AutoSelectDevice_DeviceSelect) {
  // Try channels from 1 -> 9. and see that we get the more specific surroundXX
  // device opened for channels 4-8.  For all other channels, the device should
  // default to |AlsaPcmOutputStream::kDefaultDevice|.  We should also not
  // downmix any channel in this case because downmixing is only defined for
  // channels 4-8, which we are guaranteeing to work.
  //
  // Note that the loop starts at "1", so the first parameter is ignored in
  // these arrays.
  const char* kExpectedDeviceName[] = { NULL,
                                        AlsaPcmOutputStream::kDefaultDevice,
                                        AlsaPcmOutputStream::kDefaultDevice,
                                        AlsaPcmOutputStream::kDefaultDevice,
                                        kSurround40, kSurround50, kSurround51,
                                        kSurround70, kSurround71,
                                        AlsaPcmOutputStream::kDefaultDevice };
  bool kExpectedDownmix[] = { false, false, false, false, false, true,
                              false, false, false, false };
  ChannelLayout kExpectedLayouts[] = { CHANNEL_LAYOUT_NONE,
                                       CHANNEL_LAYOUT_MONO,
                                       CHANNEL_LAYOUT_STEREO,
                                       CHANNEL_LAYOUT_SURROUND,
                                       CHANNEL_LAYOUT_4POINT0,
                                       CHANNEL_LAYOUT_5POINT0,
                                       CHANNEL_LAYOUT_5POINT1,
                                       CHANNEL_LAYOUT_7POINT0,
                                       CHANNEL_LAYOUT_7POINT1 };


  for (int i = 1; i < 9; ++i) {
    if (i == 3 || i == 4 || i == 5)  // invalid number of channels
      continue;
    SCOPED_TRACE(base::StringPrintf("Attempting %d Channel", i));

    // Hints will only be grabbed for channel numbers that have non-default
    // devices associated with them.
    if (kExpectedDeviceName[i] != AlsaPcmOutputStream::kDefaultDevice) {
      // The DeviceNameHint and DeviceNameFreeHint need to be paired to avoid a
      // memory leak.
      EXPECT_CALL(mock_alsa_wrapper_, DeviceNameHint(_, _, _))
          .WillOnce(DoAll(SetArgumentPointee<2>(&kFakeHints[0]), Return(0)));
      EXPECT_CALL(mock_alsa_wrapper_, DeviceNameFreeHint(&kFakeHints[0]))
          .Times(1);
    }

    EXPECT_CALL(mock_alsa_wrapper_,
                PcmOpen(_, StrEq(kExpectedDeviceName[i]), _, _))
        .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle), Return(0)));
    EXPECT_CALL(mock_alsa_wrapper_,
                PcmSetParams(kFakeHandle, _, _, i, _, _, _))
        .WillOnce(Return(0));

    // The parameters are specified by ALSA documentation, and are in constants
    // in the implementation files.
    EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("IOID")))
        .WillRepeatedly(Invoke(OutputHint));
    EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("NAME")))
        .WillRepeatedly(Invoke(EchoHint));

    test_stream_.reset(CreateStream(kExpectedLayouts[i]));
    EXPECT_TRUE(test_stream_->AutoSelectDevice(i));
    EXPECT_EQ(kExpectedDownmix[i], test_stream_->should_downmix_);

    Mock::VerifyAndClearExpectations(&mock_alsa_wrapper_);
    Mock::VerifyAndClearExpectations(&mock_manager_);
  }
}

TEST_F(AlsaPcmOutputStreamTest, AutoSelectDevice_FallbackDevices) {
  using std::string;

  // If there are problems opening a multi-channel device, it the fallbacks
  // operations should be as follows.  Assume the multi-channel device name is
  // surround50:
  //
  //   1) Try open "surround50"
  //   2) Try open "plug:surround50".
  //   3) Try open "default".
  //   4) Try open "plug:default".
  //   5) Give up trying to open.
  //
  const string first_try = kSurround50;
  const string second_try = string(AlsaPcmOutputStream::kPlugPrefix) +
                            kSurround50;
  const string third_try = AlsaPcmOutputStream::kDefaultDevice;
  const string fourth_try = string(AlsaPcmOutputStream::kPlugPrefix) +
                            AlsaPcmOutputStream::kDefaultDevice;

  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameHint(_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(&kFakeHints[0]), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameFreeHint(&kFakeHints[0]))
      .Times(1);
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("IOID")))
      .WillRepeatedly(Invoke(OutputHint));
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameGetHint(_, StrEq("NAME")))
      .WillRepeatedly(Invoke(EchoHint));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillRepeatedly(Return(kDummyMessage));

  InSequence s;
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(first_try.c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(second_try.c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(third_try.c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_, PcmOpen(_, StrEq(fourth_try.c_str()), _, _))
      .WillOnce(Return(kTestFailedErrno));

  test_stream_.reset(CreateStream(CHANNEL_LAYOUT_5POINT0));
  EXPECT_FALSE(test_stream_->AutoSelectDevice(5));
}

TEST_F(AlsaPcmOutputStreamTest, AutoSelectDevice_HintFail) {
  // Should get |kDefaultDevice|, and force a 2-channel downmix on a failure to
  // enumerate devices.
  EXPECT_CALL(mock_alsa_wrapper_, DeviceNameHint(_, _, _))
      .WillRepeatedly(Return(kTestFailedErrno));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmOpen(_, StrEq(AlsaPcmOutputStream::kDefaultDevice), _, _))
      .WillOnce(DoAll(SetArgumentPointee<0>(kFakeHandle), Return(0)));
  EXPECT_CALL(mock_alsa_wrapper_,
              PcmSetParams(kFakeHandle, _, _, 2, _, _, _))
      .WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper_, StrError(kTestFailedErrno))
      .WillOnce(Return(kDummyMessage));

  test_stream_.reset(CreateStream(CHANNEL_LAYOUT_5POINT0));
  EXPECT_TRUE(test_stream_->AutoSelectDevice(5));
  EXPECT_TRUE(test_stream_->should_downmix_);
}

TEST_F(AlsaPcmOutputStreamTest, BufferPacket_StopStream) {
  InitBuffer();
  test_stream_->stop_stream_ = true;
  bool source_exhausted;
  test_stream_->BufferPacket(&source_exhausted);
  EXPECT_EQ(0u, test_stream_->buffer_->forward_bytes());
  EXPECT_TRUE(source_exhausted);
}

TEST_F(AlsaPcmOutputStreamTest, ScheduleNextWrite) {
  test_stream_->TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream_->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  InitBuffer();

  EXPECT_CALL(mock_alsa_wrapper_, PcmAvailUpdate(_))
      .WillOnce(Return(10));
  test_stream_->ScheduleNextWrite(false);

  // TODO(sergeyu): Figure out how to check that the task has been added to the
  // message loop.

  // Cleanup the message queue. Currently ~MessageQueue() doesn't free pending
  // tasks unless running on valgrind. The code below is needed to keep
  // heapcheck happy.
  test_stream_->stop_stream_ = true;
  message_loop_.RunAllPending();

  test_stream_->TransitionTo(AlsaPcmOutputStream::kIsClosed);
}

TEST_F(AlsaPcmOutputStreamTest, ScheduleNextWrite_StopStream) {
  test_stream_->TransitionTo(AlsaPcmOutputStream::kIsOpened);
  test_stream_->TransitionTo(AlsaPcmOutputStream::kIsPlaying);

  InitBuffer();

  test_stream_->stop_stream_ = true;
  test_stream_->ScheduleNextWrite(true);

  // TODO(ajwong): Find a way to test whether or not another task has been
  // posted so we can verify that the Alsa code will indeed break the task
  // posting loop.

  test_stream_->TransitionTo(AlsaPcmOutputStream::kIsClosed);
}
