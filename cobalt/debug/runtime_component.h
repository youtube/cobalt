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

#ifndef COBALT_DEBUG_RUNTIME_COMPONENT_H_
#define COBALT_DEBUG_RUNTIME_COMPONENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/debug/runtime_inspector.h"
#include "cobalt/script/global_object_proxy.h"

namespace cobalt {
namespace debug {

class RuntimeComponent : public DebugServer::Component {
 public:
  RuntimeComponent(const base::WeakPtr<DebugServer>& server,
                   script::GlobalObjectProxy* global_object_proxy);

 private:
  // Command handlers.
  JSONObject CallFunctionOn(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);
  JSONObject Enable(const JSONObject& params);
  JSONObject Evaluate(const JSONObject& params);
  JSONObject GetProperties(const JSONObject& params);
  JSONObject ReleaseObject(const JSONObject& params);
  JSONObject ReleaseObjectGroup(const JSONObject& params);

  // Sends a notification to the client that an execution context has been
  // created. Currently we just send this notification once when the component
  // is enabled, passing default values.
  void OnExecutionContextCreated();

  // Calls |command| in |runtime_inspector_| and creates a response object from
  // the result.
  JSONObject RunCommand(const std::string& command, const JSONObject& params);

  scoped_ptr<RuntimeInspector> runtime_inspector_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_RUNTIME_COMPONENT_H_
