// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/file.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/metrics/stats_tracker.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/string.h"

namespace starboard {
namespace {

bool DirectoryCloseLogFailure(const char* path, DIR* dir) {
  if (closedir(dir) != 0) {
    SB_LOG(ERROR) << "Failed to close directory: '" << path << "'";
    return false;
  }
  return true;
}

}  // namespace

ssize_t ReadAll(int fd, void* data, int size) {
  if (fd < 0 || size < 0) {
    return -1;
  }
  ssize_t bytes_read = 0;
  ssize_t rv;
  do {
    // Needs to cast void* to char* as MSVC returns error for pointer
    // arithmetic.
    rv =
        read(fd, reinterpret_cast<char*>(data) + bytes_read, size - bytes_read);
    if (rv <= 0) {
      break;
    }
    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

void RecordFileWriteStat(int write_file_result) {
  auto& stats_tracker = StatsTrackerContainer::GetInstance()->stats_tracker();
  if (write_file_result <= 0) {
    stats_tracker.FileWriteFail();
  } else {
    stats_tracker.FileWriteSuccess();
    stats_tracker.FileWriteBytesWritten(/*bytes_written=*/write_file_result);
  }
}

bool SbFileDeleteRecursive(const char* path, bool preserve_root) {
  struct stat file_info;
  if (stat(path, &file_info) != 0) {
    SB_LOG(ERROR) << "Path does not exist: '" << path << "'";
    return false;
  }

  DIR* dir = opendir(path);

  // The |path| points to a file. Remove it and return.
  if (!dir) {
    return SbFileDelete(path);
  }

  SbFileInfo info;

  std::vector<char> entry(kSbFileMaxName);

  struct dirent dirent_buffer;
  struct dirent* dirent;
  while (true) {
    if (entry.size() < kSbFileMaxName || !dir || !entry.data()) {
      break;
    }
    int result = readdir_r(dir, &dirent_buffer, &dirent);
    if (result || !dirent) {
      break;
    }
    starboard::strlcpy(entry.data(), dirent->d_name, kSbFileMaxName);

    if (!strcmp(entry.data(), ".") || !strcmp(entry.data(), "..")) {
      continue;
    }

    std::string abspath(path);
    abspath.append(kSbFileSepString);
    abspath.append(entry.data());

    if (!SbFileDeleteRecursive(abspath.data(), false)) {
      DirectoryCloseLogFailure(path, dir);
      return false;
    }
  }

  // Don't forget to close and remove the directory before returning!
  if (DirectoryCloseLogFailure(path, dir)) {
    return preserve_root ? true : SbFileDelete(path);
  }
  return false;
}

ssize_t WriteAll(int file, const void* data, int size) {
  if (file < 0 || size < 0) {
    return -1;
  }
  ssize_t bytes_written = 0;
  ssize_t rv;
  do {
    rv = write(file, reinterpret_cast<const char*>(data) + bytes_written,
               size - bytes_written);
    if (rv <= 0) {
      break;
    }
    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

}  // namespace starboard
