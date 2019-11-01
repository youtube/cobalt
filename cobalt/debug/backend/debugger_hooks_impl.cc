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

#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

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

void DebuggerHooksImpl::AsyncTaskScheduled(const void* task,
                                           const std::string& name,
                                           bool recurring) const {
  if (script_debugger_) {
    script_debugger_->AsyncTaskScheduled(task, name, recurring);
  }
}

void DebuggerHooksImpl::AsyncTaskStarted(const void* task) const {
  if (script_debugger_) {
    script_debugger_->AsyncTaskStarted(task);
  }
}

void DebuggerHooksImpl::AsyncTaskFinished(const void* task) const {
  if (script_debugger_) {
    script_debugger_->AsyncTaskFinished(task);
  }
}

void DebuggerHooksImpl::AsyncTaskCanceled(const void* task) const {
  if (script_debugger_) {
    script_debugger_->AsyncTaskCanceled(task);
  }
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
