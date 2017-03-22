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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MIME_TYPE_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MIME_TYPE_H_

#include <string>
#include <vector>

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
  };

  static const int kInvalidParamIndex;

  explicit MimeType(const std::string& content_type);

  bool is_valid() const { return is_valid_; }

  const std::string& type() const { return type_; }
  const std::string& subtype() const { return subtype_; }

  const std::vector<std::string>& GetCodecs() const;

  int GetParamCount() const;
  ParamType GetParamType(int index) const;
  const std::string& GetParamName(int index) const;

  int GetParamIntValue(int index) const;
  float GetParamFloatValue(int index) const;
  const std::string& GetParamStringValue(int index) const;

  int GetParamIntValue(const char* name, int default_value) const;
  float GetParamFloatValue(const char* name, float default_value) const;
  const std::string& GetParamStringValue(
      const char* name,
      const std::string& default_value) const;

 private:
  struct Param {
    ParamType type;
    std::string name;
    std::string value;
  };

  // Use std::vector as the number of components are usually small and we'd like
  // to keep the order of components.
  typedef std::vector<Param> Params;

  int GetParamIndexByName(const char* name) const;

  bool is_valid_;
  std::string type_;
  std::string subtype_;
  Params params_;
  mutable std::vector<std::string> codecs_;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MIME_TYPE_H_
