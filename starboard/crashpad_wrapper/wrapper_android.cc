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

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace crashpad {

namespace {

bool CopyFile(const std::string& src_path, const std::string& dst_path) {
  // Don't use the 2-arg open() since _FORTIFY_SOURCE rewrites it to __open_2,
  // which bypasses -Wl,--wrap=open.
  const int src = open(src_path.c_str(), O_RDONLY, 0);
  if (src < 0) {
    PLOG(ERROR) << "Couldn't open " << src_path;
    return false;
  }
  const int dst =
      open(dst_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
  if (dst < 0) {
    PLOG(ERROR) << "Couldn't create " << dst_path;
    close(src);
    return false;
  }
  bool ok = true;
  char buffer[4096];
  while (true) {
    const ssize_t bytes_read = read(src, buffer, sizeof(buffer));
    if (bytes_read == 0) {
      break;
    }
    if (bytes_read < 0 || write(dst, buffer, bytes_read) != bytes_read) {
      ok = false;
      break;
    }
  }
  close(src);
  return close(dst) == 0 && ok;
}

// Copies the files in `src_dir_path` into `dst_dir_path`.
bool CopyDirContents(const std::string& src_dir_path,
                     const std::string& dst_dir_path) {
  DIR* src_dir = opendir(src_dir_path.c_str());
  if (!src_dir) {
    PLOG(ERROR) << "Couldn't open " << src_dir_path;
    return false;
  }
  bool ok = true;
  while (struct dirent* entry = readdir(src_dir)) {
    const std::string name(entry->d_name);
    if (name == "." || name == "..") {
      continue;
    }
    if (!CopyFile(src_dir_path + kSbFileSepChar + name,
                  dst_dir_path + kSbFileSepChar + name)) {
      ok = false;
      break;
    }
  }
  closedir(src_dir);
  return ok;
}

// Copies the CA certificates out of the APK into the cache directory, and
// returns the path of the copy. This process reads APK assets through
// Starboard's file emulation, but the handler is a separate process and can't.
std::optional<base::FilePath> CopyCACertificatesToCache(
    const std::string& ca_certificates_path) {
  std::vector<char> cache_directory_path(kSbFileMaxPath);
  if (ca_certificates_path.empty() ||
      !SbSystemGetPath(kSbSystemPathCacheDirectory, cache_directory_path.data(),
                       kSbFileMaxPath)) {
    return std::nullopt;
  }

  std::string copy_path(cache_directory_path.data());
  copy_path.push_back(kSbFileSepChar);
  copy_path.append("certs");
  struct stat info;
  if (mkdir(copy_path.c_str(), 0700) != 0 &&
      !(stat(copy_path.c_str(), &info) == 0 && S_ISDIR(info.st_mode))) {
    return std::nullopt;
  }

  if (!CopyDirContents(ca_certificates_path, copy_path)) {
    return std::nullopt;
  }
  return base::FilePath(copy_path);
}

}  // namespace

void InstallCrashpadHandler(const std::string& ca_certificates_path) {
  const std::optional<base::FilePath> handler_ca_certificates_path =
      CopyCACertificatesToCache(ca_certificates_path);
  if (!handler_ca_certificates_path) {
    // TODO: Consider still starting the handler, with uploads disabled, so
    // that it saves a minidump on the device without an unsafe upload. That
    // needs the certificates resolved before the Crashpad database is
    // initialized, and checks that database pruning works without uploads.
    LOG(ERROR) << "Failed to copy the CA certificates, not installing the "
                  "Crashpad handler";
    return;
  }
  InstallCrashpadHandlerImpl(handler_ca_certificates_path->value());
}

}  // namespace crashpad
