// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/debugger_hooks_impl.h"

#include <sstream>

#include "base/json/string_escape.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {

// Indexed by ::logging::LogSeverity
const char* kConsoleMethodName[] = {
    "info",   // LOG_INFO
    "warn",   // LOG_WARNING
    "error",  // LOG_ERROR
    "error",  // LOG_FATAL - there is no console.fatal() function.
};
static_assert(::logging::LOGGING_NUM_SEVERITIES ==
                  arraysize(kConsoleMethodName),
              "Incorrect count of kConsoleMethodName");

}  // namespace

DebuggerHooksImpl::DebuggerHooksImpl()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

void DebuggerHooksImpl::AttachDebugger(
    script::ScriptDebugger* script_debugger) {
  script_debugger_ = script_debugger;
}

void DebuggerHooksImpl::DetachDebugger() {
  if (script_debugger_) {
    script_debugger_->AllAsyncTasksCanceled();
  }
  script_debugger_ = nullptr;
}

void DebuggerHooksImpl::ConsoleLog(::logging::LogSeverity severity,
                                   std::string message) const {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&DebuggerHooksImpl::ConsoleLog,
                              base::Unretained(this), severity, message));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!script_debugger_) return;
  std::ostringstream js_code;
  js_code << "console." << kConsoleMethodName[severity] << '('
          << base::GetQuotedJSONString(message) << ')';
  script_debugger_->EvaluateDebuggerScript(js_code.str(), nullptr);
}

void DebuggerHooksImpl::AsyncTaskScheduled(const void* task,
                                           const std::string& name,
                                           AsyncTaskFrequency frequency) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (script_debugger_) {
    script_debugger_->AsyncTaskScheduled(
        task, name, (frequency == AsyncTaskFrequency::kRecurring));
  }
}

void DebuggerHooksImpl::AsyncTaskStarted(const void* task) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (script_debugger_) {
    script_debugger_->AsyncTaskStarted(task);
  }
}

void DebuggerHooksImpl::AsyncTaskFinished(const void* task) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (script_debugger_) {
    script_debugger_->AsyncTaskFinished(task);
  }
}

void DebuggerHooksImpl::AsyncTaskCanceled(const void* task) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (script_debugger_) {
    script_debugger_->AsyncTaskCanceled(task);
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
