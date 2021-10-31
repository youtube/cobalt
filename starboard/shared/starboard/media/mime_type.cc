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

MimeType::ParamType GetParamTypeByValue(const std::string& value) {
  int count;
  int i;
  if (SbStringScanF(value.c_str(), "%d%n", &i, &count) == 1 &&
      count == value.size()) {
    return MimeType::kParamTypeInteger;
  }
  float f;
  std::stringstream buffer(value);
  buffer.imbue(std::locale::classic());
  buffer >> f;
  if (!buffer.fail() && buffer.rdbuf()->in_avail() == 0) {
    return MimeType::kParamTypeFloat;
  }

  if (value == "true" || value == "false") {
    return MimeType::kParamTypeBoolean;
  }

  return MimeType::kParamTypeString;
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

const char* ParamTypeToString(MimeType::ParamType param_type) {
  switch (param_type) {
    case MimeType::kParamTypeInteger:
      return "Integer";
    case MimeType::kParamTypeFloat:
      return "Float";
    case MimeType::kParamTypeString:
      return "String";
    case MimeType::kParamTypeBoolean:
      return "Boolean";
    default:
      SB_NOTREACHED();
      return "Unknown";
  }
}

}  // namespace

const int MimeType::kInvalidParamIndex = -1;

MimeType::MimeType(const std::string& content_type)
    : raw_content_type_(content_type), is_valid_(false) {
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
    // The parameter must be on the format 'name=value' and neither |name| nor
    // |value| can be empty. |value| must also not contain '|'.
    if (name_and_value.size() != 2 || name_and_value[0].empty() ||
        name_and_value[1].empty() ||
        name_and_value[1].find('|') != std::string::npos) {
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
      // the first parameter if it is present.
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
  std::stringstream buffer(params_[index].value.c_str());
  buffer.imbue(std::locale::classic());
  buffer >> f;

  return f;
}

const std::string& MimeType::GetParamStringValue(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  return params_[index].value;
}

bool MimeType::GetParamBoolValue(int index) const {
  SB_DCHECK(is_valid());
  SB_DCHECK(index < GetParamCount());

  if (GetParamType(index) != kParamTypeBoolean) {
    return false;
  }

  return params_[index].value == "true";
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

bool MimeType::RegisterBoolParameter(const char* name) {
  return RegisterParameter(name, kParamTypeBoolean);
}

bool MimeType::RegisterStringParameter(const char* name,
                                       const std::string& pattern /* = "" */) {
  if (!RegisterParameter(name, kParamTypeString)) {
    return false;
  }

  int param_index = GetParamIndexByName(name);
  if (param_index == kInvalidParamIndex || pattern.empty()) {
    return true;
  }

  // Compare the parameter value with the provided pattern.
  const std::string& param_value = GetParamStringValue(param_index);
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

  if (matches) {
    return true;
  }

  SB_LOG(INFO) << "Extended Parameter '" << name << "=" << param_value
               << "' does not match the supplied pattern: '" << pattern << "'";
  is_valid_ = false;
  return false;
}

int MimeType::GetParamIndexByName(const char* name) const {
  for (size_t i = 0; i < params_.size(); ++i) {
    if (SbStringCompareNoCase(params_[i].name.c_str(), name) == 0) {
      return static_cast<int>(i);
    }
  }
  return kInvalidParamIndex;
}

bool MimeType::RegisterParameter(const char* name, ParamType param_type) {
  if (!is_valid()) {
    return false;
  }

  int index = GetParamIndexByName(name);
  if (index == kInvalidParamIndex) {
    return true;
  }

  const std::string& param_value = GetParamStringValue(index);
  ParamType parsed_type = GetParamType(index);

  // Check that the parameter can be returned as the requested type.
  // Allowed conversions:
  // Any Type -> String, Int -> Float
  bool convertible =
      param_type == parsed_type || param_type == kParamTypeString ||
      (param_type == kParamTypeFloat && parsed_type == kParamTypeInteger);
  if (!convertible) {
    SB_LOG(INFO) << "Extended Parameter '" << name << "=" << param_value
                 << "' can't be converted to " << ParamTypeToString(param_type);
    is_valid_ = false;
    return false;
  }

  // All validations succeeded.
  return true;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
