// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/splash_screen_cache.h"

#include <memory>
#include <string>
#include <vector>

#include "base/hash.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "cobalt/base/get_application_key.h"
#include "starboard/common/string.h"
#include "starboard/directory.h"
#include "starboard/file.h"

namespace cobalt {
namespace browser {
namespace {
bool CreateDirsForKey(const std::string& key) {
  std::vector<char> path(SB_FILE_MAX_PATH, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       SB_FILE_MAX_PATH)) {
    return false;
  }
  std::size_t prev_found = 0;
  std::size_t found = key.find(SB_FILE_SEP_STRING);
  SbStringConcat(path.data(), SB_FILE_SEP_STRING, SB_FILE_MAX_PATH);
  while (found != std::string::npos) {
    SbStringConcat(path.data(),
                   key.substr(prev_found, found - prev_found).c_str(),
                   SB_FILE_MAX_PATH);
    if (!SbDirectoryCreate(path.data())) {
      return false;
    }
    prev_found = found;
    found = key.find(SB_FILE_SEP_STRING, prev_found + 1);
  }
  return true;
}

}  // namespace

SplashScreenCache::SplashScreenCache() : last_page_hash_(0) {
  base::AutoLock lock(lock_);
}

bool SplashScreenCache::CacheSplashScreen(const std::string& key,
                                          const std::string& content) const {
  base::AutoLock lock(lock_);
  if (key.empty()) {
    return false;
  }

  // If an identical page was already read from disk, skip writing
  if (base::Hash(content) == last_page_hash_) {
    return false;
  }

  std::vector<char> path(SB_FILE_MAX_PATH, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       SB_FILE_MAX_PATH)) {
    return false;
  }
  if (!CreateDirsForKey(key)) {
    return false;
  }
  std::string full_path = std::string(path.data()) + SB_FILE_SEP_STRING + key;
  starboard::ScopedFile cache_file(
      full_path.c_str(), kSbFileCreateAlways | kSbFileWrite, NULL, NULL);

  return cache_file.WriteAll(content.c_str(),
                             static_cast<int>(content.size())) > 0;
}

bool SplashScreenCache::IsSplashScreenCached(const std::string& key) const {
  base::AutoLock lock(lock_);
  std::vector<char> path(SB_FILE_MAX_PATH, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       SB_FILE_MAX_PATH)) {
    return false;
  }
  std::string full_path = std::string(path.data()) + SB_FILE_SEP_STRING + key;
  return !key.empty() && SbFileExists(full_path.c_str());
}

int SplashScreenCache::ReadCachedSplashScreen(
    const std::string& key, std::unique_ptr<char[]>* result) const {
  base::AutoLock lock(lock_);
  if (!result) {
    return 0;
  }
  std::vector<char> path(SB_FILE_MAX_PATH, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       SB_FILE_MAX_PATH)) {
    result->reset();
    return 0;
  }
  std::string full_path = std::string(path.data()) + SB_FILE_SEP_STRING + key;
  starboard::ScopedFile cache_file(full_path.c_str(),
                                   kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  SbFileInfo info;
  bool success = SbFileGetPathInfo(full_path.c_str(), &info);
  if (!success) {
    result->reset();
    return 0;
  }
  const int kFileSize = static_cast<int>(info.size);
  result->reset(new char[kFileSize]);
  int result_size = cache_file.ReadAll(result->get(), kFileSize);
  last_page_hash_ = base::Hash(result->get(), result_size);
  return result_size;
}

// static
base::Optional<std::string> SplashScreenCache::GetKeyForStartUrl(
    const GURL& url) {
  base::Optional<std::string> encoded_url = base::GetApplicationKey(url);
  if (!encoded_url) {
    return base::nullopt;
  }

  std::vector<char> path(SB_FILE_MAX_PATH, 0);
  bool has_cache_dir = SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                                       SB_FILE_MAX_PATH);
  if (!has_cache_dir) {
    return base::nullopt;
  }

  std::string subpath = "";
  std::string subcomponent = SB_FILE_SEP_STRING + std::string("splash_screen");
  if (SbStringConcat(path.data(), subcomponent.c_str(), SB_FILE_MAX_PATH) >=
      SB_FILE_MAX_PATH) {
    return base::nullopt;
  }
  subpath += "splash_screen";
  subcomponent = SB_FILE_SEP_STRING + *encoded_url;
  if (SbStringConcat(path.data(), subcomponent.c_str(), SB_FILE_MAX_PATH) >=
      SB_FILE_MAX_PATH) {
    return base::nullopt;
  }
  subpath += subcomponent;
  subcomponent = SB_FILE_SEP_STRING + std::string("splash.html");
  if (SbStringConcat(path.data(), subcomponent.c_str(), SB_FILE_MAX_PATH) >
      SB_FILE_MAX_PATH) {
    return base::nullopt;
  }
  subpath += subcomponent;

  return subpath;
}

}  // namespace browser
}  // namespace cobalt
