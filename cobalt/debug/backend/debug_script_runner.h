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

#ifndef COBALT_DEBUG_BACKEND_DEBUG_SCRIPT_RUNNER_H_
#define COBALT_DEBUG_BACKEND_DEBUG_SCRIPT_RUNNER_H_

#include <string>

#include "cobalt/dom/csp_delegate.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

// Runs JavaScript code that is part of the backend implementation.
class DebugScriptRunner {
 public:
  DebugScriptRunner(script::GlobalEnvironment* global_environment,
                    script::ScriptDebugger* script_debugger,
                    const dom::CspDelegate* csp_delegate);

  // Runs |method| on the JavaScript |debugBackend| object, passing in
  // |json_params|. If |json_result| is non-NULL it receives the result.
  // Returns |true| if the method was executed; |json_result| is the value
  // returned by the method.
  // Returns |false| if the method wasn't executed; if the method isn't defined
  // |json_result| is empty, otherwise it's an error message.
  bool RunCommand(const std::string& method, const std::string& json_params,
                  std::string* json_result);

  // Loads JavaScript from file and executes the contents. Used to add
  // the JavaScript part of hybrid JS/C++ agent implementations.
  bool RunScriptFile(const std::string& filename);

 private:
  bool EvaluateDebuggerScript(const std::string& script,
                              std::string* out_result_utf8);

  // Ensures the JS eval command is enabled, overriding CSP if necessary.
  void ForceEnableEval();
  // Enables/disables eval according to CSP.
  void SetEvalAllowedFromCsp();

  // No ownership.
  script::GlobalEnvironment* global_environment_;

  // Engine-specific debugger implementation.
  script::ScriptDebugger* script_debugger_;

  // Non-owned reference to let this object query whether CSP allows eval.
  const dom::CspDelegate* csp_delegate_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_DEBUG_SCRIPT_RUNNER_H_
