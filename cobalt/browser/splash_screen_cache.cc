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

#include "base/base64.h"
#include "base/hash.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "cobalt/base/get_application_key.h"
#include "starboard/common/file.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"

namespace cobalt {
namespace browser {
namespace {
bool CreateDirsForKey(const std::string& key) {
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    return false;
  }
  std::size_t prev_found = 0;
  std::size_t found = key.find(kSbFileSepString);
  starboard::strlcat(path.data(), kSbFileSepString, kSbFileMaxPath);
  while (found != std::string::npos) {
    starboard::strlcat(path.data(),
                       key.substr(prev_found, found - prev_found).c_str(),
                       kSbFileMaxPath);
    if (!SbDirectoryCreate(path.data())) {
      return false;
    }
    prev_found = found;
    found = key.find(kSbFileSepString, prev_found + 1);
  }
  return true;
}

}  // namespace

SplashScreenCache::SplashScreenCache() : last_page_hash_(0) {
  base::AutoLock lock(lock_);
}

bool SplashScreenCache::CacheSplashScreen(
    const std::string& content,
    const base::Optional<std::string>& topic) const {
  base::AutoLock lock(lock_);
  // Cache the content so that it's retrievable for the topic specified in the
  // rel attribute. This topic may or may not match the topic-specified for this
  // particular startup, tracked with "topic_".
  base::Optional<std::string> key = GetKeyForStartConfig(url_, topic);
  if (!key) {
    return false;
  }

  // If an identical page was already read from disk, skip writing
  if (base::Hash(content) == last_page_hash_) {
    return false;
  }

  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    return false;
  }
  if (!CreateDirsForKey(key.value())) {
    return false;
  }
  std::string full_path =
      std::string(path.data()) + kSbFileSepString + key.value();
  starboard::ScopedFile cache_file(
      full_path.c_str(), kSbFileCreateAlways | kSbFileWrite, NULL, NULL);

  return cache_file.WriteAll(content.c_str(),
                             static_cast<int>(content.size())) > 0;
}

bool SplashScreenCache::IsSplashScreenCached() const {
  base::AutoLock lock(lock_);
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    return false;
  }
  base::Optional<std::string> key = GetKeyForStartConfig(url_, topic_);
  if (!key) return false;
  std::string full_path =
      std::string(path.data()) + kSbFileSepString + key.value();
  return SbFileExists(full_path.c_str());
}

int SplashScreenCache::ReadCachedSplashScreen(
    const std::string& key, std::unique_ptr<char[]>* result) const {
  base::AutoLock lock(lock_);
  if (!result) {
    return 0;
  }
  std::vector<char> path(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(),
                       kSbFileMaxPath)) {
    result->reset();
    return 0;
  }
  std::string full_path = std::string(path.data()) + kSbFileSepString + key;
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

base::Optional<std::string> SplashScreenCache::GetKeyForStartConfig(
    const GURL& url, const base::Optional<std::string>& topic) const {
  base::Optional<std::string> encoded_url = base::GetApplicationKey(url);
  if (!encoded_url) {
    return base::nullopt;
  }

  std::vector<char> path(kSbFileMaxPath, 0);
  bool has_cache_dir =
      SbSystemGetPath(kSbSystemPathCacheDirectory, path.data(), kSbFileMaxPath);
  if (!has_cache_dir) {
    return base::nullopt;
  }

  std::string subpath = "";
  if (!AddPathDirectory(std::string("splash_screen"), path, subpath)) {
    return base::nullopt;
  }
  if (!AddPathDirectory(*encoded_url, path, subpath)) {
    return base::nullopt;
  }
  if (topic && !topic.value().empty()) {
    std::string encoded_topic;
    base::Base64Encode(topic.value(), &encoded_topic);
    if (!AddPathDirectory(encoded_topic, path, subpath)) {
      return base::nullopt;
    }
  }
  if (!AddPathDirectory(std::string("splash.html"), path, subpath)) {
    return base::nullopt;
  }

  return subpath.erase(0, 1);  // Remove leading separator
}

bool SplashScreenCache::AddPathDirectory(const std::string& directory,
                                         std::vector<char>& path,
                                         std::string& subpath) const {
  std::string subcomponent = kSbFileSepString + directory;
  if (starboard::strlcat(path.data(), subcomponent.c_str(), kSbFileMaxPath) >=
      static_cast<int>(kSbFileMaxPath)) {
    return false;
  }
  subpath += subcomponent;
  return true;
}

}  // namespace browser
}  // namespace cobalt
