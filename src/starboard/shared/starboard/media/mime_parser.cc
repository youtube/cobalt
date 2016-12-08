// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/media/mime_parser.h"

#include <vector>

#include "starboard/character.h"
#include "starboard/log.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

void ToLower(std::string* string) {
  SB_DCHECK(string);
  for (size_t i = 0; i < string->size(); ++i) {
    (*string)[i] = SbCharacterToLower((*string)[i]);
  }
}

void Trim(std::string* string) {
  SB_DCHECK(string);
  while (!string->empty() && SbCharacterIsSpace(*string->rbegin())) {
    string->resize(string->size() - 1);
  }
  while (!string->empty() && SbCharacterIsSpace(*string->begin())) {
    string->erase(string->begin());
  }
}

bool IsSeparator(char ch, std::string separator) {
  if (separator.empty()) {
    return SbCharacterIsSpace(ch);
  }
  return separator.find(ch) != separator.npos;
}

// For any given string, split it into substrings separated by any character in
// the |separator| string.  If |separator| is empty string, treat any white
// space as separators.
std::vector<std::string> SplitString(std::string string,
                                     std::string separator = "") {
  std::vector<std::string> result;
  while (!string.empty()) {
    Trim(&string);
    if (string.empty()) {
      break;
    }
    bool added = false;
    for (size_t i = 0; i < string.size(); ++i) {
      if (IsSeparator(string[i], separator)) {
        result.push_back(string.substr(0, i));
        Trim(&result.back());
        string = string.substr(i + 1);
        added = true;
        break;
      }
    }
    if (!added) {
      Trim(&string);
      result.push_back(string);
      break;
    }
  }
  return result;
}

}  // namespace

MimeParser::MimeParser(std::string mime_with_params) {
  Trim(&mime_with_params);
  ToLower(&mime_with_params);

  std::vector<std::string> splits = SplitString(mime_with_params, ";");
  if (splits.empty()) {
    return;  // It is invalid, leave |mime_| as empty.
  }
  mime_ = splits[0];
  // Mime is in the form of "video/mp4".
  if (SplitString(mime_, "/").size() != 2) {
    mime_.clear();
    return;
  }
  splits.erase(splits.begin());

  while (!splits.empty()) {
    std::string params = *splits.begin();
    splits.erase(splits.begin());

    // Param is in the form of 'name=value' or 'name="value"'.
    std::vector<std::string> name_and_value = SplitString(params, "=");
    if (name_and_value.size() != 2) {
      mime_.clear();
      return;
    }
    std::string name = name_and_value[0];
    std::string value = name_and_value[1];

    if (value.size() >= 2 && value[0] == '\"' &&
        value[value.size() - 1] == '\"') {
      value.erase(value.begin());
      value.resize(value.size() - 1);
      Trim(&value);
    }

    params_[name] = value;
  }
}

bool MimeParser::HasParam(const std::string& name) const {
  return params_.find(name) != params_.end();
}

std::string MimeParser::GetParam(const std::string& name) const {
  std::map<std::string, std::string>::const_iterator iter = params_.find(name);
  if (iter == params_.end()) {
    return "";
  }
  return iter->second;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
