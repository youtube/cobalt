// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_V8C_SOURCE_CODE_H_
#define COBALT_SCRIPT_V8C_V8C_SOURCE_CODE_H_

#include <string>

#include "cobalt/base/source_location.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace script {
namespace v8c {

// TODO: Consider pre-compiling scripts here.
// TODO: Also separately consider making this class entirely non-engine
// specific if no engine is going to do anything special with it.
class V8cSourceCode : public SourceCode {
 public:
  V8cSourceCode(const std::string& source_utf8,
                const base::SourceLocation& source_location,
                bool is_muted = false)
      : source_utf8_(source_utf8),
        location_(source_location),
        is_muted_(is_muted) {}

  const std::string& source_utf8() const { return source_utf8_; }
  const base::SourceLocation& location() const { return location_; }
  bool is_muted() const { return is_muted_; }

 private:
  std::string source_utf8_;
  base::SourceLocation location_;
  bool is_muted_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_SOURCE_CODE_H_
