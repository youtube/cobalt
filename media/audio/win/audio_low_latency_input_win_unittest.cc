// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;
using ::testing::AnyNumber;
using ::testing::Between;
using ::testing::Gt;
using ::testing::NotNull;

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData, void(AudioInputStream* stream,
      const uint8* src, uint32 size,
      uint32 hardware_delay_bytes));
  MOCK_METHOD1(OnClose, void(AudioInputStream* stream));
  MOCK_METHOD2(OnError, void(AudioInputStream* stream, int code));
};

// This audio sink implementation should be used for manual tests only since
// the recorded data is stored on a raw binary data file.
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const size_t kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name)
      : buffer_(0, kMaxBufferSize),
    file_(fopen(file_name, "wb")),
    bytes_to_write_(0) {
  }

  virtual ~WriteToFileAudioSink() {
    size_t bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      const uint8* chunk;
      size_t chunk_size;

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
                      const uint8* src,
                      uint32 size,
                      uint32 hardware_delay_bytes) {
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
  size_t bytes_to_write_;
};

// Convenience method which ensures that we are not running on the build
// bots and that at least one valid input device can be found.
static bool CanRunAudioTests() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return false;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  if (NULL == audio_man)
    return false;
  // TODO(henrika): note that we use Wave today to query the number of
  // existing input devices.
  return audio_man->HasAudioInputDevices();
}

// Convenience method which creates a default AudioInputStream object but
// also allows the user to modify the default settings.
class AudioInputStreamWrapper {
 public:
  AudioInputStreamWrapper()
      : com_init_(ScopedCOMInitializer::kMTA),
        audio_man_(AudioManager::GetAudioManager()),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
        channel_layout_(CHANNEL_LAYOUT_STEREO),
        bits_per_sample_(16) {
    // Use native/mixing sample rate and 10ms frame size as default.
    sample_rate_ = static_cast<int>(
        WASAPIAudioInputStream::HardwareSampleRate(eConsole));
    samples_per_packet_ = sample_rate_ / 100;
  }

  ~AudioInputStreamWrapper() {}

  // Creates AudioInputStream object using default parameters.
  AudioInputStream* Create() {
    return CreateInputStream();
  }

  // Creates AudioInputStream object using non-default parameters where the
  // frame size is modified.
  AudioInputStream* Create(int samples_per_packet) {
    samples_per_packet_ = samples_per_packet;
    return CreateInputStream();
  }

  AudioParameters::Format format() const { return format_; }
  int channels() const {
    return ChannelLayoutToChannelCount(channel_layout_);
  }
  int bits_per_sample() const { return bits_per_sample_; }
  int sample_rate() const { return sample_rate_; }
  int samples_per_packet() const { return samples_per_packet_; }

 private:
  AudioInputStream* CreateInputStream() {
    AudioInputStream* ais = audio_man_->MakeAudioInputStream(
        AudioParameters(format_, channel_layout_, sample_rate_,
                        bits_per_sample_, samples_per_packet_),
                        AudioManagerBase::kDefaultDeviceId);
    EXPECT_TRUE(ais);
    return ais;
  }

  ScopedCOMInitializer com_init_;
  AudioManager* audio_man_;
  AudioParameters::Format format_;
  ChannelLayout channel_layout_;
  int bits_per_sample_;
  int sample_rate_;
  int samples_per_packet_;
};

// Convenience method which creates a default AudioInputStream object.
static AudioInputStream* CreateDefaultAudioInputStream() {
  AudioInputStreamWrapper aisw;
  AudioInputStream* ais = aisw.Create();
  return ais;
}

// Verify that we can retrieve the current hardware/mixing sample rate
// for all supported device roles. The ERole enumeration defines constants
// that indicate the role that the system/user has assigned to an audio
// endpoint device.
// TODO(henrika): modify this test when we suport full device enumeration.
TEST(WinAudioInputTest, WASAPIAudioInputStreamHardwareSampleRate) {
  if (!CanRunAudioTests())
    return;

  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Default device intended for games, system notification sounds,
  // and voice commands.
  int fs = static_cast<int>(
      WASAPIAudioInputStream::HardwareSampleRate(eConsole));
  EXPECT_GE(fs, 0);

  // Default communication device intended for e.g. VoIP communication.
  fs = static_cast<int>(
      WASAPIAudioInputStream::HardwareSampleRate(eCommunications));
  EXPECT_GE(fs, 0);

  // Multimedia device for music, movies and live music recording.
  fs = static_cast<int>(
      WASAPIAudioInputStream::HardwareSampleRate(eMultimedia));
  EXPECT_GE(fs, 0);
}

// Test Create(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamCreateAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  ais->Close();
}

// Test Open(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenStartAndClose) {
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

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenStartStopAndClose) {
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
TEST(MacAudioInputTest, WASAPIAudioInputStreamMiscCallingSequences) {
  if (!CanRunAudioTests())
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  WASAPIAudioInputStream* wais = static_cast<WASAPIAudioInputStream*>(ais);

  // Open(), Open() should fail the second time.
  EXPECT_TRUE(ais->Open());
  EXPECT_FALSE(ais->Open());

  MockAudioInputCallback sink;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  ais->Start(&sink);
  EXPECT_TRUE(wais->started());
  ais->Start(&sink);
  EXPECT_TRUE(wais->started());

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  ais->Stop();
  EXPECT_FALSE(wais->started());
  ais->Stop();
  EXPECT_FALSE(wais->started());

  EXPECT_CALL(sink, OnClose(ais))
    .Times(1);
  ais->Close();
}

TEST(WinAudioInputTest, WASAPIAudioInputStreamTestPacketSizes) {
  if (!CanRunAudioTests())
    return;

  // 10 ms packet size.

  // Create default WASAPI input stream which records in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioInputStreamWrapper aisw;
  AudioInputStream* ais = aisw.Create();
  EXPECT_TRUE(ais->Open());

  MockAudioInputCallback sink;

  // Derive the expected size in bytes of each recorded packet.
  uint32 bytes_per_packet = aisw.channels() * aisw.samples_per_packet() *
      (aisw.bits_per_sample() / 8);

  // We use 10ms packets and will run the test for ~100ms. Given that the
  // startup sequence takes some time, it is reasonable to expect 5-12
  // callbacks in this time period. All should contain valid packets of
  // the same size and a valid delay estimate.
  EXPECT_CALL(sink, OnData(
      ais, NotNull(), bytes_per_packet, Gt(bytes_per_packet)))
      .Times(Between(5, 10));

  ais->Start(&sink);
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout_ms());
  ais->Stop();

  // Store current packet size (to be used in the subsequent tests).
  int samples_per_packet_10ms = aisw.samples_per_packet();

  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();

  // 20 ms packet size.

  ais = aisw.Create(2 * samples_per_packet_10ms);
  EXPECT_TRUE(ais->Open());
  bytes_per_packet = aisw.channels() * aisw.samples_per_packet() *
      (aisw.bits_per_sample() / 8);

  EXPECT_CALL(sink, OnData(
      ais, NotNull(), bytes_per_packet, Gt(bytes_per_packet)))
      .Times(Between(5, 10));
  ais->Start(&sink);
  base::PlatformThread::Sleep(2 * TestTimeouts::tiny_timeout_ms());
  ais->Stop();

  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();

  // 5 ms packet size.

  ais = aisw.Create(samples_per_packet_10ms / 2);
  EXPECT_TRUE(ais->Open());
  bytes_per_packet = aisw.channels() * aisw.samples_per_packet() *
    (aisw.bits_per_sample() / 8);

  EXPECT_CALL(sink, OnData(
      ais, NotNull(), bytes_per_packet, Gt(bytes_per_packet)))
      .Times(Between(2 * 5, 2 * 10));
  ais->Start(&sink);
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout_ms());
  ais->Stop();

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
TEST(WinAudioInputTest, DISABLED_WASAPIAudioInputStreamRecordToFile) {
  if (!CanRunAudioTests())
    return;

  const char* file_name = "out_stereo_10sec.pcm";

  AudioInputStreamWrapper aisw;
  AudioInputStream* ais = aisw.Create();
  EXPECT_TRUE(ais->Open());

  fprintf(stderr, "               File name  : %s\n", file_name);
  fprintf(stderr, "               Sample rate: %d\n", aisw.sample_rate());
  WriteToFileAudioSink file_sink(file_name);
  fprintf(stderr, "               >> Speak into the mic while recording...\n");
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());
  ais->Stop();
  fprintf(stderr, "               >> Recording has stopped.\n");
  ais->Close();
}
