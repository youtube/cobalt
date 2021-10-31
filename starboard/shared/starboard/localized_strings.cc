// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/localized_strings.h"

#include <algorithm>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/system.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {

namespace {

std::string GetFilenameForLanguage(const std::string& language) {
  const int kBufferSize = 256;
  char buffer[kBufferSize];
  bool got_path =
      SbSystemGetPath(kSbSystemPathContentDirectory, buffer, kBufferSize);
  if (!got_path) {
    SB_DLOG(ERROR) << "Cannot get content path for i18n files.";
    return std::string();
  }

  return std::string(buffer).append("/i18n/").append(language).append(".csv");
}

bool ReadFile(const std::string& filename, std::string* out_result) {
  SB_DCHECK(filename.length() > 0);
  SB_DCHECK(out_result);

  ScopedFile file(filename.c_str(), kSbFileOpenOnly | kSbFileRead);
  if (!file.IsValid()) {
    SB_DLOG(WARNING) << "Cannot open i18n file: " << filename;
    return false;
  }

  SbFileInfo file_info = {0};
  bool got_info = file.GetInfo(&file_info);
  if (!got_info) {
    SB_DLOG(ERROR) << "Cannot get information for i18n file.";
    return false;
  }
  SB_DCHECK(file_info.size > 0);

  const int kMaxBufferSize = 16 * 1024;
  if (file_info.size > kMaxBufferSize) {
    SB_DLOG(ERROR) << "i18n file exceeds maximum size: " << file_info.size
                   << " (" << kMaxBufferSize << ")";
    return false;
  }

  char* buffer = new char[file_info.size];
  SB_DCHECK(buffer);
  int64_t bytes_to_read = file_info.size;
  char* buffer_pos = buffer;
  while (bytes_to_read > 0) {
    int max_bytes_to_read = static_cast<int>(
        std::min(static_cast<int64_t>(kSbInt32Max), bytes_to_read));
    int bytes_read = file.Read(buffer_pos, max_bytes_to_read);
    if (bytes_read < 0) {
      SB_DLOG(ERROR) << "Read from i18n file failed.";
      delete[] buffer;
      return false;
    }
    bytes_to_read -= bytes_read;
    buffer_pos += bytes_read;
  }

  *out_result = std::string(buffer, file_info.size);
  delete[] buffer;
  return true;
}

}  // namespace

LocalizedStrings::LocalizedStrings(const std::string& language) {
  match_type_ = MatchType::kNoMatch;
  bool did_load_strings = LoadStrings(language);

  if (did_load_strings) {
    match_type_ = MatchType::kPrimaryMatch;
  } else {
    // Failed to load strings - try generic version of the language.
    size_t dash = language.find_first_of("-_");
    if (dash != std::string::npos) {
      std::string generic_lang(language.c_str(), dash);
      did_load_strings = LoadStrings(generic_lang);
      if (did_load_strings) {
        match_type_ = MatchType::kSecondaryMatch;
      }
    }
  }

  SB_DCHECK(did_load_strings);
}

std::string LocalizedStrings::GetString(const std::string& id,
                                        const std::string& fallback) const {
  StringMap::const_iterator iter = strings_.find(id);
  if (iter == strings_.end()) {
    return fallback;
  }
  return iter->second;
}

LocalizedStrings::MatchType LocalizedStrings::GetMatchType() const {
  return match_type_;
}

bool LocalizedStrings::LoadStrings(const std::string& language) {
  const std::string filename = GetFilenameForLanguage(language);
  std::string file_contents;
  if (!ReadFile(filename, &file_contents)) {
    SB_DLOG(ERROR) << "Error reading i18n file.";
    return false;
  }
  SB_DCHECK(file_contents.length() > 0);
  SB_DCHECK(file_contents[file_contents.length() - 1] == '\n');

  // Each line of the file corresponds to one message (key/value).
  size_t pos = 0;
  while (true) {
    size_t next_pos = file_contents.find("\n", pos);
    if (next_pos == std::string::npos) {
      break;
    }
    bool got_string =
        LoadSingleString(std::string(file_contents, pos, next_pos - pos));
    SB_DCHECK(got_string);
    pos = next_pos + 1;
  }

  return true;
}

bool LocalizedStrings::LoadSingleString(const std::string& message) {
  // A single message is a key/value pair with separator.
  size_t separator_pos = message.find(';');
  if (separator_pos == std::string::npos) {
    SB_DLOG(ERROR) << "No separator found in: " << message;
    return false;
  }

  const std::string key(message, 0, separator_pos);
  const std::string value(message, separator_pos + 1);
  strings_[key] = value;
  return true;
}

}  // namespace starboard
}  // namespace shared
}  // namespace starboard
