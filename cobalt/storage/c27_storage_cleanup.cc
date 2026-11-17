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

#include "cobalt/storage/c27_storage_cleanup.h"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/timer/elapsed_timer.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace cobalt {
namespace storage {

const char kC27FilesDirectoryName[] = ".cobalt_files";
const char kC27MigrationMarkerFileName[] = "migration_completed.txt";
const char kC27LegacyRecordDigestFileName[] = "legacy_record_sha256.txt";

namespace {

// starboard::kCobaltDefaultUrl in Cobalt 27.
const char kC27DefaultUrl[] = "https://www.youtube.com/tv";

// Keys are web-safe base64. Anything else must not become a path component.
bool IsValidAppKey(const std::string& app_key) {
  const char kAllowed[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";
  return !app_key.empty() &&
         app_key.find_first_not_of(kAllowed) == std::string::npos;
}

// Removes the file or link at |path| (never a directory, never a link's
// target) and adds a regular file's size to |bytes|. Returns false if
// something is there that could not be removed.
bool DeleteFileOrLink(const std::string& path, int64_t* bytes) {
  struct stat info;
  const bool is_file = stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
  if (unlink(path.c_str()) != 0) {
    return errno == ENOENT || errno == ENOTDIR;
  }
  if (is_file) {
    *bytes += info.st_size;
  }
  return true;
}

// Deletes the file, link or directory tree at |path|. Links are never
// followed: rmdir() and unlink() act on the link itself, and a directory is
// only entered after rmdir() has reported it to be a real, non-empty one.
// Returns false if something could not be deleted.
//
// Only calls that the Evergreen loader exports from Starboard 16 on are used;
// lstat() is not one of them. Before Starboard 16 the loader does not bridge
// errno, so this would have to be rewritten with SbDirectory* and
// SbFileGetPathInfo() there.
bool DeleteTree(const std::string& path, int64_t* bytes) {
  if (rmdir(path.c_str()) == 0 || errno == ENOENT) {
    return true;
  }
  if (errno == ENOTDIR) {
    return DeleteFileOrLink(path, bytes);
  }
  if (errno != ENOTEMPTY && errno != EEXIST) {
    return false;
  }

  DIR* directory = opendir(path.c_str());
  if (!directory) {
    return false;
  }
  std::vector<std::string> names;
  struct dirent buffer;
  struct dirent* entry = nullptr;
  while (readdir_r(directory, &buffer, &entry) == 0 && entry) {
    const std::string name(entry->d_name);
    if (name != "." && name != "..") {
      names.push_back(name);
    }
  }
  closedir(directory);

  bool success = true;
  for (const std::string& name : names) {
    success &= DeleteTree(path + kSbFileSepString + name, bytes);
  }
  return rmdir(path.c_str()) == 0 && success;
}

void LogIfFailed(bool success, const base::FilePath& path) {
  if (!success) {
    LOG(WARNING) << "C27 storage cleanup: could not delete " << path.value();
  }
}

base::FilePath GetStarboardPath(SbSystemPathId id) {
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(id, path.data(), static_cast<int>(path.size()))) {
    return base::FilePath();
  }
  return base::FilePath(path.data());
}

}  // namespace

std::string GetC27AppKey(const std::string& url) {
  std::string key = url.empty() ? kC27DefaultUrl : url;

  // "<scheme>://<user info>@..." without the user info.
  const size_t scheme_end = key.find("://");
  const size_t at = key.find('@');
  if (scheme_end != std::string::npos && at != std::string::npos &&
      scheme_end < at) {
    key.erase(scheme_end + 3, at - (scheme_end + 2));
  }
  const size_t query = key.find('?');
  if (query != std::string::npos) {
    key.erase(query);
  }
  const size_t fragment = key.find('#');
  if (fragment != std::string::npos) {
    key.erase(fragment);
  }

  std::string encoded;
  base::Base64Encode(key, &encoded);
  std::replace(encoded.begin(), encoded.end(), '+', '-');
  std::replace(encoded.begin(), encoded.end(), '/', '_');

  const size_t max_size = static_cast<size_t>(kSbFileMaxName) - 24;
  if (encoded.size() > max_size) {
    encoded = encoded.substr(encoded.size() - max_size);
  }
  return encoded;
}

int64_t DeleteOrphanedC27Storage(const base::FilePath& storage_dir,
                                 const base::FilePath& cache_dir,
                                 const std::string& c27_app_key) {
  if (!IsValidAppKey(c27_app_key)) {
    return -1;
  }

  int64_t bytes = 0;
  if (storage_dir.IsAbsolute()) {
    const base::FilePath files_dir =
        storage_dir.DirName().Append(kC27FilesDirectoryName);
    const base::FilePath profile = files_dir.Append(c27_app_key);
    LogIfFailed(DeleteTree(profile.value(), &bytes), profile);
    // Fails, as intended, if other apps' profiles are still in there.
    rmdir(files_dir.value().c_str());
  }
  if (cache_dir.IsAbsolute()) {
    const base::FilePath c27_cache = cache_dir.Append(c27_app_key);
    LogIfFailed(DeleteTree(c27_cache.value(), &bytes), c27_cache);
    for (const char* name :
         {kC27MigrationMarkerFileName, kC27LegacyRecordDigestFileName}) {
      const base::FilePath marker = cache_dir.DirName().Append(name);
      LogIfFailed(DeleteFileOrLink(marker.value(), &bytes), marker);
    }
  }
  return bytes;
}

void DeleteOrphanedC27StorageAtStartup(const std::string& url) {
  base::ElapsedTimer timer;
  const int64_t bytes = DeleteOrphanedC27Storage(
      GetStarboardPath(kSbSystemPathStorageDirectory),
      GetStarboardPath(kSbSystemPathCacheDirectory), GetC27AppKey(url));
  if (bytes < 0) {
    LOG(WARNING) << "C27 storage cleanup: skipped, unusable app key.";
  } else if (bytes > 0) {
    LOG(INFO) << "C27 storage cleanup: deleted " << bytes << " bytes in "
              << timer.Elapsed().InMilliseconds() << " ms.";
  }
}

}  // namespace storage
}  // namespace cobalt
