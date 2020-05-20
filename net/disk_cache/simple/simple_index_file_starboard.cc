// Copyright 2019 Google Inc. All Rights Reserved.
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

#include "net/disk_cache/simple/simple_index_file.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/types.h"

namespace disk_cache {

// static
bool SimpleIndexFile::TraverseCacheDirectory(
    const base::FilePath& cache_path,
    const EntryFileCallback& entry_file_callback) {
  SbFileError error;
  SbDirectory dir = SbDirectoryOpen(cache_path.value().c_str(), &error);
  if (dir == kSbDirectoryInvalid) {
    PLOG(ERROR) << "opendir " << cache_path.value() << ", erron: " << error;
    return false;
  }
#if SB_API_VERSION >= 12
  std::vector<char> entry(kSbFileMaxName);

  while (true) {
    if (!SbDirectoryGetNext(dir, entry.data(), entry.size())) {
      PLOG(ERROR) << "readdir " << cache_path.value();
      return false;
    }

    const std::string file_name(entry.data());
#else   // SB_API_VERSION >= 12
  SbDirectoryEntry entry;

  while (true) {
    if (!SbDirectoryGetNext(dir, &entry)) {
      PLOG(ERROR) << "readdir " << cache_path.value();
      return false;
    }

    const std::string file_name(entry.name);
#endif  // SB_API_VERSION >= 12
    if (file_name == "." || file_name == "..")
      continue;
    const base::FilePath file_path =
        cache_path.Append(base::FilePath(file_name));
    base::File::Info file_info;
    if (!base::GetFileInfo(file_path, &file_info)) {
      LOG(ERROR) << "Could not get file info for " << file_path.value();
      continue;
    }

    entry_file_callback.Run(file_path, file_info.last_accessed,
                            file_info.last_modified, file_info.size);
  }

  return true;
}

}  // namespace disk_cache
