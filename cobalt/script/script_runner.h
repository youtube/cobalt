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

#ifndef COBALT_SCRIPT_SCRIPT_RUNNER_H_
#define COBALT_SCRIPT_SCRIPT_RUNNER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/source_location.h"

namespace cobalt {
namespace script {

class GlobalEnvironment;

// Maintains a handle to a JavaScript global object, and provides an interface
// to execute JavaScript code.
class ScriptRunner {
 public:
  static scoped_ptr<ScriptRunner> CreateScriptRunner(
      const scoped_refptr<GlobalEnvironment>& global_environment);

  // |out_succeeded| is an optional parameter which reports whether the
  //   script executed without errors.
  virtual std::string Execute(const std::string& script_utf8,
                              const base::SourceLocation& script_location,
                              bool mute_errors, bool* out_succeeded) = 0;
  virtual GlobalEnvironment* GetGlobalEnvironment() const { return NULL; }
  virtual ~ScriptRunner() {}
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_RUNNER_H_
