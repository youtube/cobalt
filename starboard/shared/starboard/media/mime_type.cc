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

#include "starboard/shared/starboard/media/mime_type.h"

#include <algorithm>
#include <iosfwd>
#include <locale>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

typedef std::vector<std::string> Strings;

void ParseParamTypeAndValue(const std::string& name,
                            const std::string& value,
                            MimeType::Param* param) {
  SB_DCHECK(param);

  param->name = name;
  if (value.size() >= 2 && value[0] == '\"' && value.back() == '\"') {
    param->type = MimeType::kParamTypeString;
    param->string_value = value.substr(1, value.size() - 2);
    return;
  }

  param->string_value = value;

  int count;
  int i;
  if (SbStringScanF(value.c_str(), "%d%n", &i, &count) == 1 &&
      count == value.size()) {
    param->type = MimeType::kParamTypeInteger;
    param->int_value = i;
    return;
  }
  float f;
  std::stringstream buffer(value);
  buffer.imbue(std::locale::classic());
  buffer >> f;
  if (!buffer.fail() && buffer.rdbuf()->in_avail() == 0) {
    param->type = MimeType::kParamTypeFloat;
    param->float_value = f;
    return;
  }
  if (value == "true" || value == "false") {
    param->type = MimeType::kParamTypeBoolean;
    param->bool_value = value == "true";
    return;
  }

  param->type = MimeType::kParamTypeString;
}

bool ContainsSpace(const std::string& str) {
  for (size_t i = 0; i < str.size(); ++i) {
    if (isspace(str[i])) {
      return true;
    }
  }
  return false;
}

void Trim(std::string* str) {
  while (!str->empty() && isspace(*str->begin())) {
    str->erase(str->begin());
  }
  while (!str->empty() && isspace(*str->rbegin())) {
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

// static
bool MimeType::ParseParamString(const std::string& param_string, Param* param) {
  std::vector<std::string> name_and_value = SplitAndTrim(param_string, '=');
  // The parameter must be on the format 'name=value' and neither |name| nor
  // |value| can be empty. |value| must also not contain '|' and ';'.
  if (name_and_value.size() != 2 || name_and_value[0].empty() ||
      name_and_value[1].empty() ||
      name_and_value[1].find('|') != std::string::npos ||
      name_and_value[1].find(';') != std::string::npos) {
    return false;
  }

  if (param) {
    ParseParamTypeAndValue(name_and_value[0], name_and_value[1], param);
  }
  return true;
}

MimeType::MimeType(const std::string& content_type) {
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
  for (Strings::iterator iter = components.begin(); iter != components.end();
       ++iter) {
    Param param;
    if (!ParseParamString(*iter, &param)) {
      return;
    }
    // There can only be no more than one codecs parameter and it has to be
    // the first parameter if it is present.
    if (param.name == "codecs") {
      if (!params_.empty()) {
        return;
      }
      codecs_ = SplitAndTrim(param.string_value, ',');
    }
    params_.push_back(param);
  }

  is_valid_ = true;
}

int MimeType::GetParamCount() const {
  return static_cast<int>(params_.size());
}

MimeType::ParamType MimeType::GetParamType(int index) const {
  SB_DCHECK(index < GetParamCount());

  return params_[index].type;
}

const std::string& MimeType::GetParamName(int index) const {
  SB_DCHECK(index < GetParamCount());

  return params_[index].name;
}

int MimeType::GetParamIndexByName(const char* name) const {
  for (size_t i = 0; i < params_.size(); ++i) {
    if (strcasecmp(params_[i].name.c_str(), name) == 0) {
      return static_cast<int>(i);
    }
  }
  return kInvalidParamIndex;
}

int MimeType::GetParamIntValue(int index) const {
  SB_DCHECK(index < GetParamCount());

  if (params_[index].type == kParamTypeInteger) {
    return params_[index].int_value;
  }
  return 0;
}

float MimeType::GetParamFloatValue(int index) const {
  SB_DCHECK(index < GetParamCount());

  if (params_[index].type == kParamTypeInteger) {
    return params_[index].int_value;
  }
  if (params_[index].type == kParamTypeFloat) {
    return params_[index].float_value;
  }
  return 0.0f;
}

const std::string& MimeType::GetParamStringValue(int index) const {
  SB_DCHECK(index < GetParamCount());

  return params_[index].string_value;
}

bool MimeType::GetParamBoolValue(int index) const {
  SB_DCHECK(index < GetParamCount());

  if (params_[index].type == kParamTypeBoolean) {
    return params_[index].bool_value;
  }
  return false;
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

bool MimeType::GetParamBoolValue(const char* name, bool default_value) const {
  int index = GetParamIndexByName(name);
  if (index != kInvalidParamIndex) {
    return GetParamBoolValue(index);
  }
  return default_value;
}

bool MimeType::ValidateIntParameter(const char* name) const {
  if (!is_valid()) {
    return false;
  }

  int index = GetParamIndexByName(name);
  if (index == kInvalidParamIndex) {
    return true;
  }
  return GetParamType(index) == kParamTypeInteger;
}

bool MimeType::ValidateFloatParameter(const char* name) const {
  if (!is_valid()) {
    return false;
  }

  int index = GetParamIndexByName(name);
  if (index == kInvalidParamIndex) {
    return true;
  }
  ParamType type = GetParamType(index);
  return type == kParamTypeInteger || type == kParamTypeFloat;
}

bool MimeType::ValidateStringParameter(const char* name,
                                       const std::string& pattern) const {
  if (!is_valid()) {
    return false;
  }

  int index = GetParamIndexByName(name);
  if (pattern.empty() || index == kInvalidParamIndex) {
    return true;
  }

  const std::string& param_value = params_[index].string_value;

  bool matches = false;
  size_t match_start = 0;
  while (!matches) {
    match_start = pattern.find(param_value, match_start);
    if (match_start == std::string::npos) {
      break;
    }

    size_t match_end = match_start + param_value.length();
    matches =
        // Matches if the match is at the start of the string or
        // if the preceding character is the divider _and_
        (match_start <= 0 || pattern[match_start - 1] == '|') &&
        // if the end of the match is the end of the pattern or
        // if the succeeding character is the divider.
        (match_end >= pattern.length() || pattern[match_end] == '|');
    match_start = match_end + 1;
  }
  return matches;
}

bool MimeType::ValidateBoolParameter(const char* name) const {
  if (!is_valid()) {
    return false;
  }

  int index = GetParamIndexByName(name);
  if (index == kInvalidParamIndex) {
    return true;
  }
  ParamType type = GetParamType(index);
  return type == kParamTypeBoolean;
}

std::string MimeType::ToString() const {
  if (!is_valid()) {
    return "{ InvalidMimeType }; ";
  }
  std::stringstream ss;
  ss << "{ type: " << type();
  ss << ", subtype: " << subtype();
  ss << ", codecs: ";
  if (codecs_.empty()) {
    ss << "null";
  } else {
    ss << codecs_[0];
    for (size_t i = 1; i < codecs_.size(); i++) {
      ss << "|" << codecs_[i];
    }
  }
  ss << ", params: ";
  if (params_.empty()) {
    ss << "null";
  } else {
    ss << "{ ";
    for (size_t i = 0; i < params_.size(); i++) {
      const Param& param = params_[i];
      if (i != 0) {
        ss << ",";
      }
      ss << param.name << "=";
      switch (param.type) {
        case kParamTypeInteger:
          ss << "(int)" << param.int_value;
          break;
        case kParamTypeFloat:
          ss << "(float)" << param.float_value;
          break;
        case kParamTypeString:
          ss << "(string)" << param.string_value;
          break;
        case kParamTypeBoolean:
          ss << "(bool)" << (param.bool_value ? "true" : "false");
          break;
      }
    }
    ss << " }";
  }
  ss << " }";
  return ss.str();
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
