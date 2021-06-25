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

#include <cstring>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/string.h"

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
  SbFileError file_error = kSbFileOk;
  starboard::ScopedFile file(file_name_path.c_str(),
                             kSbFileCreateAlways | kSbFileWrite, NULL,
                             &file_error);
  if (!file.IsValid()) {
    SB_LOG(ERROR) << "Failed to open file: " << file_name_path
                  << "with error: " << file_error;
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
  SbDirectory directory = SbDirectoryOpen(dir.c_str(), NULL);

  if (!SbDirectoryIsValid(directory)) {
    SB_LOG(ERROR) << "Failed to open dir='" << dir << "'";
    return false;
  }

  bool found = false;
#if SB_API_VERSION >= 12
  std::vector<char> filename(kSbFileMaxName);
  while (SbDirectoryGetNext(directory, filename.data(), filename.size())) {
    if (!strncmp(kFilePrefix, filename.data(), sizeof(kFilePrefix) - 1) &&
        EndsWith(filename.data(), kGoodFileSuffix)) {
      found = true;
      break;
    }
  }
#else
  SbDirectoryEntry entry;
  while (SbDirectoryGetNext(directory, &entry)) {
    if (!strncmp(kFilePrefix, entry.name, sizeof(kFilePrefix) - 1) &&
        EndsWith(entry.name, kGoodFileSuffix)) {
      found = true;
      break;
    }
  }
#endif

  SbDirectoryClose(directory);
  return found;
}

}  // namespace loader_app
}  // namespace starboard
