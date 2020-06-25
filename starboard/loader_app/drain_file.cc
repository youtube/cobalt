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

#include "starboard/loader_app/drain_file.h"

#include <algorithm>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/string.h"

#ifdef __cplusplus
extern "C" {
#endif

const SbTime kDrainFileMaximumAge = kSbTimeHour;
const char kDrainFilePrefix[] = "drain_";

#ifdef __cplusplus
}  // extern "C"
#endif

namespace starboard {
namespace loader_app {
namespace {

std::string ExtractAppKey(const std::string& str) {
  const size_t begin = str.find_first_of('_') + 1;
  const size_t end = str.find_last_of('_');

  if ((begin == std::string::npos) || (end == std::string::npos) ||
      (end - begin < 1))
    return "";

  return str.substr(begin, end - begin);
}

SbTime ExtractTimestamp(const std::string& str) {
  const size_t index = str.find_last_of('_') + 1;

  if ((index == std::string::npos) || (index == str.size() - 1))
    return 0;

  const std::string timestamp = str.substr(index, str.size() - index);

  return SbStringAToL(timestamp.c_str());
}

bool IsExpired(const std::string& filename) {
  const SbTime timestamp = ExtractTimestamp(filename);
  return timestamp + kDrainFileMaximumAge < SbTimeGetNow();
}

std::vector<std::string> FindAllWithPrefix(const std::string& dir,
                                           const std::string& prefix) {
  if (prefix.empty())
    return std::vector<std::string>();

  SbDirectory slot = SbDirectoryOpen(dir.c_str(), NULL);

  if (!SbDirectoryIsValid(slot)) {
    SB_LOG(ERROR) << "Failed to open provided directory '" << dir << "'";
    return std::vector<std::string>();
  }

  std::vector<char> filename(kSbFileMaxName);
  std::vector<std::string> filenames;

  while (SbDirectoryGetNext(slot, filename.data(), filename.size())) {
    if (!SbStringCompare(prefix.data(), filename.data(), prefix.size()))
      filenames.push_back(std::string(filename.data()));
  }

  SbDirectoryClose(slot);
  return filenames;
}

void Rank(const char* dir, char* app_key, size_t len) {
  SB_DCHECK(dir);
  SB_DCHECK(app_key);

  std::vector<std::string> filenames = FindAllWithPrefix(dir, kDrainFilePrefix);

  std::remove_if(filenames.begin(), filenames.end(), IsExpired);

  if (filenames.empty())
    return;

  // This lambda compares two strings, each string being a drain file name. This
  // function returns |true| when |left| has an earlier timestamp than |right|,
  // or when |left| is ASCII-betically lower than |right| if their timestamps
  // are equal.
  auto compare_filenames = [](const std::string& left,
                              const std::string& right) -> bool {
    const SbTime left_timestamp = ExtractTimestamp(left);
    const SbTime right_timestamp = ExtractTimestamp(right);

    if (left_timestamp != right_timestamp)
      return left_timestamp < right_timestamp;

    const std::string left_app_key = ExtractAppKey(left);
    const std::string right_app_key = ExtractAppKey(right);

    return SbStringCompare(left_app_key.c_str(), right_app_key.c_str(),
                           right_app_key.size()) < 0;
  };

  std::sort(filenames.begin(), filenames.end(), compare_filenames);

  const std::string& ranking_app_key = ExtractAppKey(filenames.front());

  if (SbStringCopy(app_key, ranking_app_key.c_str(), len) >= len)
    SB_LOG(ERROR) << "Returned value was truncated";
}

}  // namespace

namespace drain_file {

bool TryDrain(const char* dir, const char* app_key) {
  SB_DCHECK(dir);
  SB_DCHECK(app_key);

  std::vector<std::string> filenames = FindAllWithPrefix(dir, kDrainFilePrefix);

  for (const auto& filename : filenames) {
    if (IsExpired(filename))
      continue;
    if (filename.find(app_key) == std::string::npos)
      return false;
    SB_LOG(INFO) << "Found valid drain file '" << filename << "'";
    return true;
  }

  std::string path(dir);
  path.append(kSbFileSepString);
  path.append(kDrainFilePrefix);
  path.append(app_key);
  path.append("_");
  path.append(std::to_string(SbTimeGetNow()));

  SbFileError error = kSbFileOk;
  SbFile file = SbFileOpen(path.c_str(), kSbFileCreateAlways, NULL, &error);

  SB_DCHECK(error == kSbFileOk);
  SB_DCHECK(SbFileClose(file));

  SB_LOG(INFO) << "Created drain file at '" << path << "'";

  return true;
}

bool RankAndCheck(const char* dir, const char* app_key) {
  SB_DCHECK(dir);
  SB_DCHECK(app_key);

  std::vector<char> ranking_app_key(kSbFileMaxName);

  Rank(dir, ranking_app_key.data(), ranking_app_key.size());

  return !SbStringCompareAll(ranking_app_key.data(), app_key);
}

void Clear(const char* dir, const char* app_key, bool expired) {
  SB_DCHECK(dir);

  std::vector<std::string> filenames = FindAllWithPrefix(dir, kDrainFilePrefix);

  for (const auto& filename : filenames) {
    if (expired && !IsExpired(filename))
      continue;
    if (app_key && (filename.find(app_key) != std::string::npos))
      continue;

    const std::string path = dir + std::string(kSbFileSepString) + filename;

    if (!SbFileDelete(path.c_str()))
      SB_LOG(ERROR) << "Failed to remove expired drain file at '" << filename
                    << "'";
  }
}

bool Draining(const char* dir, const char* app_key) {
  SB_DCHECK(dir);

  std::string prefix(kDrainFilePrefix);

  if (app_key)
    prefix.append(app_key);

  const std::vector<std::string> filenames =
      FindAllWithPrefix(dir, prefix.c_str());

  for (const auto& filename : filenames) {
    if (!IsExpired(filename))
      return true;
  }
  return false;
}

}  // namespace drain_file
}  // namespace loader_app
}  // namespace starboard

#ifdef __cplusplus
extern "C" {
#endif

bool DrainFileTryDrain(const char* dir, const char* app_key) {
  return starboard::loader_app::drain_file::TryDrain(dir, app_key);
}

bool DrainFileRankAndCheck(const char* dir, const char* app_key) {
  return starboard::loader_app::drain_file::RankAndCheck(dir, app_key);
}

void DrainFileClear(const char* dir, const char* app_key, bool expired) {
  starboard::loader_app::drain_file::Clear(dir, app_key, expired);
}

bool DrainFileDraining(const char* dir, const char* app_key) {
  return starboard::loader_app::drain_file::Draining(dir, app_key);
}

#ifdef __cplusplus
}  // extern "C"
#endif
