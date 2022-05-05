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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MIME_TYPE_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MIME_TYPE_H_

#include <string>
#include <vector>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// This class can be used to parse a content type for media in the form of
// "type/subtype; param1=value1; param2="value2".  For example, the content type
// "video/webm; codecs="vp8, vp9"; width=1920; height=1080; framerate=59.96"
// will be parsed into:
//   type: video
//   subtype: webm
//   codecs: vp8, vp9
//   width: 1920
//   height: 1080
//   framerate: 59.96
//
// Note that "codecs" is a special case because:
// 1. It has to be the first parameter when available.
// 2. It may contain multiple values separated by comma.
//
// The following are the restrictions on the components:
// 1. Type/subtype has to be in the very beginning.
// 2. String values may be double quoted.
// 3. Numeric values cannot be double quoted.
class MimeType {
 public:
  enum ParamType {
    kParamTypeInteger,
    kParamTypeFloat,
    kParamTypeString,
    kParamTypeBoolean,
  };

  struct Param {
    ParamType type;
    std::string name;
    std::string string_value;
    union {
      int int_value;
      float float_value;
      bool bool_value;
    };
  };

  static const int kInvalidParamIndex;

  // Expose the function as a helper function to parse a mime attribute.
  static bool ParseParamString(const std::string& param_string, Param* param);

  explicit MimeType(const std::string& content_type);

  bool is_valid() const { return is_valid_; }

  const std::string& type() const { return type_; }
  const std::string& subtype() const { return subtype_; }
  const std::vector<std::string>& GetCodecs() const { return codecs_; }

  int GetParamCount() const;
  ParamType GetParamType(int index) const;
  const std::string& GetParamName(int index) const;
  // GetParamIndexByName() will return |kInvalidParamIndex| if the param name is
  // not found.
  int GetParamIndexByName(const char* name) const;

  int GetParamIntValue(int index) const;
  float GetParamFloatValue(int index) const;
  const std::string& GetParamStringValue(int index) const;
  bool GetParamBoolValue(int index) const;

  int GetParamIntValue(const char* name, int default_value) const;
  float GetParamFloatValue(const char* name, float default_value) const;
  const std::string& GetParamStringValue(
      const char* name,
      const std::string& default_value) const;
  bool GetParamBoolValue(const char* name, bool default_value) const;

  // Validate functions will return true if the param contains a valid value or
  // if param name is not found.
  bool ValidateIntParameter(const char* name) const;
  bool ValidateFloatParameter(const char* name) const;
  // Allows passing a pattern on the format "value_1|...|value_n"
  // where the parameter value must match one of the values in the pattern in
  // order to be considered valid.
  bool ValidateStringParameter(const char* name,
                               const std::string& pattern = "") const;
  bool ValidateBoolParameter(const char* name) const;

  std::string ToString() const;

 private:
  // Use std::vector as the number of components are usually small and we'd like
  // to keep the order of components.
  typedef std::vector<Param> Params;

  bool is_valid_ = false;
  std::string type_;
  std::string subtype_;
  std::vector<std::string> codecs_;
  Params params_;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MIME_TYPE_H_
