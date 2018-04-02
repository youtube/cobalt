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

#include "net/dial/dial_system_config.h"

#include "starboard/file.h"
#include "starboard/system.h"

namespace {
const char kInAppDialUuidFilename[] = "upnp_udn";
const size_t kUuidSizeBytes = 16;
}

namespace net {

// static
std::string DialSystemConfig::GetFriendlyName() {
  char buffer[kMaxNameSize];
  if (SbSystemGetProperty(kSbSystemPropertyFriendlyName, buffer,
                          sizeof(buffer))) {
    return std::string(buffer);
  } else {
    return std::string();
  }
}

// static
std::string DialSystemConfig::GetManufacturerName() {
  char buffer[kMaxNameSize];
  if (SbSystemGetProperty(kSbSystemPropertyManufacturerName, buffer,
                          sizeof(buffer))) {
    return std::string(buffer);
  } else {
    return std::string();
  }
}

// static
std::string DialSystemConfig::GetModelName() {
  char buffer[kMaxNameSize];
  if (SbSystemGetProperty(kSbSystemPropertyModelName, buffer, sizeof(buffer))) {
    return std::string(buffer);
  } else {
    return std::string();
  }
}

namespace {
std::string GenerateRandomUuid() {
  char uuid_buffer[kUuidSizeBytes];
  SbSystemGetRandomData(uuid_buffer, kUuidSizeBytes);
  return std::string(uuid_buffer, kUuidSizeBytes);
}
}  // namespace

// static
std::string DialSystemConfig::GeneratePlatformUuid() {
  char path_buffer[SB_FILE_MAX_PATH];
  bool success = SbSystemGetPath(kSbSystemPathCacheDirectory,
                                 path_buffer,
                                 sizeof(path_buffer));

  DCHECK(success) << "kSbSystemPathCacheDirectory not implemented";

  std::string path(path_buffer);
  path.append(SB_FILE_SEP_STRING);
  path.append(kInAppDialUuidFilename);

  bool created;
  SbFileError error;
  starboard::ScopedFile file(path.c_str(),
                             kSbFileOpenAlways | kSbFileRead | kSbFileWrite,
                             &created,
                             &error);
  if (error != kSbFileOk) {
    LOG(ERROR) << "Unable to open or create " << path;
    return GenerateRandomUuid();
  }

  char uuid_buffer[kUuidSizeBytes];
  int bytes_read = file.ReadAll(uuid_buffer, kUuidSizeBytes);

  if (bytes_read == kUuidSizeBytes) {
    return std::string(uuid_buffer, bytes_read);
  }

  file.Truncate(0);

  std::string uuid = GenerateRandomUuid();

  int bytes_written = file.WriteAll(uuid.data(), uuid.size());

  if (bytes_written != uuid.size()) {
    LOG(ERROR) << "Unable to store device UUID to " << path;
  }

  return uuid;
}

}  // namespace net
