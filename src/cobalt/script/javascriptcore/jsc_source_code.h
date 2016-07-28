/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SOURCE_CODE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SOURCE_CODE_H_

#include <string>

#include "cobalt/script/source_code.h"

#include "config.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceCode.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCSourceCode : public SourceCode {
 public:
  explicit JSCSourceCode(const std::string& source_utf8,
                         const base::SourceLocation& source_location);
  JSC::SourceCode& source() { return source_; }

 private:
  JSC::SourceCode source_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SOURCE_CODE_H_
