// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/webdriver/script_executor.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace webdriver {
namespace {
// Path to the script to initialize the script execution harness.
const char kWebDriverInitScriptPath[] = "webdriver/webdriver-init.js";

// Wrapper around a scoped_refptr<script::SourceCode> instance. The script
// at kWebDriverInitScriptPath will be loaded from disk and a new
// script::SourceCode will be created.
class LazySourceLoader {
 public:
  LazySourceLoader() {
    FilePath exe_path;
    if (!PathService::Get(base::DIR_EXE, &exe_path)) {
      NOTREACHED() << "Failed to get EXE path.";
      return;
    }
    FilePath script_path = exe_path.Append(kWebDriverInitScriptPath);
    std::string script_contents;
    if (!file_util::ReadFileToString(script_path, &script_contents)) {
      NOTREACHED() << "Failed to read script contents.";
      return;
    }
    source_code_ = script::SourceCode::CreateSourceCode(
        script_contents.c_str(),
        base::SourceLocation(kWebDriverInitScriptPath, 1, 1));
  }
  const scoped_refptr<script::SourceCode>& source_code() {
    return source_code_;
  }

 private:
  scoped_refptr<script::SourceCode> source_code_;
};

// The script only needs to be loaded once, so allow it to persist as a
// LazyInstance and be shared amongst different WindowDriver instances.
base::LazyInstance<LazySourceLoader> lazy_source_loader =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

void ScriptExecutor::LoadExecutorSourceCode() { lazy_source_loader.Get(); }

scoped_refptr<ScriptExecutor> ScriptExecutor::Create(
    ElementMapping* element_mapping,
    const scoped_refptr<script::GlobalEnvironment>& global_environment) {
  // This could be NULL if there was an error loading the harness source from
  // disk.
  scoped_refptr<script::SourceCode> source =
      lazy_source_loader.Get().source_code();
  if (!source) {
    return NULL;
  }

  // Create a new ScriptExecutor and bind it to the global object.
  scoped_refptr<ScriptExecutor> script_executor =
      new ScriptExecutor(element_mapping);
  global_environment->Bind("webdriverExecutor", script_executor);

  // Evaluate the harness initialization script.
  std::string result;
  if (!global_environment->EvaluateScript(source, &result)) {
    return NULL;
  }

  // The initialization script should have set this.
  DCHECK(script_executor->execute_script_harness());
  return script_executor;
}

bool ScriptExecutor::Execute(
    const scoped_refptr<ScriptExecutorParams>& params,
    ScriptExecutorResult::ResultHandler* result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<ScriptExecutorResult> executor_result(
      new ScriptExecutorResult(result_handler));
  return ExecuteInternal(params, executor_result);
}

void ScriptExecutor::set_execute_script_harness(
    const ExecuteFunctionCallbackHolder& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  execute_callback_.emplace(this, callback);
}

const ScriptExecutor::ExecuteFunctionCallbackHolder*
ScriptExecutor::execute_script_harness() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (execute_callback_) {
    return &(execute_callback_->referenced_value());
  } else {
    return NULL;
  }
}

scoped_refptr<dom::Element> ScriptExecutor::IdToElement(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(element_mapping_);
  return element_mapping_->IdToElement(protocol::ElementId(id));
}

std::string ScriptExecutor::ElementToId(
    const scoped_refptr<dom::Element>& element) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(element_mapping_);
  return element_mapping_->ElementToId(element).id();
}

bool ScriptExecutor::ExecuteInternal(
    const scoped_refptr<ScriptExecutorParams>& params,
    const scoped_refptr<ScriptExecutorResult>& result_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(params);
  DCHECK(result_handler);
  ExecuteFunctionCallback::ReturnValue callback_result =
      execute_callback_->value().Run(params, result_handler);
  return !callback_result.exception;
}

}  // namespace webdriver
}  // namespace cobalt
