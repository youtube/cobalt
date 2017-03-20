// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "cobalt/audio/audio_file_reader.h"
#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/time.h"

namespace cobalt {
namespace speech {

namespace {
const int kMaxBufferSize = 1024 * 1024;
const int kMinMicrophoneReadInBytes = 1024;
// The possiblity of microphone creation failed is 1/20.
const int kCreationRange = 20;
// The possiblity of microphone open failed is 1/20.
const int kOpenRange = 20;
// The possiblity of microphone read failed is 1/300.
const int kReadRange = 300;
// The possiblity of microphone close failed is 1/20.
const int kCloseRange = 20;
const int kFailureNumber = 5;

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
    SB_DLOG(WARNING) << "Mocking microphone creation failed.";
    return;
  }

  if (read_data_from_file_) {
    if (options.file_path) {
      SB_DCHECK(!options.file_path->empty());
      // External input file.
      file_paths_.push_back(options.file_path.value());
    } else {
      FilePath audio_files_path;
      SB_CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &audio_files_path));
      audio_files_path = audio_files_path.Append(FILE_PATH_LITERAL("cobalt"))
                             .Append(FILE_PATH_LITERAL("speech"))
                             .Append(FILE_PATH_LITERAL("testdata"));

      file_util::FileEnumerator file_enumerator(
          audio_files_path, false /* Not recursive */,
          file_util::FileEnumerator::FILES);
      for (FilePath next = file_enumerator.Next(); !next.empty();
           next = file_enumerator.Next()) {
        file_paths_.push_back(next);
      }
    }
  } else {
    file_length_ = std::min(options.audio_data_size, kMaxBufferSize);
    SB_DCHECK(file_length_ > 0);
    file_buffer_.reset(new uint8[file_length_]);
    SbMemoryCopy(file_buffer_.get(), options.external_audio_data, file_length_);
  }
}

bool MicrophoneFake::Open() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldFail(kOpenRange)) {
    SB_DLOG(WARNING) << "Mocking microphone open failed.";
    return false;
  }

  if (read_data_from_file_) {
    // Read from local files.
    SB_DCHECK(file_paths_.size() != 0);
    size_t random_index =
        static_cast<size_t>(base::RandGenerator(file_paths_.size()));
    starboard::ScopedFile file(file_paths_[random_index].value().c_str(),
                               kSbFileOpenOnly | kSbFileRead, NULL, NULL);
    SB_DCHECK(file.IsValid());
    int file_buffer_size =
        std::min(static_cast<int>(file.GetSize()), kMaxBufferSize);
    SB_DCHECK(file_buffer_size > 0);

    scoped_array<char> audio_input(new char[file_buffer_size]);
    int read_bytes = file.ReadAll(audio_input.get(), file_buffer_size);
    if (read_bytes < 0) {
      return false;
    }

    scoped_ptr<audio::AudioFileReader> reader(audio::AudioFileReader::TryCreate(
        reinterpret_cast<const uint8*>(audio_input.get()), file_buffer_size,
        audio::kSampleTypeInt16));
    const float kSupportedSampleRate = 16000.0f;
    const int kSupportedMonoChannel = 1;
    if (!reader) {
      // If it is not a WAV file, read audio data as raw audio.
      file_buffer_.reset(new uint8[file_buffer_size]);
      SbMemoryCopy(file_buffer_.get(), audio_input.get(), file_buffer_size);
      file_length_ = file_buffer_size;
    } else if (reader->sample_type() != ::media::ShellAudioBus::kInt16 ||
               reader->sample_rate() != kSupportedSampleRate ||
               reader->number_of_channels() != kSupportedMonoChannel) {
      // If it is a WAV file but it doesn't meet the audio input criteria, treat
      // it as an error.
      return false;
    } else {
      // Read WAV PCM16 audio data.
      int size =
          static_cast<int>(reader->number_of_frames() *
                           audio::GetSampleTypeSize(reader->sample_type()));
      file_buffer_.reset(new uint8[size]);
      SbMemoryCopy(file_buffer_.get(), reader->sample_data().get(), size);
      file_length_ = size;
    }
  }
  return true;
}

int MicrophoneFake::Read(char* out_data, int data_size) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldFail(kReadRange)) {
    SB_DLOG(WARNING) << "Mocking microphone read failed.";
    return -1;
  }

  int copy_bytes = std::min(file_length_ - read_index_, data_size);
  SbMemoryCopy(out_data, file_buffer_.get() + read_index_, copy_bytes);
  read_index_ += copy_bytes;
  if (read_index_ == file_length_) {
    read_index_ = 0;
  }

  return copy_bytes;
}

bool MicrophoneFake::Close() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (read_data_from_file_) {
    file_buffer_.reset();
    file_length_ = -1;
  }

  read_index_ = 0;

  if (ShouldFail(kCloseRange)) {
    SB_DLOG(WARNING) << "Mocking microphone close failed.";
    return false;
  }

  return true;
}

int MicrophoneFake::MinMicrophoneReadInBytes() {
  return kMinMicrophoneReadInBytes;
}

}  // namespace speech
}  // namespace cobalt

#endif  // defined(ENABLE_FAKE_MICROPHONE)
