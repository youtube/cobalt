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

#include "cobalt/script/script_runner.h"

#include "base/logging.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace script {

namespace {

class ScriptRunnerImpl : public ScriptRunner {
 public:
  explicit ScriptRunnerImpl(
      const scoped_refptr<GlobalEnvironment> global_environment)
      : global_environment_(global_environment) {}

  std::string Execute(const std::string& script_utf8,
                      const base::SourceLocation& script_location) OVERRIDE;
  GlobalEnvironment* GetGlobalEnvironment() const OVERRIDE {
    return global_environment_;
  }

 private:
  scoped_refptr<GlobalEnvironment> global_environment_;
};

std::string ScriptRunnerImpl::Execute(
    const std::string& script_utf8,
    const base::SourceLocation& script_location) {
  scoped_refptr<SourceCode> source_code =
      SourceCode::CreateSourceCode(script_utf8, script_location);
  if (source_code == NULL) {
    NOTREACHED() << "Failed to pre-process JavaScript source.";
    return "";
  }
  std::string result;
  if (!global_environment_->EvaluateScript(source_code, &result)) {
    DLOG(WARNING) << "Failed to execute JavaScript: " << result;
    return "";
  }
  return result;
}

}  // namespace

scoped_ptr<ScriptRunner> ScriptRunner::CreateScriptRunner(
    const scoped_refptr<GlobalEnvironment>& global_environment) {
  return scoped_ptr<ScriptRunner>(new ScriptRunnerImpl(global_environment));
}

}  // namespace script
}  // namespace cobalt
