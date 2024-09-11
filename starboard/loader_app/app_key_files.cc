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

#include "starboard/loader_app/app_key_files.h"

#include <dirent.h>

#include <cstring>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"

namespace starboard {
namespace loader_app {

namespace {
const char kFilePrefix[] = "app_key_";
const char kGoodFileSuffix[] = ".good";
const char kBadFileSuffix[] = ".bad";
}  // namespace

std::string GetAppKeyFilePathWithSuffix(const std::string& dir,
                                        const std::string& app_key,
                                        const std::string& suffix) {
  if (dir.empty() || app_key.empty() || suffix.empty()) {
    SB_LOG(ERROR) << "GetAppKeyFilePathWithSuffix: invalid input";
    return "";
  }
  std::string file_name = dir;
  file_name += kSbFileSepString;
  file_name += kFilePrefix;
  file_name += app_key;
  file_name += suffix;
  return file_name;
}

std::string GetGoodAppKeyFilePath(const std::string& dir,
                                  const std::string& app_key) {
  return GetAppKeyFilePathWithSuffix(dir, app_key, kGoodFileSuffix);
}

std::string GetBadAppKeyFilePath(const std::string& dir,
                                 const std::string& app_key) {
  return GetAppKeyFilePathWithSuffix(dir, app_key, kBadFileSuffix);
}

bool CreateAppKeyFile(const std::string& file_name_path) {
  if (file_name_path.empty()) {
    return false;
  }

  starboard::ScopedFile file(file_name_path.c_str(),
                             O_CREAT | O_TRUNC | O_WRONLY);
  if (!file.IsValid()) {
    SB_LOG(ERROR) << "Failed to open file: " << file_name_path;
    // TODO: retrieve error in starboard::ScopedFile
    // SB_LOG(ERROR) << "Failed to open file: " << file_name_path
    // << "with error: " << file.error_details();
    return false;
  }
  return true;
}

namespace {
bool EndsWith(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) {
    return false;
  }
  return strcmp(s.c_str() + (s.size() - suffix.size()), suffix.c_str()) == 0;
}
}  // namespace

bool AnyGoodAppKeyFile(const std::string& dir) {
  DIR* directory = opendir(dir.c_str());

  if (!directory) {
    SB_LOG(ERROR) << "Failed to open dir='" << dir << "'";
    return false;
  }

  bool found = false;
  std::vector<char> filename(kSbFileMaxName);

  while (true) {
    if (filename.size() < kSbFileMaxName) {
      break;
    }

    if (!directory || !filename.data()) {
      break;
    }

    struct dirent dirent_buffer;
    struct dirent* dirent;
    int result = readdir_r(directory, &dirent_buffer, &dirent);
    if (result || !dirent) {
      break;
    }

    starboard::strlcpy(filename.data(), dirent->d_name, filename.size());

    if (!strncmp(kFilePrefix, filename.data(), sizeof(kFilePrefix) - 1) &&
        EndsWith(filename.data(), kGoodFileSuffix)) {
      found = true;
      break;
    }
  }
  closedir(directory);
  return found;
}

}  // namespace loader_app
}  // namespace starboard
