// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_SOURCE_CODE_H_
#define COBALT_SCRIPT_SOURCE_CODE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"

namespace cobalt {
namespace script {

// Opaque type that encapsulates JavaScript source code that is ready to be
// evaluated by a JavaScriptEngine
class SourceCode : public base::RefCounted<SourceCode> {
 public:
  // Convert the utf8 encoded string to an object that represents script that
  // can be evaluated.
  // Defined in the engine's implementation.
  static scoped_refptr<SourceCode> CreateSourceCode(
      const std::string& script_utf8,
      const base::SourceLocation& script_location);

 protected:
  SourceCode() {}
  virtual ~SourceCode() {}
  friend class base::RefCounted<SourceCode>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SOURCE_CODE_H_
