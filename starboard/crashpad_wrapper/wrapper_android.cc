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

#include "starboard/crashpad_wrapper/wrapper.h"

#include <sys/stat.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "starboard/configuration_constants.h"
#include "starboard/crashpad_wrapper/certificates_cache.h"
#include "starboard/system.h"

namespace crashpad {

namespace {

// Returns the path and modification time of the app's native library
// directory, which Starboard reports as the executable file. Both change when
// the app is installed or updated. Returns an empty string on failure.
std::string GetInstallStamp() {
  std::vector<char> path(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathExecutableFile, path.data(),
                       kSbFileMaxPath)) {
    return "";
  }
  struct stat info;
  if (stat(path.data(), &info) != 0) {
    return "";
  }
  return std::string(path.data()) + "\n" + base::NumberToString(info.st_mtime);
}

}  // namespace

void InstallCrashpadHandler(const std::string& ca_certificates_path) {
  // This process reads the CA certificates in the APK through Starboard's file
  // emulation, but the handler is a separate process and can't, so it gets a
  // copy in the cache directory.
  std::vector<char> cache_directory_path(kSbFileMaxPath);
  std::optional<base::FilePath> handler_ca_certificates_path;
  if (SbSystemGetPath(kSbSystemPathCacheDirectory, cache_directory_path.data(),
                      kSbFileMaxPath)) {
    handler_ca_certificates_path = SyncCACertificatesCache(
        ca_certificates_path, cache_directory_path.data(), GetInstallStamp());
  }
  if (!handler_ca_certificates_path) {
    // TODO(b/571897340): Consider still starting the handler, with uploads
    // disabled, so that it saves a minidump on the device without an unsafe
    // upload. That needs the certificates resolved before the Crashpad
    // database is initialized, and checks that database pruning works without
    // uploads.
    LOG(ERROR) << "Failed to copy the CA certificates, not installing the "
                  "Crashpad handler";
    return;
  }
  InstallCrashpadHandlerImpl(handler_ca_certificates_path->value());
}

}  // namespace crashpad
