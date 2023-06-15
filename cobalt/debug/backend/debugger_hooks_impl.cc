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
#include <utility>

#include "base/callback.h"
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
static_assert(::logging::LOG_NUM_SEVERITIES == arraysize(kConsoleMethodName),
              "Incorrect count of kConsoleMethodName");

}  // namespace

DebuggerHooksImpl::DebuggerHooksImpl()
    : message_loop_(base::MessageLoop::current()) {}

void DebuggerHooksImpl::EnsureScriptDebuggerIfAvailable() {
  if (create_debugger_callback_) {
    LOG(INFO) << "DebuggerHooksImpl::EnsureScriptDebuggerIfAvailable() Calling "
                 "callback";
    std::move(create_debugger_callback_).Run();
    LOG(INFO) << "DebuggerHooksImpl::EnsureScriptDebuggerIfAvailable() Calling "
                 "callback result "
              << (script_debugger_ ? "Yes" : "None");
  }
}

void DebuggerHooksImpl::AttachDebugger(
    script::ScriptDebugger* script_debugger) {
  LOG(INFO) << "DebuggerHooksImpl::AttachDebugger()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  script_debugger_ = script_debugger;
}

void DebuggerHooksImpl::DetachDebugger() {
  LOG(INFO) << "DebuggerHooksImpl::DetachDebugger()";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (script_debugger_) {
    script_debugger_->AllAsyncTasksCanceled();
  }
  script_debugger_ = nullptr;
}

void DebuggerHooksImpl::ConsoleLog(::logging::LogSeverity severity,
                                   std::string message) {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->task_runner()->PostTask(
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
                                           AsyncTaskFrequency frequency) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureScriptDebuggerIfAvailable();
  if (script_debugger_) {
    LOG(INFO) << __FUNCTION__;
    script_debugger_->AsyncTaskScheduled(
        task, name, (frequency == AsyncTaskFrequency::kRecurring));
  } else {
    LOG(INFO) << __FUNCTION__;
  }
}

void DebuggerHooksImpl::AsyncTaskStarted(const void* task) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureScriptDebuggerIfAvailable();
  if (script_debugger_) {
    LOG(INFO) << __FUNCTION__;
    script_debugger_->AsyncTaskStarted(task);
  } else {
    LOG(INFO) << __FUNCTION__;
  }
}

void DebuggerHooksImpl::AsyncTaskFinished(const void* task) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  EnsureScriptDebuggerIfAvailable();
  if (script_debugger_) {
    LOG(INFO) << __FUNCTION__;
    script_debugger_->AsyncTaskFinished(task);
  } else {
    LOG(INFO) << __FUNCTION__;
  }
}

void DebuggerHooksImpl::AsyncTaskCanceled(const void* task) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureScriptDebuggerIfAvailable();
  if (script_debugger_) {
    LOG(INFO) << __FUNCTION__;
    script_debugger_->AsyncTaskCanceled(task);
  } else {
    LOG(INFO) << __FUNCTION__;
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
