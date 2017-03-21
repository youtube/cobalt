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
#include "cobalt/script/mozjs-45/mozjs_source_code.h"

namespace cobalt {
namespace script {
scoped_refptr<SourceCode> SourceCode::CreateSourceCode(
    const std::string& script_utf8,
    const base::SourceLocation& script_location) {
  return new mozjs::MozjsSourceCode(script_utf8, script_location);
}
}  // namespace script
}  // namespace cobalt
