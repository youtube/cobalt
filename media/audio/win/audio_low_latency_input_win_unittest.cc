// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Gt;
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
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const size_t kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name)
      : buffer_(0, kMaxBufferSize),
        bytes_to_write_(0) {
    FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_EXE, &file_path));
    file_path = file_path.AppendASCII(file_name);
    binary_file_ = file_util::OpenFile(file_path, "wb");
    DLOG_IF(ERROR, !binary_file_) << "Failed to open binary PCM data file.";
    LOG(INFO) << ">> Output file: " << file_path.value()
              << " has been created.";
  }

  virtual ~WriteToFileAudioSink() {
    size_t bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      const uint8* chunk;
      int chunk_size;

      // Stop writing if no more data is available.
      if (!buffer_.GetCurrentChunk(&chunk, &chunk_size))
        break;

      // Write recorded data chunk to the file and prepare for next chunk.
      fwrite(chunk, 1, chunk_size, binary_file_);
      buffer_.Seek(chunk_size);
      bytes_written += chunk_size;
    }
    file_util::CloseFile(binary_file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  virtual void OnData(AudioInputStream* stream,
                      const uint8* src,
                      uint32 size,
                      uint32 hardware_delay_bytes,
                      double volume) {
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
  FILE* binary_file_;
  size_t bytes_to_write_;
};

// Convenience method which ensures that we are not running on the build
// bots and that at least one valid input device can be found. We also
// verify that we are not running on XP since the low-latency (WASAPI-
// based) version requires Windows Vista or higher.
static bool CanRunAudioTests(AudioManager* audio_man) {
  if (!media::IsWASAPISupported()) {
    LOG(WARNING) << "This tests requires Windows Vista or higher.";
    return false;
  }
  // TODO(henrika): note that we use Wave today to query the number of
  // existing input devices.
  bool input = audio_man->HasAudioInputDevices();
  LOG_IF(WARNING, !input) << "No input device detected.";
  return input;
}

// Convenience method which creates a default AudioInputStream object but
// also allows the user to modify the default settings.
class AudioInputStreamWrapper {
 public:
  explicit AudioInputStreamWrapper(AudioManager* audio_manager)
      : com_init_(ScopedCOMInitializer::kMTA),
        audio_man_(audio_manager),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
        channel_layout_(CHANNEL_LAYOUT_STEREO),
        bits_per_sample_(16) {
    // Use native/mixing sample rate and 10ms frame size as default.
    sample_rate_ = static_cast<int>(
        WASAPIAudioInputStream::HardwareSampleRate(
            AudioManagerBase::kDefaultDeviceId));
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
static AudioInputStream* CreateDefaultAudioInputStream(
    AudioManager* audio_manager) {
  AudioInputStreamWrapper aisw(audio_manager);
  AudioInputStream* ais = aisw.Create();
  return ais;
}

// Verify that we can retrieve the current hardware/mixing sample rate
// for all available input devices.
TEST(WinAudioInputTest, WASAPIAudioInputStreamHardwareSampleRate) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Retrieve a list of all available input devices.
  media::AudioDeviceNames device_names;
  audio_manager->GetAudioInputDeviceNames(&device_names);

  // Scan all available input devices and repeat the same test for all of them.
  for (media::AudioDeviceNames::const_iterator it = device_names.begin();
       it != device_names.end(); ++it) {
    // Retrieve the hardware sample rate given a specified audio input device.
    // TODO(tommi): ensure that we don't have to cast here.
    int fs = static_cast<int>(WASAPIAudioInputStream::HardwareSampleRate(
        it->unique_id));
    EXPECT_GE(fs, 0);
  }
}

// Test Create(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamCreateAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream(audio_manager.get());
  ais->Close();
}

// Test Open(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream(audio_manager.get());
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenStartAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream(audio_manager.get());
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenStartStopAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream(audio_manager.get());
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Stop();
  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();
}

// Test some additional calling sequences.
TEST(WinAudioInputTest, WASAPIAudioInputStreamMiscCallingSequences) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioInputStream* ais = CreateDefaultAudioInputStream(audio_manager.get());
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
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  int count = 0;
  MessageLoopForUI loop;

  // 10 ms packet size.

  // Create default WASAPI input stream which records in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioInputStreamWrapper aisw(audio_manager.get());
  AudioInputStream* ais = aisw.Create();
  EXPECT_TRUE(ais->Open());

  MockAudioInputCallback sink;

  // Derive the expected size in bytes of each recorded packet.
  uint32 bytes_per_packet = aisw.channels() * aisw.samples_per_packet() *
      (aisw.bits_per_sample() / 8);

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  EXPECT_CALL(sink, OnData(
      ais, NotNull(), bytes_per_packet, Gt(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();

  // Store current packet size (to be used in the subsequent tests).
  int samples_per_packet_10ms = aisw.samples_per_packet();

  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();

  // 20 ms packet size.

  count = 0;
  ais = aisw.Create(2 * samples_per_packet_10ms);
  EXPECT_TRUE(ais->Open());
  bytes_per_packet = aisw.channels() * aisw.samples_per_packet() *
      (aisw.bits_per_sample() / 8);

  EXPECT_CALL(sink, OnData(
      ais, NotNull(), bytes_per_packet, Gt(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();

  EXPECT_CALL(sink, OnClose(ais))
      .Times(1);
  ais->Close();

  // 5 ms packet size.

  count = 0;
  ais = aisw.Create(samples_per_packet_10ms / 2);
  EXPECT_TRUE(ais->Open());
  bytes_per_packet = aisw.channels() * aisw.samples_per_packet() *
    (aisw.bits_per_sample() / 8);

  EXPECT_CALL(sink, OnData(
      ais, NotNull(), bytes_per_packet, Gt(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
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
  scoped_ptr<AudioManager> audio_manager(AudioManager::Create());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  // Name of the output PCM file containing captured data. The output file
  // will be stored in the directory containing 'media_unittests.exe'.
  // Example of full name: \src\build\Debug\out_stereo_10sec.pcm.
  const char* file_name = "out_stereo_10sec.pcm";

  AudioInputStreamWrapper aisw(audio_manager.get());
  AudioInputStream* ais = aisw.Create();
  EXPECT_TRUE(ais->Open());

  LOG(INFO) << ">> Sample rate: " << aisw.sample_rate() << " [Hz]";
  WriteToFileAudioSink file_sink(file_name);
  LOG(INFO) << ">> Speak into the default microphone while recording.";
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  LOG(INFO) << ">> Recording has stopped.";
  ais->Close();
}

}  // namespace media
