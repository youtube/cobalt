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

#ifndef COBALT_DEBUG_BACKEND_DEBUG_BACKEND_H_
#define COBALT_DEBUG_BACKEND_DEBUG_BACKEND_H_

#include <string>

#include "base/callback.h"
#include "cobalt/debug/backend/css_agent.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/script_debugger.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {
namespace backend {

// Provides hooks for the JavaScript part of hybrid JS/C++ agent implementations
// to call C++ debugger backend logic. Also serves as the persistent JavaScript
// object to which the the JavaScript agents attach their methods implementing
// various protocol commands called by |DebugScriptRunner::RunCommand|.
class DebugBackend : public script::Wrappable {
 public:
  // Callback to forward asynchronous protocol events to the frontend.
  // See: https://chromedevtools.github.io/devtools-protocol/
  typedef base::Callback<void(const std::string& method,
                              const std::string& params)>
      OnEventCallback;

  DebugBackend(script::GlobalEnvironment* global_environment,
               script::ScriptDebugger* script_debugger,
               const OnEventCallback& on_event_callback);

  void BindAgents(scoped_refptr<CSSAgent> css_agent) { css_agent_ = css_agent; }

  void UnbindAgents() { css_agent_ = nullptr; }

  // Sends a protocol event to the debugger frontend.
  void SendEvent(const std::string& method, const std::string& params);

  // Returns the RemoteObject JSON representation of the given object for the
  // debugger frontend.
  // https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#type-RemoteObject
  std::string CreateRemoteObject(const script::ValueHandleHolder& object,
                                 const std::string& group);
  // Returns the JavaScript object given the ID of a RemoteObject.
  const script::ValueHandleHolder* LookupRemoteObjectId(
      const std::string& objectId);

  scoped_refptr<CSSAgent> native_css_agent() { return css_agent_; }

  DEFINE_WRAPPABLE_TYPE(DebugBackend);

 private:
  // Engine-specific debugger implementation.
  script::ScriptDebugger* script_debugger_;

  // Callback to send events.
  OnEventCallback on_event_callback_;

  scoped_refptr<CSSAgent> css_agent_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_DEBUG_BACKEND_H_
