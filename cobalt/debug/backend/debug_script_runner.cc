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

#include "cobalt/debug/backend/debug_script_runner.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
const char kContentDir[] = "debug_backend";
}  // namespace

DebugScriptRunner::DebugScriptRunner(
    script::GlobalEnvironment* global_environment,
    script::ScriptDebugger* script_debugger,
    const dom::CspDelegate* csp_delegate)
    : global_environment_(global_environment),
      script_debugger_(script_debugger),
      csp_delegate_(csp_delegate) {}

bool DebugScriptRunner::RunCommand(const std::string& method,
                                   const std::string& json_params,
                                   std::string* json_result) {
  // If either the domain or the method are undefined in JavaScript, return
  // an empty string to indicate it's unimplemented. Otherwise go ahead and
  // run the method, letting it fail if there's any exception.
  std::string domain(method, 0, method.find('.'));
  std::string script = base::StringPrintf(
      "(typeof debugBackend.%s === 'undefined' ||"
      " typeof debugBackend.%s === 'undefined') ? '' : debugBackend.%s(%s);",
      domain.c_str(), method.c_str(), method.c_str(), json_params.c_str());
  return EvaluateDebuggerScript(script, json_result) && !json_result->empty();
}

bool DebugScriptRunner::RunScriptFile(const std::string& filename) {
  std::string result;

  base::FilePath file_path;
  base::PathService::Get(paths::DIR_COBALT_WEB_ROOT, &file_path);
  file_path = file_path.AppendASCII(kContentDir);
  file_path = file_path.AppendASCII(filename);

  std::string script;
  if (!base::ReadFileToString(file_path, &script)) {
    DLOG(WARNING) << "Cannot read file: " << file_path.value();
    return false;
  }

  if (!EvaluateDebuggerScript(script, nullptr)) {
    DLOG(ERROR) << "Failed to run script file " << filename << ": " << result;
    return false;
  }
  return true;
}

bool DebugScriptRunner::EvaluateDebuggerScript(const std::string& script,
                                               std::string* out_result_utf8) {
  ForceEnableEval();
  bool success =
      script_debugger_->EvaluateDebuggerScript(script, out_result_utf8);
  SetEvalAllowedFromCsp();
  return success;
}

void DebugScriptRunner::ForceEnableEval() {
  global_environment_->EnableEval();
  global_environment_->SetReportEvalCallback(base::Closure());
}

void DebugScriptRunner::SetEvalAllowedFromCsp() {
  std::string eval_disabled_message;
  bool allow_eval = csp_delegate_->AllowEval(&eval_disabled_message);
  if (allow_eval) {
    global_environment_->EnableEval();
  } else {
    global_environment_->DisableEval(eval_disabled_message);
  }

  global_environment_->SetReportEvalCallback(base::Bind(
      &dom::CspDelegate::ReportEval, base::Unretained(csp_delegate_)));
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
