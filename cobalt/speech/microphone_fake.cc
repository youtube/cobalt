// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/speech/microphone_fake.h"

#if defined(ENABLE_FAKE_MICROPHONE)

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "cobalt/audio/audio_file_reader.h"
#include "starboard/common/file.h"
#include "starboard/memory.h"
#include "starboard/time.h"

namespace cobalt {
namespace speech {

typedef audio::AudioBus AudioBus;

namespace {

const int kMaxBufferSize = 1024 * 1024;
const int kMinMicrophoneReadInBytes = 1024;
// The possibility of microphone creation failed is 1/20.
const int kCreationRange = 20;
// The possibility of microphone open failed is 1/20.
const int kOpenRange = 20;
// The possibility of microphone read failed is 1/300.
const int kReadRange = 300;
// The possibility of microphone close failed is 1/20.
const int kCloseRange = 20;
const int kFailureNumber = 5;
const int kSupportedMonoChannel = 1;
const char kMicrophoneLabel[] = "FakeMicrophone";

bool ShouldFail(int range) {
  return base::RandGenerator(range) == kFailureNumber;
}

}  // namespace

MicrophoneFake::MicrophoneFake(const Options& options)
    : Microphone(),
      read_data_from_file_(options.audio_data_size == 0),
      file_length_(-1),
      read_index_(0),
      is_valid_(!ShouldFail(kCreationRange)) {
  if (!is_valid_) {
    DLOG(WARNING) << "Mocking microphone creation failed.";
    return;
  }

  if (read_data_from_file_) {
    if (options.file_path) {
      DCHECK(!options.file_path->empty());
      // External input file.
      file_paths_.push_back(options.file_path.value());
    } else {
      base::FilePath audio_files_path;
      CHECK(base::PathService::Get(base::DIR_TEST_DATA, &audio_files_path));
      audio_files_path = audio_files_path.Append(FILE_PATH_LITERAL("cobalt"))
                             .Append(FILE_PATH_LITERAL("speech"))
                             .Append(FILE_PATH_LITERAL("testdata"));

      base::FileEnumerator file_enumerator(audio_files_path,
                                           false /* Not recursive */,
                                           base::FileEnumerator::FILES);
      for (base::FilePath next = file_enumerator.Next(); !next.empty();
           next = file_enumerator.Next()) {
        file_paths_.push_back(next);
      }
    }
  } else {
    file_length_ = std::min(options.audio_data_size, kMaxBufferSize);
    DCHECK_GT(file_length_, 0);
    audio_bus_.reset(
        new AudioBus(kSupportedMonoChannel,
                     file_length_ / audio::GetSampleTypeSize(AudioBus::kInt16),
                     AudioBus::kInt16, AudioBus::kInterleaved));
    memcpy(audio_bus_->interleaved_data(), options.external_audio_data,
                 file_length_);
  }
}

bool MicrophoneFake::Open() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (ShouldFail(kOpenRange)) {
    DLOG(WARNING) << "Mocking microphone open failed.";
    return false;
  }

  if (read_data_from_file_) {
    // Read from local files.
    DCHECK_NE(file_paths_.size(), 0u);
    size_t random_index =
        static_cast<size_t>(base::RandGenerator(file_paths_.size()));
    starboard::ScopedFile file(file_paths_[random_index].value().c_str(),
                               kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    DCHECK(file.IsValid());
    int file_buffer_size =
        std::min(static_cast<int>(file.GetSize()), kMaxBufferSize);
    DCHECK_GT(file_buffer_size, 0);

    std::unique_ptr<char[]> audio_input(new char[file_buffer_size]);
    int read_bytes = file.ReadAll(audio_input.get(), file_buffer_size);
    if (read_bytes < 0) {
      return false;
    }

    std::unique_ptr<audio::AudioFileReader> reader(
        audio::AudioFileReader::TryCreate(
            reinterpret_cast<const uint8*>(audio_input.get()), file_buffer_size,
            audio::kSampleTypeInt16));
    const float kSupportedSampleRate = 16000.0f;
    if (!reader) {
      // If it is not a WAV file, read audio data as raw audio.
      audio_bus_.reset(new AudioBus(
          kSupportedMonoChannel,
          file_buffer_size / audio::GetSampleTypeSize(AudioBus::kInt16),
          AudioBus::kInt16, AudioBus::kInterleaved));
      memcpy(audio_bus_->interleaved_data(), audio_input.get(),
                   file_buffer_size);
      file_length_ = file_buffer_size;
    } else if (reader->sample_type() != AudioBus::kInt16 ||
               reader->sample_rate() != kSupportedSampleRate ||
               reader->number_of_channels() != kSupportedMonoChannel) {
      // If it is a WAV file but it doesn't meet the audio input criteria, treat
      // it as an error.
      return false;
    } else {
      audio_bus_ = reader->ResetAndReturnAudioBus();
      file_length_ =
          static_cast<int>(reader->number_of_frames() *
                           audio::GetSampleTypeSize(reader->sample_type()));
    }
  }
  return true;
}

int MicrophoneFake::Read(char* out_data, int data_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (ShouldFail(kReadRange)) {
    DLOG(WARNING) << "Mocking microphone read failed.";
    return -1;
  }

  int copy_bytes = std::min(file_length_ - read_index_, data_size);
  memcpy(out_data, audio_bus_->interleaved_data() + read_index_,
               copy_bytes);
  read_index_ += copy_bytes;
  if (read_index_ == file_length_) {
    read_index_ = 0;
  }

  return copy_bytes;
}

bool MicrophoneFake::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (read_data_from_file_) {
    audio_bus_.reset();
    file_length_ = -1;
  }

  read_index_ = 0;

  if (ShouldFail(kCloseRange)) {
    DLOG(WARNING) << "Mocking microphone close failed.";
    return false;
  }

  return true;
}

int MicrophoneFake::MinMicrophoneReadInBytes() {
  return kMinMicrophoneReadInBytes;
}

const char* MicrophoneFake::Label() { return kMicrophoneLabel; }

}  // namespace speech
}  // namespace cobalt

#endif  // defined(ENABLE_FAKE_MICROPHONE)
