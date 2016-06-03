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
#include "cobalt/debug/component_connector.h"
#include "cobalt/debug/debug_script_runner.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {

class RuntimeComponent {
 public:
  explicit RuntimeComponent(ComponentConnector* connector);

 private:
  // Command handlers.
  JSONObject CallFunctionOn(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);
  JSONObject Enable(const JSONObject& params);
  JSONObject Evaluate(const JSONObject& params);
  JSONObject GetProperties(const JSONObject& params);
  JSONObject ReleaseObject(const JSONObject& params);
  JSONObject ReleaseObjectGroup(const JSONObject& params);

  // Sends an event to the client to notify an execution context has been
  // created. Currently we just send this event once when the component
  // is enabled, passing default values.
  void OnExecutionContextCreated();

  // Helper object to connect to the debug server, etc.
  ComponentConnector* connector_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_RUNTIME_COMPONENT_H_
