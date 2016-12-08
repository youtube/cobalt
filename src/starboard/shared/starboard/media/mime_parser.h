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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MIME_PARSER_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MIME_PARSER_H_

#include <map>
#include <string>

#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// This class can be used to parse media mime types with params.  For example,
// the following mime type 'video/mp4; codecs="avc1.4d401e"; width=640' will be
// parsed into:
//   mime => video/mp4
//     param: codecs => avc1.4d401e
//     param: width  => 640
class MimeParser {
 public:
  explicit MimeParser(std::string mime_with_params);

  bool is_valid() const { return !mime_.empty(); }
  const std::string& mime() const { return mime_; }

  bool HasParam(const std::string& name) const;
  std::string GetParam(const std::string& name) const;

 private:
  std::string mime_;
  std::map<std::string, std::string> params_;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MIME_PARSER_H_
