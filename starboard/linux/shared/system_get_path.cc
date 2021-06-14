// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#endif
#include "starboard/user.h"

namespace {
const int kMaxPathSize = kSbFileMaxPath;

// Gets the path to the cache directory, using the user's home directory.
bool GetCacheDirectory(char* out_path, int path_size) {
  std::vector<char> home_path(kMaxPathSize + 1);
  if (!SbUserGetProperty(SbUserGetCurrent(), kSbUserPropertyHomeDirectory,
                         home_path.data(), kMaxPathSize)) {
    return false;
  }
  int result =
      SbStringFormatF(out_path, path_size, "%s/.cache", home_path.data());
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }
  return SbDirectoryCreate(out_path);
}

// Gets the path to the storage directory, using the user's home directory.
bool GetStorageDirectory(char* out_path, int path_size) {
  std::vector<char> home_path(kMaxPathSize + 1);
  if (!SbUserGetProperty(SbUserGetCurrent(), kSbUserPropertyHomeDirectory,
                         home_path.data(), kMaxPathSize)) {
    return false;
  }
  int result = SbStringFormatF(out_path, path_size, "%s/.cobalt_storage",
                               home_path.data());
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

  std::vector<char> path(kMaxPathSize + 1);
  ssize_t bytes_read = readlink("/proc/self/exe", path.data(), kMaxPathSize);
  if (bytes_read < 1) {
    return false;
  }

  path[bytes_read] = '\0';
  if (bytes_read >= path_size) {
    return false;
  }

  starboard::strlcpy(out_path, path.data(), path_size);
  return true;
}

#if SB_IS(EVERGREEN_COMPATIBLE)
// May override the content path if there is EvergreenConfig published.
// The override allows for switching to different content paths based
// on the Evergreen binary executed.
// Returns false if it failed.
bool GetEvergreenContentPathOverride(char* out_path, int path_size) {
  const starboard::elf_loader::EvergreenConfig* evergreen_config =
      starboard::elf_loader::EvergreenConfig::GetInstance();
  if (!evergreen_config) {
    return true;
  }
  if (evergreen_config->content_path_.empty()) {
    return true;
  }

  if (starboard::strlcpy(out_path, evergreen_config->content_path_.c_str(),
                         path_size) >= path_size) {
    return false;
  }
  return true;
}
#endif

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
      const_cast<char *>(strrchr(out_path, '/'));
  if (!last_slash) {
    return false;
  }

  *last_slash = '\0';
  return true;
}

// Gets only the name portion of the current executable.
bool GetExecutableName(char* out_path, int path_size) {
  std::vector<char> path(kMaxPathSize, 0);
  if (!GetExecutablePath(path.data(), kMaxPathSize)) {
    return false;
  }

  const char* last_slash = strrchr(path.data(), '/');
  if (starboard::strlcpy(out_path, last_slash + 1, path_size) >= path_size) {
    return false;
  }
  return true;
}

// Gets the path to a temporary directory that is unique to this process.
bool GetTemporaryDirectory(char* out_path, int path_size) {
  std::vector<char> binary_name(kMaxPathSize, 0);
  if (!GetExecutableName(binary_name.data(), kMaxPathSize)) {
    return false;
  }

  int result = SbStringFormatF(out_path, path_size, "/tmp/%s-%d",
                               binary_name.data(), static_cast<int>(getpid()));
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }

  return true;
}

// Gets the path to the content directory.
bool GetContentDirectory(char* out_path, int path_size) {
  if (!GetExecutableDirectory(out_path, path_size)) {
    return false;
  }
  if (starboard::strlcat(out_path, "/content", path_size) >= path_size) {
    return false;
  }
  return true;
}
}  // namespace

bool SbSystemGetPath(SbSystemPathId path_id, char* out_path, int path_size) {
  if (!out_path || !path_size) {
    return false;
  }

  const int kPathSize = kSbFileMaxPath;
  std::vector<char> path(kPathSize);
  path[0] = '\0';

  switch (path_id) {
    case kSbSystemPathContentDirectory:
      if (!GetContentDirectory(path.data(), kPathSize)) {
        return false;
      }
#if SB_IS(EVERGREEN_COMPATIBLE)
      if (!GetEvergreenContentPathOverride(path.data(), kPathSize)) {
        return false;
      }

#endif
      break;

    case kSbSystemPathCacheDirectory:
      if (!GetCacheDirectory(path.data(), kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path.data(), "/cobalt", kPathSize) >= kPathSize) {
        return false;
      }
      if (!SbDirectoryCreate(path.data())) {
        return false;
      }
      break;

    case kSbSystemPathDebugOutputDirectory:
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path.data(),
                           kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path.data(), "/log", kPathSize) >= kPathSize) {
        return false;
      }
      SbDirectoryCreate(path.data());
      break;

    case kSbSystemPathTempDirectory:
      if (!GetTemporaryDirectory(path.data(), kPathSize)) {
        return false;
      }
      SbDirectoryCreate(path.data());
      break;

    case kSbSystemPathTestOutputDirectory:
      return SbSystemGetPath(kSbSystemPathDebugOutputDirectory, out_path,
                             path_size);

    case kSbSystemPathExecutableFile:
      return GetExecutablePath(out_path, path_size);

    case kSbSystemPathFontConfigurationDirectory:
    case kSbSystemPathFontDirectory:
#if SB_IS(EVERGREEN_COMPATIBLE)
      if (!GetContentDirectory(path.data(), kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path.data(), "/fonts", kPathSize) >= kPathSize) {
        return false;
      }
      break;
#else
      return false;
#endif

#if SB_API_VERSION >= 12
    case kSbSystemPathStorageDirectory:
      if (!GetStorageDirectory(path.data(), kPathSize)) {
        return false;
      }
      break;
#endif

    default:
      SB_NOTIMPLEMENTED() << "SbSystemGetPath not implemented for "
                          << path_id;
      return false;
  }

  int length = strlen(path.data());
  if (length < 1 || length > path_size) {
    return false;
  }

  starboard::strlcpy(out_path, path.data(), path_size);
  return true;
}
