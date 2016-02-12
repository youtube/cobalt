/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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
#ifndef COBALT_DEBUG_JAVASCRIPT_DEBUGGER_COMPONENT_H_
#define COBALT_DEBUG_JAVASCRIPT_DEBUGGER_COMPONENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/javascript_debugger_interface.h"

namespace cobalt {
namespace debug {

class JavaScriptDebuggerComponent : public DebugServer::Component {
 public:
  JavaScriptDebuggerComponent(const base::WeakPtr<DebugServer>& server,
                              script::GlobalObjectProxy* global_object_proxy);

 private:
  // Called by |javascript_debugger_| to provide notifications to this
  // component.
  void OnNotification(const std::string& method, const JSONObject& params);

  JSONObject Enable(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);

  // Gets the source of a specified script.
  JSONObject GetScriptSource(const JSONObject& params);

  // No ownership.
  script::GlobalObjectProxy* global_object_proxy_;

  // Handles all debugging interaction with the JavaScript engine.
  scoped_ptr<script::JavaScriptDebuggerInterface> javascript_debugger_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_JAVASCRIPT_DEBUGGER_COMPONENT_H_
