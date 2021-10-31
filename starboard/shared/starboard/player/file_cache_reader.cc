// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/file_cache_reader.h"

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

FileCacheReader::FileCacheReader(const char* filename, int file_cache_size)
    : filename_(filename), default_file_cache_size_(file_cache_size) {}

int FileCacheReader::Read(void* out_buffer, int bytes_to_read) {
  EnsureFileOpened();

  int total_bytes_read = 0;

  while (bytes_to_read > 0 && file_cache_.size() != 0) {
    int bytes_read = ReadFromCache(
        static_cast<char*>(out_buffer) + total_bytes_read, bytes_to_read);
    SB_CHECK(bytes_read >= 0);
    bytes_to_read -= bytes_read;
    total_bytes_read += bytes_read;
    RefillCacheIfEmpty();
  }

  return total_bytes_read;
}

int64_t FileCacheReader::GetSize() {
  EnsureFileOpened();
  return file_->GetSize();
}

void FileCacheReader::EnsureFileOpened() {
  if (file_) {
    return;
  }
  file_.reset(new ScopedFile(filename_.c_str(), kSbFileOpenOnly | kSbFileRead));
  SB_CHECK(file_->IsValid());

  max_file_cache_size_ =
      std::min(default_file_cache_size_, static_cast<int>(file_->GetSize()));
  file_cache_.resize(max_file_cache_size_);
  file_cache_offset_ = max_file_cache_size_;
}

int FileCacheReader::ReadFromCache(void* out_buffer, int bytes_to_read) {
  SB_CHECK(file_cache_offset_ <= file_cache_.size());
  bytes_to_read = std::min(
      static_cast<int>(file_cache_.size()) - file_cache_offset_, bytes_to_read);
  memcpy(out_buffer, file_cache_.data() + file_cache_offset_,
               bytes_to_read);
  file_cache_offset_ += bytes_to_read;
  return bytes_to_read;
}

void FileCacheReader::RefillCacheIfEmpty() {
  if (file_cache_offset_ != file_cache_.size()) {
    return;
  }
  file_cache_offset_ = 0;
  int bytes_read = file_->ReadAll(file_cache_.data(), file_cache_.size());
  SB_CHECK(bytes_read >= 0);
  if (bytes_read < static_cast<int>(file_cache_.size())) {
    file_cache_.resize(bytes_read);
  }
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
