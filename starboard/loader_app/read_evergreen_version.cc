// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/read_evergreen_version.h"

#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace loader_app {
namespace {

// Evergreen version key in the manifest file.
const char kVersionKey[] = "version";

}  // namespace

const int kMaxEgVersionLength = 20;

bool ReadEvergreenVersion(const std::vector<char>& manifest_file_path,
                          char* version,
                          int version_length) {
  // Check the manifest file exists
  struct stat info;
  if (stat(manifest_file_path.data(), &info) != 0) {
    SB_LOG(WARNING)
        << "Failed to open the manifest file at the installation path.";
    return false;
  }

  starboard::ScopedFile manifest_file(manifest_file_path.data(), O_RDONLY,
                                      S_IRWXU | S_IRGRP);
  int64_t file_size = manifest_file.GetSize();
  std::vector<char> file_data(file_size);
  int read_size = manifest_file.ReadAll(file_data.data(), file_size);
  if (read_size < 0) {
    SB_LOG(WARNING) << "Error while reading from the manifest file.";
    return false;
  }

  Json::Reader reader;
  Json::Value obj;
  if (!reader.parse(file_data.data(), file_data.data() + read_size, obj) ||
      !obj.isMember(kVersionKey)) {
    SB_LOG(WARNING) << "Failed to parse version from the manifest file at the "
                       "installation path.";
    return false;
  }

  // TODO: b/485307186 - Add a check (and unit test) for truncation.
  snprintf(version, version_length, "%s", obj[kVersionKey].asString().c_str());
  return true;
}

}  // namespace loader_app
