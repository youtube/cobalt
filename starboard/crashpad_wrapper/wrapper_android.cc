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

#include <algorithm>
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

bool ReadFile(const std::string& path, std::string* contents) {
  const int fd = open(path.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    return false;
  }
  contents->clear();
  char buffer[4096];
  ssize_t bytes_read;
  while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
    contents->append(buffer, bytes_read);
  }
  close(fd);
  return bytes_read == 0;
}

bool WriteFile(const std::string& path, const std::string& contents) {
  const int fd =
      open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return false;
  }
  const bool ok = write(fd, contents.data(), contents.size()) ==
                  static_cast<ssize_t>(contents.size());
  return close(fd) == 0 && ok;
}

// Returns the sorted names of the files in `dir_path`.
std::optional<std::vector<std::string>> ListDir(const std::string& dir_path) {
  DIR* dir = opendir(dir_path.c_str());
  if (!dir) {
    PLOG(ERROR) << "Couldn't open " << dir_path;
    return std::nullopt;
  }
  std::vector<std::string> names;
  while (struct dirent* entry = readdir(dir)) {
    const std::string name(entry->d_name);
    if (name != "." && name != "..") {
      names.push_back(name);
    }
  }
  closedir(dir);
  std::sort(names.begin(), names.end());
  return names;
}

// Replaces the files in `dst_dir_path` with the ones in `src_dir_path`.
bool CopyDirContents(const std::string& src_dir_path,
                     const std::vector<std::string>& names,
                     const std::string& dst_dir_path) {
  const std::optional<std::vector<std::string>> old_names =
      ListDir(dst_dir_path);
  if (!old_names) {
    return false;
  }
  for (const std::string& name : *old_names) {
    if (unlink((dst_dir_path + kSbFileSepChar + name).c_str()) != 0) {
      return false;
    }
  }
  for (const std::string& name : names) {
    if (!CopyFile(src_dir_path + kSbFileSepChar + name,
                  dst_dir_path + kSbFileSepChar + name)) {
      return false;
    }
  }
  return true;
}

// Returns a stamp of the CA certificates in `ca_certificates_path`: the path,
// the manifest.json of the Evergreen package they ship in, and the file names.
// The certificates are at content/ssl/certs in the package, and change only
// with it. Returns an empty string when there's no manifest.json.
std::string GetCACertificatesStamp(const std::string& ca_certificates_path,
                                   const std::vector<std::string>& names) {
  const base::FilePath manifest_path = base::FilePath(ca_certificates_path)
                                           .DirName()
                                           .DirName()
                                           .DirName()
                                           .Append("manifest.json");
  std::string manifest;
  if (!ReadFile(manifest_path.value(), &manifest)) {
    return "";
  }
  std::string stamp = ca_certificates_path + "\n" + manifest;
  for (const std::string& name : names) {
    stamp += "\n" + name;
  }
  return stamp;
}

// Copies the CA certificates out of the APK into the cache directory, and
// returns the path of the copy. This process reads APK assets through
// Starboard's file emulation, but the handler is a separate process and can't.
// The copy is kept until the certificates change, since copying them takes
// over 100 ms.
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

  const std::optional<std::vector<std::string>> names =
      ListDir(ca_certificates_path);
  if (!names) {
    return std::nullopt;
  }

  // Reuse the copy if it's of the same certificates and no file is missing,
  // since Android can delete cache files.
  const std::string stamp_path = copy_path + ".stamp";
  const std::string stamp =
      GetCACertificatesStamp(ca_certificates_path, *names);
  std::string old_stamp;
  if (!stamp.empty() && ReadFile(stamp_path, &old_stamp) &&
      old_stamp == stamp && ListDir(copy_path) == names) {
    return base::FilePath(copy_path);
  }

  unlink(stamp_path.c_str());
  if (!CopyDirContents(ca_certificates_path, *names, copy_path)) {
    return std::nullopt;
  }
  if (!stamp.empty() && !WriteFile(stamp_path, stamp)) {
    LOG(WARNING) << "Couldn't write " << stamp_path;
  }
  return base::FilePath(copy_path);
}

}  // namespace

void InstallCrashpadHandler(const std::string& ca_certificates_path) {
  const std::optional<base::FilePath> handler_ca_certificates_path =
      CopyCACertificatesToCache(ca_certificates_path);
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
