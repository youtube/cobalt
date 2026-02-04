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
#include <dirent.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>

#include "starboard/configuration_constants.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#endif
#include "starboard/shared/starboard/get_home_directory.h"

namespace {

#if SB_IS(EVERGREEN_COMPATIBLE)
// May override the content path if there is EvergreenConfig published.
// The override allows for switching to different content paths based
// on the Evergreen binary executed.
// Returns false if it failed.
bool GetEvergreenContentPathOverride(char* out_path, int path_size) {
  const elf_loader::EvergreenConfig* evergreen_config =
     elf_loader::EvergreenConfig::GetInstance();
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

// Places up to |path_size| - 1 characters of the path to the current
// executable in |out_path|, ensuring it is NULL-terminated. Returns success
// status. The result being greater than |path_size| - 1 characters is a
// failure. |out_path| may be written to in unsuccessful cases.
bool GetExecutablePath(char* out_path, int path_size) {
  if (path_size < 1) {
    return false;
  }

  char path[kSbFileMaxPath + 1];
  ssize_t bytes_read = readlink("/proc/self/exe", path, kSbFileMaxPath);
  if (bytes_read < 1) {
    return false;
  }

  path[bytes_read] = '\0';
  if (bytes_read > path_size) {
    return false;
  }

  starboard::strlcpy<char>(out_path, path, path_size);
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
      const_cast<char*>(strrchr(out_path, '/'));
  if (!last_slash) {
    return false;
  }

  *last_slash = '\0';
  return true;
}

// Checks if content directory is valid.
bool IsValidContentDirectory(const char* path) {
  if (!path || path[0] == '\0') {
    return false;
  }

  // The content directory is assummed valid if it contains fonts/fonts.xml file.
  // TODO: we need to find better way to validate content directory since
  // below checking is just an assumption not a standard.
  const std::string testFontsFullPath = std::string(path) + "/fonts/fonts.xml";
  struct stat info;
  return (stat(testFontsFullPath.c_str(), &info) == 0);
}

// Gets the path to the content directory.
bool GetContentDirectory(char* out_path, int path_size) {
  // `COBALT_CONTENT_DIR` is used to provide the path of content directory in
  // PATH-like format. This is not a evergreen standard but required for RDK
  // plugin environment where cobalt plugin uses `COBALT_CONTENT_DIR` env to
  // set the content path.
  const char* paths = std::getenv("COBALT_CONTENT_DIR");
  if (paths && paths[0] != '\0') {
    // Treat the environment variable as PATH-like search variable
    std::stringstream pathsStream(paths);
    std::string contentPath;
    while (getline(pathsStream, contentPath, ':')) {
      if (IsValidContentDirectory(contentPath.c_str())) {
        return (starboard::strlcat<char>(out_path, contentPath.c_str(),
                                         path_size) < path_size);
      }
    }
    SB_LOG(WARNING) << "GetContentDirectory: COBALT_CONTENT_DIR=" << paths
                    << " don't have cobalt libs";
  }

  if (GetExecutableDirectory(out_path, path_size) &&
      IsValidContentDirectory(out_path)) {
    return true;
  }

  out_path[0] = '\0';
  // Default path as expected by cobalt plugin
  return (starboard::strlcat<char>(out_path, "/usr/share/content/data",
                                   path_size) < path_size);
}

// Gets the path to the cache directory, using the home directory.
bool GetCacheDirectory(char* out_path, int path_size) {
  std::vector<char> home_path(kSbFileMaxPath + 1);
  if (!starboard::shared::starboard::GetHomeDirectory(home_path.data(),
                                                      kSbFileMaxPath)) {
    return false;
  }
  int result = snprintf(out_path, path_size, "%s/.cache", home_path.data());
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }
  struct stat info;
  return mkdir(out_path, 0700) == 0 ||
         (stat(out_path, &info) == 0 && S_ISDIR(info.st_mode));
}

// Gets path to the storage directory, using the home directory.
bool GetStorageDirectory(char* out_path, int path_size) {

  const char* storage_path = std::getenv("COBALT_STORAGE_DIR");
  if (storage_path) {
    if (starboard::strlcat<char>(out_path, storage_path, path_size) < path_size) {
      struct stat info;
      return mkdir(out_path, 0700) == 0 ||
         (stat(out_path, &info) == 0 && S_ISDIR(info.st_mode));
    }
    else
      SB_LOG(ERROR) << "GetStorageDirectory: out_path exceeds max file path size";
  }

  std::vector<char> home_path(kSbFileMaxPath + 1);
  if (!starboard::shared::starboard::GetHomeDirectory(home_path.data(),
                                                      kSbFileMaxPath)) {
    return false;
  }

  int result = snprintf(out_path, path_size, "%s/.cobalt_storage", home_path.data());
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }
  SB_LOG(INFO) << "SbSysGetPath: StorageDirectoy = " << std::string(out_path);
  struct stat info;
  return mkdir(out_path, 0700) == 0 ||
         (stat(out_path, &info) == 0 && S_ISDIR(info.st_mode));
}

// Gets path to the files directory, using the home directory.
bool GetFilesDirectory(char* out_path, int path_size) {

  const char* files_path = std::getenv("COBALT_FILES_DIR");
  if (files_path) {
    if (starboard::strlcat<char>(out_path, files_path, path_size) < path_size) {
      struct stat info;
      return mkdir(out_path, 0700) == 0 ||
         (stat(out_path, &info) == 0 && S_ISDIR(info.st_mode));
    }
    else
      SB_LOG(ERROR) << "GetFilesDirectory: out_path exceeds max file path size";
  }

  std::vector<char> home_path(kSbFileMaxPath + 1);
  if (!starboard::shared::starboard::GetHomeDirectory(home_path.data(),
                                                      kSbFileMaxPath)) {
    return false;
  }

  int result = snprintf(out_path, path_size, "%s/.cobalt_files", home_path.data());
  if (result < 0 || result >= path_size) {
    out_path[0] = '\0';
    return false;
  }
  SB_LOG(INFO) << "SbSysGetPath: FilesDirectoy = " << std::string(out_path);
  struct stat info;
  return mkdir(out_path, 0700) == 0 ||
         (stat(out_path, &info) == 0 && S_ISDIR(info.st_mode));
}

// Gets only the name portion of the current executable.
bool GetExecutableName(char* out_path, int path_size) {
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!GetExecutablePath(path.data(), kSbFileMaxPath)) {
    return false;
  }

  const char* last_slash = strrchr(path.data(), '/');
  if (starboard::strlcpy<char>(out_path, last_slash + 1, path_size) >= path_size) {
    return false;
  }
  return true;
}

// Gets the path to a temporary directory that is unique to this process.
bool GetTemporaryDirectory(char* out_path, int path_size) {
  auto* temp = std::getenv("COBALT_TEMP");
  if (temp) {
    return starboard::strlcpy<char>(out_path, temp, path_size) <= path_size;
  }

  std::vector<char> binary_name(kSbFileMaxPath, 0);
  if (!GetExecutableName(binary_name.data(), kSbFileMaxPath)) {
    return false;
  }

  int result = snprintf(out_path, path_size, "/tmp/%s-%d", binary_name.data(),
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

  const int kPathSize = kSbFileMaxPath;
  char path[kPathSize];
  path[0] = '\0';

  switch (path_id) {
    case kSbSystemPathContentDirectory:
      if (!GetContentDirectory(path, kPathSize)){
        return false;
      }
#if SB_IS(EVERGREEN_COMPATIBLE)
      if (!GetEvergreenContentPathOverride(path, kPathSize)) {
        return false;
      }
#endif
      break;

    case kSbSystemPathCacheDirectory:
      if (!GetCacheDirectory(path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat<char>(path, "/cobalt", kPathSize) >= kPathSize) {
        return false;
      }
      struct stat info;
      if (mkdir(path, 0700) != 0 &&
           !(stat(path, &info) == 0 && S_ISDIR(info.st_mode))) {
        return false;
      }
      break;

    case kSbSystemPathDebugOutputDirectory:
      if (!SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat<char>(path, "/log", kPathSize) >= kPathSize) {
        return false;
      }
      mkdir(path, 0700);
      break;

    case kSbSystemPathTempDirectory:
      if (!GetTemporaryDirectory(path, kPathSize)) {
        return false;
      }
      mkdir(path, 0700);
      break;

    case kSbSystemPathExecutableFile:
      return GetExecutablePath(out_path, path_size);

    case kSbSystemPathFontConfigurationDirectory:
    case kSbSystemPathFontDirectory:
#if SB_IS(EVERGREEN_COMPATIBLE)
      if (!GetContentDirectory(path, kPathSize)) {
        return false;
      }
      if (starboard::strlcat(path, "/fonts", kPathSize) >= kPathSize) {
        return false;
      }
      break;
#else
      return false;
#endif

    case kSbSystemPathStorageDirectory:
      if (!GetStorageDirectory(path, kPathSize)) {
          return false;
      }
      break;

    case kSbSystemPathFilesDirectory: {
      if (!GetFilesDirectory(path, kPathSize)) {
        return false;
      }
      break;
    }

    default:
      SB_NOTIMPLEMENTED() << "SbSystemGetPath not implemented for " << path_id;
      return false;
  }

  int length = strlen(path);
  if (length < 1 || length > path_size) {
    return false;
  }

  starboard::strlcpy<char>(out_path, path, path_size);
  return true;
}
