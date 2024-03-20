// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/localized_strings.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/system.h"
#include "starboard/types.h"

namespace base {

namespace {

base::Optional<base::FilePath> GetFilenameForLanguage(
    const std::string& language) {
  const int kBufferSize = 256;
  char buffer[kBufferSize];
  bool got_path =
      SbSystemGetPath(kSbSystemPathContentDirectory, buffer, kBufferSize);
  if (!got_path) {
    DLOG(ERROR) << "Cannot get content path for i18n files.";
    return base::nullopt;
  }

  return base::FilePath(buffer).Append("i18n").Append(language).AddExtension(
      "csv");
}

}  // namespace

LocalizedStrings* LocalizedStrings::GetInstance() {
  return base::Singleton<LocalizedStrings>::get();
}

LocalizedStrings::LocalizedStrings() {
  // Initialize to US English on creation. A subsequent call to Initialize
  // will overwrite all available strings in the specified language.
  Initialize("en-US");
}

LocalizedStrings::~LocalizedStrings() {}

void LocalizedStrings::Initialize(const std::string& language) {
  bool did_load_strings = LoadStrings(language);

  if (!did_load_strings) {
    // Failed to load strings - try generic version of the language.
    size_t dash = language.find_first_of("-_");
    if (dash != std::string::npos) {
      std::string generic_lang(language.c_str(), dash);
      did_load_strings = LoadStrings(generic_lang);
    }
  }
}

std::string LocalizedStrings::GetString(const std::string& id,
                                        const std::string& fallback) {
  StringMap::const_iterator iter = strings_.find(id);
  if (iter == strings_.end()) {
    return fallback;
  }
  return iter->second;
}

bool LocalizedStrings::LoadStrings(const std::string& language) {
  auto opt_file_path = GetFilenameForLanguage(language);
  if (!opt_file_path) {
    return false;
  }
  auto file_path = opt_file_path.value();
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    DLOG(ERROR) << "Error reading i18n file.";
    return false;
  }
  DCHECK_GT(file_contents.length(), 0);
  DCHECK_EQ(file_contents[file_contents.length() - 1], '\n');

  // Each line of the file corresponds to one message (key/value).
  size_t pos = 0;
  while (true) {
    size_t next_pos = file_contents.find("\n", pos);
    if (next_pos == std::string::npos) {
      break;
    }
    bool got_string =
        LoadSingleString(std::string(file_contents, pos, next_pos - pos));
    DCHECK(got_string);
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

}  // namespace base
