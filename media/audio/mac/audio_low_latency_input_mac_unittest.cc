// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::NotNull;

namespace media {

ACTION_P3(CheckCountAndPostQuitTask, count, limit, loop) {
  if (++*count >= limit) {
    loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }
}

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD5(OnData, void(AudioInputStream* stream,
                            const uint8* src, uint32 size,
                            uint32 hardware_delay_bytes, double volume));
  MOCK_METHOD1(OnClose, void(AudioInputStream* stream));
  MOCK_METHOD2(OnError, void(AudioInputStream* stream, int code));
};

// This audio sink implementation should be used for manual tests only since
// the recorded data is stored on a raw binary data file.
// The last test (WriteToFileAudioSink) - which is disabled by default -
// can use this audio sink to store the captured data on a file for offline
// analysis.
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const int kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name)
      : buffer_(0, kMaxBufferSize),
        file_(fopen(file_name, "wb")),
        bytes_to_write_(0) {
  }

  virtual ~WriteToFileAudioSink() {
    int bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      const uint8* chunk;
      int chunk_size;

      // Stop writing if no more data is available.
      if (!buffer_.GetCurrentChunk(&chunk, &chunk_size))
        break;

      // Write recorded data chunk to the file and prepare for next chunk.
      fwrite(chunk, 1, chunk_size, file_);
      buffer_.Seek(chunk_size);
      bytes_written += chunk_size;
    }
    fclose(file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  virtual void OnData(AudioInputStream* stream,
                      const uint8* src, uint32 size,
                      uint32 hardware_delay_bytes, double volume) {
    // Store data data in a temporary buffer to avoid making blocking
    // fwrite() calls in the audio callback. The complete buffer will be
    // written to file in the destructor.
    if (buffer_.Append(src, size)) {
      bytes_to_write_ += size;
    }
  }

  virtual void OnClose(AudioInputStream* stream) {}
  virtual void OnError(AudioInputStream* stream, int code) {}

 private:
  media::SeekableBuffer buffer_;
  FILE* file_;
  int bytes_to_write_;
};

class MacAudioInputTest : public testing::Test {
 protected:
  MacAudioInputTest() : audio_manager_(AudioManager::Create()) {}
  virtual ~MacAudioInputTest() {}

  // Convenience method which ensures that we are not running on the build
  // bots and that at least one valid input device can be found.
  bool CanRunAudioTests() {
    bool has_input = audio_manager_->HasAudioInputDevices();
    if (!has_input)
      LOG(WARNING) << "No input devices detected";
    return has_input;
  }

  // Convenience method which creates a default AudioInputStream object using
  // a 10ms frame size and a sample rate which is set to the hardware sample
  // rate.
  AudioInputStream* CreateDefaultAudioInputStream() {
    int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
    int samples_per_packet = fs / 100;
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
        CHANNEL_LAYOUT_STEREO, fs, 16, samples_per_packet),
        AudioManagerBase::kDefaultDeviceId);
    EXPECT_TRUE(ais);
    return ais;
  }

  // Convenience method which creates an AudioInputStream object with a
  // specified channel layout.
  AudioInputStream* CreateAudioInputStream(ChannelLayout channel_layout) {
    int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
    int samples_per_packet = fs / 100;
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
        channel_layout, fs, 16, samples_per_packet),
        AudioManagerBase::kDefaultDeviceId);
    EXPECT_TRUE(ais);
    return ais;
  }

  scoped_ptr<AudioManager> audio_manager_;
};

// Test Create(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamCreateAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  ais->Close();
}

// Test Open(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Test Open(), Start(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenStartAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// Test Open(), Start(), Stop(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenStartStopAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Stop();
  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// Test some additional calling sequences.
TEST_F(MacAudioInputTest, AUAudioInputStreamMiscCallingSequences) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  AUAudioInputStream* auais = static_cast<AUAudioInputStream*>(ais);

  // Open(), Open() should fail the second time.
  EXPECT_TRUE(ais->Open());
  EXPECT_FALSE(ais->Open());

  MockAudioInputCallback sink;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  ais->Start(&sink);
  EXPECT_TRUE(auais->started());
  ais->Start(&sink);
  EXPECT_TRUE(auais->started());

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  ais->Stop();
  EXPECT_FALSE(auais->started());
  ais->Stop();
  EXPECT_FALSE(auais->started());

  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// Verify that recording starts and stops correctly in mono using mocked sink.
TEST_F(MacAudioInputTest, AUAudioInputStreamVerifyMonoRecording) {
  if (!CanRunAudioTests())
    return;

  int count = 0;
  MessageLoopForUI loop;

  // Create an audio input stream which records in mono.
  AudioInputStream* ais = CreateAudioInputStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(ais->Open());

  int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
  int samples_per_packet = fs / 100;
  int bits_per_sample = 16;
  uint32 bytes_per_packet = samples_per_packet * (bits_per_sample / 8);

  MockAudioInputCallback sink;

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  EXPECT_CALL(sink, OnData(ais, NotNull(), bytes_per_packet,
                           Ge(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();

  // Verify that the sink receieves OnClose() call when calling Close().
  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// Verify that recording starts and stops correctly in mono using mocked sink.
TEST_F(MacAudioInputTest, AUAudioInputStreamVerifyStereoRecording) {
  if (!CanRunAudioTests())
    return;

  int count = 0;
  MessageLoopForUI loop;

  // Create an audio input stream which records in stereo.
  AudioInputStream* ais = CreateAudioInputStream(CHANNEL_LAYOUT_STEREO);
  EXPECT_TRUE(ais->Open());

  int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
  int samples_per_packet = fs / 100;
  int bits_per_sample = 16;
  uint32 bytes_per_packet = 2 * samples_per_packet * (bits_per_sample / 8);

  MockAudioInputCallback sink;

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  EXPECT_CALL(sink, OnData(ais, NotNull(), bytes_per_packet,
                           Ge(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();

  // Verify that the sink receieves OnClose() call when calling Close().
  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// This test is intended for manual tests and should only be enabled
// when it is required to store the captured data on a local file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
TEST_F(MacAudioInputTest, DISABLED_AUAudioInputStreamRecordToFile) {
  if (!CanRunAudioTests())
    return;
  const char* file_name = "out_stereo_10sec.pcm";

  int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());

  fprintf(stderr, "               File name  : %s\n", file_name);
  fprintf(stderr, "               Sample rate: %d\n", fs);
  WriteToFileAudioSink file_sink(file_name);
  fprintf(stderr, "               >> Speak into the mic while recording...\n");
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  fprintf(stderr, "               >> Recording has stopped.\n");
  ais->Close();
}

}  // namespace media
