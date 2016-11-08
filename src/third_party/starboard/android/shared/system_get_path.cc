// Copyright 2016 Google Inc. All Rights Reserved.
// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#include "starboard/system.h"

#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

#include "starboard/directory.h"
#include "starboard/string.h"
#include "third_party/starboard/android/shared/application_android.h"

namespace {
// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (path_size < 1) {
    return false;
  }

  char path[PATH_MAX + 1];
  size_t bytes_read = readlink("/proc/self/exe", path, PATH_MAX);
  if (bytes_read < 1) {
    return false;
  }

  path[bytes_read] = '\0';
  if (bytes_read > path_size) {
    return false;
  }

  SbStringCopy(out_path, path, path_size);
  return true;
}

// Places up to |path_size| - 1 characters of the path to the directory
// containing the current executable in |out_path|, ensuring it is
// NULL-terminated. Returns success status. The result being greater than
// |path_size| - 1 characters is a failure. |out_path| may be written to in
// unsuccessful cases.
bool GetExecutableDirectory(char* out_path, int path_size) {
  if (!GetExecutablePath(out_path, path_size)) {
    return false;
  }

  char* last_slash = strrchr(out_path, '/');
  if (!last_slash) {
    return false;
  }

  *last_slash = '\0';
  return true;
}
}  // namespace

bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  const char* external_files_dirs = starboard::android::shared::ApplicationAndroid::Get()->GetExternalFilesDir();

  const int kPathSize = PATH_MAX;
  char path[kPathSize];
  path[0] = '\0';

  int count = SbStringConcat(path, external_files_dirs, kPathSize);
  if(count >= kPathSize) {
    return false;
  }

  switch (path_id) {
    case kSbSystemPathContentDirectory: {
      if (SbStringConcat(path, "/content", kPathSize) >= kPathSize - count) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathCacheDirectory: { /* TODO: Use external cache dir */
      if (SbStringConcat(path, "/media/cache", kPathSize) >= kPathSize - count) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathDebugOutputDirectory: {
      if (SbStringConcat(path, "/log", kPathSize) >= kPathSize - count) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathSourceDirectory: {
      if (SbStringConcat(path, "/content/dir_source_root", kPathSize - count) >=
          kPathSize) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathTempDirectory: {
      if (SbStringConcat(path, "/tmp", kPathSize) >= kPathSize - count) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathTestOutputDirectory: { // TODO: Check what this path is for
      if (SbStringConcat(path, "/test", kPathSize) >= kPathSize - count) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    case kSbSystemPathExecutableFile: { // TODO: Check what this path is for
      if (SbStringConcat(path, "/exec", kPathSize) >= kPathSize - count) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    }

    default:
      return false;
  }

  int length = strlen(path);
  if (length < 1 || length > path_size) {
    return false;
  }

  SbStringCopy(out_path, path, path_size);
  return true;
}
