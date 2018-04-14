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

#include "cobalt/debug/debug_script_runner.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace debug {

namespace {
const char kContentDir[] = "cobalt/debug/backend";
const char kObjectIdentifier[] = "devtoolsBackend";
}  // namespace

DebugScriptRunner::DebugScriptRunner(
    script::GlobalEnvironment* global_environment,
    const dom::CspDelegate* csp_delegate,
    const OnEventCallback& on_event_callback)
    : global_environment_(global_environment),
      csp_delegate_(csp_delegate),
      on_event_callback_(on_event_callback) {
  // Bind this object to the global object so it can persist state and be
  // accessed from any of the debug components.
  global_environment_->Bind(kObjectIdentifier, make_scoped_refptr(this));
}

base::optional<std::string> DebugScriptRunner::CreateRemoteObject(
    const script::ValueHandleHolder* object, const std::string& params) {
  // Callback function should have been set by runtime.js.
  DCHECK(create_remote_object_callback_);

  CreateRemoteObjectCallback::ReturnValue result =
      create_remote_object_callback_->value().Run(object, params);
  if (result.exception) {
    DLOG(WARNING) << "Exception creating remote object.";
    return base::nullopt;
  } else {
    return result.result;
  }
}

bool DebugScriptRunner::RunCommand(const std::string& command,
                                   const std::string& json_params,
                                   std::string* json_result) {
  std::string script = base::StringPrintf("%s.%s(%s);", kObjectIdentifier,
                                          command.c_str(), json_params.c_str());
  return EvaluateScript(script, json_result);
}

bool DebugScriptRunner::RunScriptFile(const std::string& filename) {
  std::string result;
  bool success = EvaluateScriptFile(filename, &result);
  if (!success) {
    DLOG(WARNING) << "Failed to run script file " << filename << ": " << result;
  }
  return success;
}

bool DebugScriptRunner::EvaluateScript(const std::string& js_code,
                                       std::string* result) {
  DCHECK(global_environment_);
  DCHECK(result);
  scoped_refptr<script::SourceCode> source_code =
      script::SourceCode::CreateSourceCode(js_code, GetInlineSourceLocation());

  ForceEnableEval();
  bool succeeded = global_environment_->EvaluateScript(source_code, result);
  SetEvalAllowedFromCsp();
  return succeeded;
}

bool DebugScriptRunner::EvaluateScriptFile(const std::string& filename,
                                           std::string* result) {
  DCHECK(result);

  FilePath file_path;
  PathService::Get(paths::DIR_COBALT_WEB_ROOT, &file_path);
  file_path = file_path.AppendASCII(kContentDir);
  file_path = file_path.AppendASCII(filename);

  std::string script;
  if (!file_util::ReadFileToString(file_path, &script)) {
    DLOG(WARNING) << "Cannot read file: " << file_path.value();
    return false;
  }

  return EvaluateScript(script, result);
}

void DebugScriptRunner::SendEvent(const std::string& method,
                                  const base::optional<std::string>& params) {
  on_event_callback_.Run(method, params);
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

const DebugScriptRunner::CreateRemoteObjectCallbackHolder*
DebugScriptRunner::create_remote_object_callback() {
  if (create_remote_object_callback_) {
    return &(create_remote_object_callback_->referenced_value());
  } else {
    return NULL;
  }
}

void DebugScriptRunner::set_create_remote_object_callback(
    const CreateRemoteObjectCallbackHolder& callback) {
  create_remote_object_callback_.emplace(this, callback);
}

}  // namespace debug
}  // namespace cobalt
