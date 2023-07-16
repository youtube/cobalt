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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILE_CACHE_READER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILE_CACHE_READER_H_

#include <memory>
#include <string>
#include <vector>

#include "starboard/common/file.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

class FileCacheReader {
 public:
  // The class assumes that |filename| is stored in
  // "<content path>/test/starboard/shared/starboard/player/testdata".
  // Currently it's only used by media related tests to read dmp files.
  FileCacheReader(const char* filename, int file_cache_size);

  int Read(void* out_buffer, int bytes_to_read);
  const std::string& GetAbsolutePathName() const;
  int64_t GetSize();

 private:
  // This function CHECK() if file is successfully opened.
  void EnsureFileOpened();

  // Reads from the currently cached file contents, into the output buffer.
  // Returns the final number of bytes read out from the cache.
  int ReadFromCache(void* out_buffer, int bytes_to_read);

  // Reads from the file into the intermediate cache, if the intermediate cache
  // is emptied out.
  void RefillCacheIfEmpty();

  const std::string absolute_path_;
  const int default_file_cache_size_;

  std::unique_ptr<ScopedFile> file_;

  // Maximum size of the buffered file.
  int max_file_cache_size_ = 0;

  // Position marker in the buffer that we have finished reading.
  int file_cache_offset_ = 0;

  // The intermediate buffer that will hold file contents as needed by
  // parsing.
  std::vector<char> file_cache_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILE_CACHE_READER_H_
