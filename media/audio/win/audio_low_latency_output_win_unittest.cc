// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/path_service.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/base/seekable_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock_mutant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Between;
using ::testing::CreateFunctor;
using ::testing::DoAll;
using ::testing::Gt;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using base::win::ScopedCOMInitializer;

namespace media {

static const char kSpeechFile_16b_s_48k[] = "speech_16b_stereo_48kHz.raw";
static const char kSpeechFile_16b_s_44k[] = "speech_16b_stereo_44kHz.raw";
static const size_t kFileDurationMs = 20000;

static const size_t kMaxDeltaSamples = 1000;
static const char* kDeltaTimeMsFileName = "delta_times_ms.txt";

MATCHER_P(HasValidDelay, value, "") {
  // It is difficult to come up with a perfect test condition for the delay
  // estimation. For now, verify that the produced output delay is always
  // larger than the selected buffer size.
  return arg.hardware_delay_bytes > value.hardware_delay_bytes;
}

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD4(OnMoreData, uint32(AudioOutputStream* stream,
                                  uint8* dest,
                                  uint32 max_size,
                                  AudioBuffersState buffers_state));
  MOCK_METHOD2(OnError, void(AudioOutputStream* stream, int code));
};

// This audio source implementation should be used for manual tests only since
// it takes about 20 seconds to play out a file.
class ReadFromFileAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit ReadFromFileAudioSource(const std::string& name)
    : pos_(0),
      previous_call_time_(base::Time::Now()),
      text_file_(NULL),
      elements_to_write_(0) {
    // Reads a test file from media/test/data directory and stores it in
    // a scoped_array.
    ReadTestDataFile(name, &file_, &file_size_);
    file_size_ = file_size_;

    // Creates an array that will store delta times between callbacks.
    // The content of this array will be written to a text file at
    // destruction and can then be used for off-line analysis of the exact
    // timing of callbacks. The text file will be stored in media/test/data.
    delta_times_.reset(new int[kMaxDeltaSamples]);
  }

  virtual ~ReadFromFileAudioSource() {
    // Get complete file path to output file in directory containing
    // media_unittests.exe.
    FilePath file_name;
    EXPECT_TRUE(PathService::Get(base::DIR_EXE, &file_name));
    file_name = file_name.AppendASCII(kDeltaTimeMsFileName);

    EXPECT_TRUE(!text_file_);
    text_file_ = file_util::OpenFile(file_name, "wt");
    DLOG_IF(ERROR, !text_file_) << "Failed to open log file.";

    // Write the array which contains delta times to a text file.
    size_t elements_written = 0;
    while (elements_written < elements_to_write_) {
      fprintf(text_file_, "%d\n", delta_times_[elements_written]);
      ++elements_written;
    }

    file_util::CloseFile(text_file_);
  }

  // AudioOutputStream::AudioSourceCallback implementation.
  virtual uint32 OnMoreData(AudioOutputStream* stream,
                            uint8* dest,
                            uint32 max_size,
                            AudioBuffersState buffers_state) {
    // Store time difference between two successive callbacks in an array.
    // These values will be written to a file in the destructor.
    int diff = (base::Time::Now() - previous_call_time_).InMilliseconds();
    previous_call_time_ = base::Time::Now();
    if (elements_to_write_ < kMaxDeltaSamples) {
      delta_times_[elements_to_write_] = diff;
      ++elements_to_write_;
    }

    // Use samples read from a data file and fill up the audio buffer
    // provided to us in the callback.
    if (pos_ + static_cast<int>(max_size) > file_size_)
      max_size = file_size_ - pos_;
    if (max_size) {
      memcpy(dest, &file_[pos_], max_size);
      pos_ += max_size;
    }
    return max_size;
  }

  virtual void OnError(AudioOutputStream* stream, int code) {}

  int file_size() { return file_size_; }

 private:
  scoped_array<uint8> file_;
  scoped_array<int> delta_times_;
  int file_size_;
  int pos_;
  base::Time previous_call_time_;
  FILE* text_file_;
  size_t elements_to_write_;
};

// Convenience method which ensures that we are not running on the build
// bots and that at least one valid output device can be found.
static bool CanRunAudioTests() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return false;
  AudioManager* audio_man = AudioManager::GetAudioManager();
  if (NULL == audio_man)
    return false;
  // TODO(henrika): note that we use Wave today to query the number of
  // existing output devices.
  return audio_man->HasAudioOutputDevices();
}

// Convenience method which creates a default AudioOutputStream object but
// also allows the user to modify the default settings.
class AudioOutputStreamWrapper {
 public:
  AudioOutputStreamWrapper()
      : com_init_(ScopedCOMInitializer::kMTA),
        audio_man_(AudioManager::GetAudioManager()),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
        channel_layout_(CHANNEL_LAYOUT_STEREO),
        bits_per_sample_(16) {
    // Use native/mixing sample rate and 10ms frame size as default.
    sample_rate_ = static_cast<int>(
        WASAPIAudioOutputStream::HardwareSampleRate(eConsole));
    samples_per_packet_ = sample_rate_ / 100;
    DCHECK(sample_rate_);
  }

  ~AudioOutputStreamWrapper() {}

  // Creates AudioOutputStream object using default parameters.
  AudioOutputStream* Create() {
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // frame size is modified.
  AudioOutputStream* Create(int samples_per_packet) {
    samples_per_packet_ = samples_per_packet;
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // channel layout is modified.
  AudioOutputStream* Create(ChannelLayout channel_layout) {
    channel_layout_ = channel_layout;
    return CreateOutputStream();
  }

  AudioParameters::Format format() const { return format_; }
  int channels() const { return ChannelLayoutToChannelCount(channel_layout_); }
  int bits_per_sample() const { return bits_per_sample_; }
  int sample_rate() const { return sample_rate_; }
  int samples_per_packet() const { return samples_per_packet_; }

 private:
  AudioOutputStream* CreateOutputStream() {
    AudioOutputStream* aos = audio_man_->MakeAudioOutputStream(
        AudioParameters(format_, channel_layout_, sample_rate_,
                        bits_per_sample_, samples_per_packet_));
    EXPECT_TRUE(aos);
    return aos;
  }

  ScopedCOMInitializer com_init_;
  AudioManager* audio_man_;
  AudioParameters::Format format_;
  ChannelLayout channel_layout_;
  int bits_per_sample_;
  int sample_rate_;
  int samples_per_packet_;
};

// Convenience method which creates a default AudioOutputStream object.
static AudioOutputStream* CreateDefaultAudioOutputStream() {
  AudioOutputStreamWrapper aosw;
  AudioOutputStream* aos = aosw.Create();
  return aos;
}

static void QuitMessageLoop(base::MessageLoopProxy* proxy) {
  proxy->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

// Verify that we can retrieve the current hardware/mixing sample rate
// for all supported device roles. The ERole enumeration defines constants
// that indicate the role that the system/user has assigned to an audio
// endpoint device.
// TODO(henrika): modify this test when we support full device enumeration.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestHardwareSampleRate) {
  if (!CanRunAudioTests())
    return;

  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Default device intended for games, system notification sounds,
  // and voice commands.
  int fs = static_cast<int>(
      WASAPIAudioOutputStream::HardwareSampleRate(eConsole));
  EXPECT_GE(fs, 0);

  // Default communication device intended for e.g. VoIP communication.
  fs = static_cast<int>(
      WASAPIAudioOutputStream::HardwareSampleRate(eCommunications));
  EXPECT_GE(fs, 0);

  // Multimedia device for music, movies and live music recording.
  fs = static_cast<int>(
      WASAPIAudioOutputStream::HardwareSampleRate(eMultimedia));
  EXPECT_GE(fs, 0);
}

// Test Create(), Close() calling sequence.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestCreateAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream();
  aos->Close();
}

// Test Open(), Close() calling sequence.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestOpenAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream();
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestOpenStartAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream();
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(aos, _))
      .Times(0);
  aos->Start(&source);
  aos->Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestOpenStartStopAndClose) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream();
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(aos, _))
      .Times(0);
  aos->Start(&source);
  aos->Stop();
  aos->Close();
}

// Test SetVolume(), GetVolume()
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestVolume) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream();

  // Initial volume should be full volume (1.0).
  double volume = 0.0;
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  // Verify some valid volume settings.
  aos->SetVolume(0.0);
  aos->GetVolume(&volume);
  EXPECT_EQ(0.0, volume);

  aos->SetVolume(0.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(0.5, volume);

  aos->SetVolume(1.0);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  // Ensure that invalid volume setting have no effect.
  aos->SetVolume(1.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  aos->SetVolume(-0.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  aos->Close();
}

// Test some additional calling sequences.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestMiscCallingSequences) {
  if (!CanRunAudioTests())
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream();
  WASAPIAudioOutputStream* waos = static_cast<WASAPIAudioOutputStream*>(aos);

  // Open(), Open() is a valid calling sequence (second call does nothing).
  EXPECT_TRUE(aos->Open());
  EXPECT_TRUE(aos->Open());

  MockAudioSourceCallback source;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Start(&source);
  EXPECT_TRUE(waos->started());

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  aos->Stop();
  EXPECT_FALSE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());

  aos->Close();
}

// Use default packet size (10ms) and verify that rendering starts.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestPacketSizeInMilliseconds) {
  if (!CanRunAudioTests())
    return;

  MessageLoopForUI loop;
  scoped_refptr<base::MessageLoopProxy> proxy(loop.message_loop_proxy());

  MockAudioSourceCallback source;

  // Create default WASAPI output stream which plays out in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioOutputStreamWrapper aosw;
  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());

  // Derive the expected size in bytes of each packet.
  uint32 bytes_per_packet = aosw.channels() * aosw.samples_per_packet() *
                           (aosw.bits_per_sample() / 8);

  // Set up expected minimum delay estimation.
  AudioBuffersState state(0, bytes_per_packet);

  // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(aos, NotNull(), bytes_per_packet,
                                 HasValidDelay(state)))
      .WillOnce(
          DoAll(
              InvokeWithoutArgs(
                  CreateFunctor(&QuitMessageLoop, proxy.get())),
              Return(bytes_per_packet)));

  aos->Start(&source);
  loop.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                       TestTimeouts::action_timeout_ms());
  loop.Run();
  aos->Stop();
  aos->Close();
}

// Use a fixed packets size (independent of sample rate) and verify
// that rendering starts.
TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestPacketSizeInSamples) {
  if (!CanRunAudioTests())
    return;

  MessageLoopForUI loop;
  scoped_refptr<base::MessageLoopProxy> proxy(loop.message_loop_proxy());

  MockAudioSourceCallback source;

  // Create default WASAPI output stream which plays out in stereo using
  // the shared mixing rate. The buffer size is set to 1024 samples.
  AudioOutputStreamWrapper aosw;
  AudioOutputStream* aos = aosw.Create(1024);
  EXPECT_TRUE(aos->Open());

  // Derive the expected size in bytes of each packet.
  uint32 bytes_per_packet = aosw.channels() * aosw.samples_per_packet() *
                           (aosw.bits_per_sample() / 8);

  // Set up expected minimum delay estimation.
  AudioBuffersState state(0, bytes_per_packet);

  // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(aos, NotNull(), bytes_per_packet,
                                 HasValidDelay(state)))
      .WillOnce(
          DoAll(
              InvokeWithoutArgs(
                  CreateFunctor(&QuitMessageLoop, proxy.get())),
              Return(bytes_per_packet)));

  aos->Start(&source);
  loop.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                       TestTimeouts::action_timeout_ms());
  loop.Run();
  aos->Stop();
  aos->Close();
}

TEST(WinAudioOutputTest, WASAPIAudioOutputStreamTestMono) {
  if (!CanRunAudioTests())
    return;

  MessageLoopForUI loop;
  scoped_refptr<base::MessageLoopProxy> proxy(loop.message_loop_proxy());

  MockAudioSourceCallback source;

  // Create default WASAPI output stream which plays out in *mono* using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioOutputStreamWrapper aosw;
  AudioOutputStream* aos = aosw.Create(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(aos->Open());

  // Derive the expected size in bytes of each packet.
  uint32 bytes_per_packet = aosw.channels() * aosw.samples_per_packet() *
                           (aosw.bits_per_sample() / 8);

  // Set up expected minimum delay estimation.
  AudioBuffersState state(0, bytes_per_packet);

  EXPECT_CALL(source, OnMoreData(aos, NotNull(), bytes_per_packet,
                                 HasValidDelay(state)))
      .WillOnce(
          DoAll(
              InvokeWithoutArgs(
                  CreateFunctor(&QuitMessageLoop, proxy.get())),
              Return(bytes_per_packet)));

  aos->Start(&source);
  loop.PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(),
                       TestTimeouts::action_timeout_ms());
  loop.Run();
  aos->Stop();
  aos->Close();
}

// This test is intended for manual tests and should only be enabled
// when it is required to store the captured data on a local file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
// The test files are approximately 20 seconds long.
TEST(WinAudioOutputTest, DISABLED_WASAPIAudioOutputStreamReadFromFile) {
  if (!CanRunAudioTests())
    return;

  AudioOutputStreamWrapper aosw;
  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());

  std::string file_name;
  if (aosw.sample_rate() == 48000) {
    file_name = kSpeechFile_16b_s_48k;
  } else if (aosw.sample_rate() == 44100) {
    file_name = kSpeechFile_16b_s_44k;
  } else if (aosw.sample_rate() == 96000) {
    // Use 48kHz file at 96kHz as well. Will sound like Donald Duck.
    file_name = kSpeechFile_16b_s_48k;
  } else {
    FAIL() << "This test supports 44.1, 48kHz and 96kHz only.";
    return;
  }
  ReadFromFileAudioSource file_source(file_name);
  int file_duration_ms = kFileDurationMs;

  LOG(INFO) << "File name  : " << file_name.c_str();
  LOG(INFO) << "Sample rate: " << aosw.sample_rate();
  LOG(INFO) << "File size  : " << file_source.file_size();
  LOG(INFO) << ">> Listen to the file while playing...";

  aos->Start(&file_source);
  base::PlatformThread::Sleep(file_duration_ms);
  aos->Stop();

  LOG(INFO) << ">> File playout has stopped.";
  aos->Close();
}

}  // namespace media
