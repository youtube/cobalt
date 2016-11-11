/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/speech/microphone_fake.h"

#if defined(ENABLE_FAKE_MICROPHONE)

#include <algorithm>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "starboard/file.h"
#include "starboard/memory.h"
#include "starboard/time.h"

namespace cobalt {
namespace speech {

namespace {
const int kMaxBufferSize = 512 * 1024;
const int kMinMicrophoneReadInBytes = 1024;
// The possiblity of microphone creation failed is 1/20.
const int kCreationRange = 20;
// The possiblity of microphone open failed is 1/20.
const int kOpenRange = 20;
// The possiblity of microphone read failed is 1/200.
const int kReadRange = 200;
// The possiblity of microphone close failed is 1/20.
const int kCloseRange = 20;
const int kFailureNumber = 5;

bool ShouldFail(int range) {
  return base::RandGenerator(range) == kFailureNumber;
}

}  // namespace

MicrophoneFake::MicrophoneFake()
    : Microphone(),
      file_length_(-1),
      read_index_(0),
      is_valid_(!ShouldFail(kCreationRange)) {
  if (!is_valid_) {
    SB_DLOG(WARNING) << "Mocking microphone creation failed.";
    return;
  }

  FilePath audio_files_path;
  SB_CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &audio_files_path));
  audio_files_path = audio_files_path.Append(FILE_PATH_LITERAL("cobalt"))
                         .Append(FILE_PATH_LITERAL("speech"))
                         .Append(FILE_PATH_LITERAL("testdata"));

  file_util::FileEnumerator file_enumerator(audio_files_path,
                                            false /* Not recursive */,
                                            file_util::FileEnumerator::FILES);
  for (FilePath next = file_enumerator.Next(); !next.empty();
       next = file_enumerator.Next()) {
    file_paths_.push_back(next);
  }
}

bool MicrophoneFake::Open() {
  if (ShouldFail(kOpenRange)) {
    SB_DLOG(WARNING) << "Mocking microphone open failed.";
    return false;
  }

  SB_DCHECK(file_paths_.size() != 0);
  uint64 random_index = base::RandGenerator(file_paths_.size());
  starboard::ScopedFile file(file_paths_[random_index].value().c_str(),
                             kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  SB_DCHECK(file.IsValid());
  int file_buffer_size =
      std::min(static_cast<int>(file.GetSize()), kMaxBufferSize);
  SB_DCHECK(file_buffer_size > 0);
  file_buffer_.reset(new char[file_buffer_size]);
  int read_bytes = file.ReadAll(file_buffer_.get(), file_buffer_size);
  if (read_bytes < 0) {
    return false;
  }

  file_length_ = read_bytes;
  return true;
}

int MicrophoneFake::Read(char* out_data, int data_size) {
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
  file_buffer_.reset();
  file_length_ = -1;
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
