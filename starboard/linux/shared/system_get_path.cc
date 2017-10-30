// Copyright 2015 Google Inc. All Rights Reserved.
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
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

#include "starboard/directory.h"
#include "starboard/log.h"
#include "starboard/string.h"
#include "starboard/user.h"

namespace {
const int kMaxPathSize = SB_FILE_MAX_PATH;

// Gets the path to the cache directory, using the user's home directory.
bool GetCacheDirectory(char* out_path, int path_size) {
  char home_path[kMaxPathSize + 1];
  if (!SbUserGetProperty(SbUserGetCurrent(), kSbUserPropertyHomeDirectory,
                         home_path, kMaxPathSize)) {
    return false;
  }
  int result = SbStringFormatF(out_path, path_size, "%s/.cache", home_path);
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }
  return SbDirectoryCreate(out_path);
}

// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (path_size < 1) {
    return false;
  }

  char path[kMaxPathSize + 1];
  size_t bytes_read = readlink("/proc/self/exe", path, kMaxPathSize);
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

  char* last_slash =
      const_cast<char *>(SbStringFindLastCharacter(out_path, '/'));
  if (!last_slash) {
    return false;
  }

  *last_slash = '\0';
  return true;
}

// Gets only the name portion of the current executable.
bool GetExecutableName(char* out_path, int path_size) {
  char path[kMaxPathSize] = {0};
  if (!GetExecutablePath(path, kMaxPathSize)) {
    return false;
  }

  const char* last_slash = SbStringFindLastCharacter(path, '/');
  if (SbStringCopy(out_path, last_slash + 1, path_size) >= path_size) {
    return false;
  }
  return true;
}

// Gets the path to a temporary directory that is unique to this process.
bool GetTemporaryDirectory(char* out_path, int path_size) {
  char binary_name[kMaxPathSize] = {0};
  if (!GetExecutableName(binary_name, kMaxPathSize)) {
    return false;
  }

  int result = SbStringFormatF(out_path, path_size, "/tmp/%s-%d", binary_name,
                               static_cast<int>(getpid()));
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }

  return true;
}
}  // namespace

bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  const int kPathSize = SB_FILE_MAX_PATH;
  char path[kPathSize];
  path[0] = '\0';

  switch (path_id) {
    case kSbSystemPathContentDirectory:
      if (!GetExecutableDirectory(path, kPathSize)) {
        return false;
      }
      if (SbStringConcat(path, "/content/data", kPathSize) >= kPathSize) {
        return false;
      }
      break;
    case kSbSystemPathCacheDirectory:
      if (!GetCacheDirectory(path, kPathSize)) {
        return false;
      }
      if (SbStringConcat(path, "/cobalt", kPathSize) >= kPathSize) {
        return false;
      }
      if (!SbDirectoryCreate(path)) {
        return false;
      }
      break;
    case kSbSystemPathDebugOutputDirectory:
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize)) {
        return false;
      }
      if (SbStringConcat(path, "/log", kPathSize) >= kPathSize) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    case kSbSystemPathSourceDirectory:
      if (!GetExecutableDirectory(path, kPathSize)) {
        return false;
      }
      if (SbStringConcat(path, "/content/dir_source_root", kPathSize) >=
          kPathSize) {
        return false;
      }
      break;
    case kSbSystemPathTempDirectory:
      if (!GetTemporaryDirectory(path, kPathSize)) {
        return false;
      }

      SbDirectoryCreate(path);
      break;
    case kSbSystemPathTestOutputDirectory:
      return SbSystemGetPath(kSbSystemPathDebugOutputDirectory, out_path,
                             path_size);
    case kSbSystemPathExecutableFile:
      return GetExecutablePath(out_path, path_size);

  case kSbSystemPathFontConfigurationDirectory:
  case kSbSystemPathFontDirectory:
      return false;

    default:
      SB_NOTIMPLEMENTED() << "SbSystemGetPath not implemented for "
                          << path_id;
      return false;
  }

  int length = SbStringGetLength(path);
  if (length < 1 || length > path_size) {
    return false;
  }

  SbStringCopy(out_path, path, path_size);
  return true;
}
