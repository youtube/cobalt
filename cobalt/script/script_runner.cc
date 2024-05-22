// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/script_runner.h"

#include <memory>

#include "base/logging.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/source_code.h"
#if defined(STARBOARD)
#include "starboard/configuration.h"
#endif  // defined(STARBOARD)

namespace cobalt {
namespace script {

namespace {

class ScriptRunnerImpl : public ScriptRunner {
 public:
  explicit ScriptRunnerImpl(
      const scoped_refptr<GlobalEnvironment> global_environment)
      : global_environment_(global_environment) {}

  std::string Execute(const std::string& script_utf8,
                      const base::SourceLocation& script_location,
                      bool mute_errors, bool* out_succeeded) override;
  GlobalEnvironment* GetGlobalEnvironment() const override {
    return global_environment_.get();
  }

 private:
  scoped_refptr<GlobalEnvironment> global_environment_;
};

std::string ScriptRunnerImpl::Execute(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    bool mute_errors, bool* out_succeeded) {
  scoped_refptr<SourceCode> source_code =
      SourceCode::CreateSourceCode(script_utf8, script_location, mute_errors);
  if (out_succeeded) {
    *out_succeeded = false;
  }
  if (!source_code) {
    NOTREACHED() << "Failed to pre-process JavaScript source.";
    return "";
  }
  std::string result;
  if (!global_environment_->EvaluateScript(source_code, &result)) {
    LOG(WARNING) << "Failed to execute JavaScript: " << result;
    return result;
  }
  if (out_succeeded) {
    *out_succeeded = true;
  }
  return result;
}

}  // namespace

std::unique_ptr<ScriptRunner> ScriptRunner::CreateScriptRunner(
    const scoped_refptr<GlobalEnvironment>& global_environment) {
  return std::unique_ptr<ScriptRunner>(
      new ScriptRunnerImpl(global_environment));
}

}  // namespace script
}  // namespace cobalt
