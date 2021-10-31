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

#include "cobalt/debug/backend/debug_backend.h"

#include "base/memory/ptr_util.h"

namespace cobalt {
namespace debug {
namespace backend {

DebugBackend::DebugBackend(script::GlobalEnvironment* global_environment,
                           script::ScriptDebugger* script_debugger,
                           const OnEventCallback& on_event_callback)
    : script_debugger_(script_debugger), on_event_callback_(on_event_callback) {
  // Bind this object to the global object so it can persist state and be
  // accessed from any of the debug agents.
  global_environment->Bind("debugBackend", scoped_refptr<DebugBackend>(this));
}

void DebugBackend::SendEvent(const std::string& method,
                             const std::string& params) {
  on_event_callback_.Run(method, params);
}

std::string DebugBackend::CreateRemoteObject(
    const script::ValueHandleHolder& object, const std::string& group) {
  return script_debugger_->CreateRemoteObject(object, group);
}

const script::ValueHandleHolder* DebugBackend::LookupRemoteObjectId(
    const std::string& objectId) {
  return script_debugger_->LookupRemoteObjectId(objectId);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
