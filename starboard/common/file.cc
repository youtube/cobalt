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

#include <cstring>
#include <string>
#include <vector>

#include "dirent.h"
#include "starboard/common/log.h"
#include "starboard/common/metrics/stats_tracker.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/string.h"

namespace starboard {
namespace {

bool DirectoryCloseLogFailure(const char* path, DIR* dir) {
  if (!closedir(dir)) {
    SB_LOG(ERROR) << "Failed to close directory: '" << path << "'";
    return false;
  }
  return true;
}

}  // namespace

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
  if (!SbFileExists(path)) {
    SB_LOG(ERROR) << "Path does not exist: '" << path << "'";
    return false;
  }

  SbFileError err = kSbFileOk;
  DIR* dir = opendir(path);

  // The |path| points to a file. Remove it and return.
  if (err != kSbFileOk) {
    return SbFileDelete(path);
  }

  SbFileInfo info;

  std::vector<char> entry(kSbFileMaxName);

  struct dirent dirent_buffer;
  struct dirent* dirent;
  while (readdir_r(dir, &dirent_buffer, &dirent)) {
    if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
      continue;
    }

    std::string abspath(path);
    abspath.append(kSbFileSepString);
    abspath.append(dirent->d_name);

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

}  // namespace starboard
