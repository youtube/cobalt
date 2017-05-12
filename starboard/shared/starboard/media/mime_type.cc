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

#include "starboard/shared/starboard/media/mime_type.h"

#include "starboard/character.h"
#include "starboard/log.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

typedef std::vector<std::string> Strings;

MimeType::ParamType GetParamTypeByValue(const std::string& value) {
  int count;
  int i;
  if (SbStringScanF(value.c_str(), "%d%n", &i, &count) == 1 &&
      count == value.size()) {
    return MimeType::kParamTypeInteger;
  }
  float f;
  if (SbStringScanF(value.c_str(), "%g%n", &f, &count) == 1 &&
      count == value.size()) {
    return MimeType::kParamTypeFloat;
  }
  return MimeType::kParamTypeString;
}

bool ContainsSpace(const std::string& str) {
  for (size_t i = 0; i < str.size(); ++i) {
    if (SbCharacterIsSpace(str[i])) {
      return true;
    }
  }

  return false;
}

void Trim(std::string* str) {
  while (!str->empty() && SbCharacterIsSpace(*str->begin())) {
    str->erase(str->begin());
  }
  while (!str->empty() && SbCharacterIsSpace(*str->rbegin())) {
    str->resize(str->size() - 1);
  }
}

Strings SplitAndTrim(const std::string& str, char ch) {
  Strings result;
  size_t pos = 0;

  for (;;) {
    size_t next = str.find(ch, pos);
    std::string sub_str = str.substr(pos, next - pos);
    Trim(&sub_str);
    if (!sub_str.empty()) {
      result.push_back(sub_str);
    }
    if (next == str.npos) {
      break;
    }
    pos = next + 1;
  }

  return result;
}

}  // namespace

const int MimeType::kInvalidParamIndex = -1;

MimeType::MimeType(const std::string& content_type) : is_valid_(false) {
  Strings components = SplitAndTrim(content_type, ';');

  if (components.empty()) {
    return;
  }

  // 1. Verify if there is a valid type/subtype in the very beginning.
  if (ContainsSpace(components.front())) {
    return;
  }

  std::vector<std::string> type_and_container =
      SplitAndTrim(components.front(), '/');
  if (type_and_container.size() != 2 || type_and_container[0].empty() ||
      type_and_container[1].empty()) {
    return;
  }
  type_ = type_and_container[0];
  subtype_ = type_and_container[1];
  components.erase(components.begin());

  // 2. Verify the parameters have valid formats, we want to be strict here.
  bool has_codecs = false;
  for (Strings::iterator iter = components.begin(); iter != components.end();
       ++iter) {
    std::vector<std::string> name_and_value = SplitAndTrim(*iter, '=');
    if (name_and_value.size() != 2 || name_and_value[0].empty() ||
        name_and_value[1].empty()) {
      return;
    }
    Param param;
    if (name_and_value[1].size() > 2 && name_and_value[1][0] == '\"' &&
        *name_and_value[1].rbegin() == '\"') {
      param.type = kParamTypeString;
      param.value = name_and_value[1].substr(1, name_and_value[1].size() - 2);
    } else {
      param.type = GetParamTypeByValue(name_and_value[1]);
      param.value = name_and_value[1];
    }
    param.name = name_and_value[0];
    if (param.name == "codecs") {
      // There can only be no more than one codecs parameter and it has to be
      // the first paramter if it is present.
      if (!params_.empty() || has_codecs) {
        return;
      } else {
        has_codecs = true;
      }
    }
    params_.push_back(param);
  }

  is_valid_ = true;
}

const std::vector<std::string>& MimeType::GetCodecs() const {
  if (!codecs_.empty()) {
    return codecs_;
  }
  int codecs_index = GetParamIndexByName("codecs");
  if (codecs_index != 0) {
    return codecs_;
  }
  codecs_ = SplitAndTrim(params_[0].value, ',');
  return codecs_;
}

int MimeType::GetParamCount() const {
  SB_DCHECK(is_valid());

  return static_cast<int>(params_.size());
}

MimeType::ParamType MimeType::GetParamType(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  return params_[index].type;
}

const std::string& MimeType::GetParamName(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  return params_[index].name;
}

int MimeType::GetParamIntValue(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  if (GetParamType(index) != kParamTypeInteger) {
    return 0;
  }

  int i;
  SbStringScanF(params_[index].value.c_str(), "%d", &i);
  return i;
}

float MimeType::GetParamFloatValue(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  if (GetParamType(index) != kParamTypeInteger &&
      GetParamType(index) != kParamTypeFloat) {
    return 0.0f;
  }

  float f;
  SbStringScanF(params_[index].value.c_str(), "%g", &f);

  return f;
}

const std::string& MimeType::GetParamStringValue(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  return params_[index].value;
}

int MimeType::GetParamIntValue(const char* name, int default_value) const {
  int index = GetParamIndexByName(name);
  if (index != kInvalidParamIndex) {
    return GetParamIntValue(index);
  }
  return default_value;
}

float MimeType::GetParamFloatValue(const char* name,
                                   float default_value) const {
  int index = GetParamIndexByName(name);
  if (index != kInvalidParamIndex) {
    return GetParamFloatValue(index);
  }
  return default_value;
}

const std::string& MimeType::GetParamStringValue(
    const char* name,
    const std::string& default_value) const {
  int index = GetParamIndexByName(name);
  if (index != kInvalidParamIndex) {
    return GetParamStringValue(index);
  }
  return default_value;
}

int MimeType::GetParamIndexByName(const char* name) const {
  for (size_t i = 0; i < params_.size(); ++i) {
    if (SbStringCompareNoCase(params_[i].name.c_str(), name) == 0) {
      return static_cast<int>(i);
    }
  }
  return kInvalidParamIndex;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
