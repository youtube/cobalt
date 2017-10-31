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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_SOURCE_CODE_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_SOURCE_CODE_H_

#include <string>

#include "cobalt/base/source_location.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace script {
namespace mozjs {

// SpiderMonkey supports compiling scripts in one step and executing
// them in another. Typcically we don't need to execute the same script
// multiple times so we wouldn't expect much performance improvement, but it
// may be that the compiled script uses less memory than the raw script.
//
// TODO: Investigate if there are memory savings (or other benefits)
// to precompiling scripts before executing.
class MozjsSourceCode : public SourceCode {
 public:
  MozjsSourceCode(const std::string& source_utf8,
                  const base::SourceLocation& source_location)
      : source_utf8_(source_utf8), location_(source_location) {}
  const std::string& source_utf8() const { return source_utf8_; }
  const base::SourceLocation& location() const { return location_; }

 private:
  std::string source_utf8_;
  base::SourceLocation location_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_SOURCE_CODE_H_
