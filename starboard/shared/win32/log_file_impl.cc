// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/log_file_impl.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/file_internal.h"

namespace {

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
int log_file = -1;

// SbMutex is not reentrant, so factor out close log file functionality for use
// by other functions.
void CloseLogFileWithoutLock() {
  if (starboard::IsValid(log_file)) {
    fsync(log_file);
    close(log_file);
    log_file = -1;
  }
}

}  // namespace

namespace starboard {
namespace shared {
namespace win32 {

void CloseLogFile() {
  pthread_mutex_lock(&log_mutex);
  CloseLogFileWithoutLock();
  pthread_mutex_unlock(&log_mutex);
}

void OpenLogInCacheDirectory(const char* log_file_name, int creation_flags) {
  SB_DCHECK((creation_flags & O_CREAT) ||
            ((creation_flags & O_CREAT) && (creation_flags & O_TRUNC)));
  SB_DCHECK(strlen(log_file_name) != 0);
  SB_DCHECK(strchr(log_file_name, kSbFileSepChar) == nullptr);
  std::vector<char> out_path(kSbFileMaxPath + 1);
  out_path[0] = '\0';

  const int path_size = static_cast<int>(out_path.size());
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, out_path.data(),
                       path_size)) {
    return;
  }

  if (starboard::strlcat(out_path.data(), kSbFileSepString, path_size) >=
      path_size) {
    return;
  }
  if (starboard::strlcat(out_path.data(), log_file_name, path_size) >=
      path_size) {
    return;
  }

  OpenLogFile(out_path.data(), creation_flags);
}

void OpenLogFile(const char* path, const int creation_flags) {
  SB_DCHECK(creation_flags & O_CREAT);
  SB_DLOG(INFO) << "Logging to [" << path << "]";

  int flags = creation_flags | O_WRONLY;

  pthread_mutex_lock(&log_mutex);
  CloseLogFileWithoutLock();
  if ((path != nullptr) && (path[0] != '\0')) {
    log_file = open(path, flags, S_IRUSR | S_IWUSR);
    SB_DCHECK(starboard::IsValid(log_file));
  }

  pthread_mutex_unlock(&log_mutex);
}

void WriteToLogFile(const char* text, const int text_length) {
  if (text_length <= 0) {
    return;
  }
  pthread_mutex_lock(&log_mutex);
  if (!starboard::IsValid(log_file)) {
    pthread_mutex_unlock(&log_mutex);
    return;
  }

  int bytes_written = starboard::WriteAll(log_file, text, text_length);
  RecordFileWriteStat(bytes_written);
  SB_DCHECK(text_length == bytes_written);

  fsync(log_file);
  pthread_mutex_unlock(&log_mutex);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
