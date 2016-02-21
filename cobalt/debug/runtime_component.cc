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

#include "cobalt/debug/runtime_component.h"

#include "base/bind.h"

namespace cobalt {
namespace debug {

namespace {
// Definitions from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime

// Command "methods" (names):
const char kCallFunctionOn[] = "Runtime.callFunctionOn";
const char kDisable[] = "Runtime.disable";
const char kEnable[] = "Runtime.enable";
const char kEvaluate[] = "Runtime.evaluate";
const char kGetProperties[] = "Runtime.getProperties";
const char kReleaseObject[] = "Runtime.releaseObject";
const char kReleaseObjectGroup[] = "Runtime.releaseObjectGroup";

// Event "methods" (names):
const char kExecutionContextCreated[] = "Runtime.executionContextCreated";

// Parameter names:
const char kContextFrameId[] = "context.frameId";
const char kContextId[] = "context.id";
const char kContextName[] = "context.name";
const char kErrorMessage[] = "error.message";
const char kResult[] = "result";

// Constant parameter values:
const char kContextFrameIdValue[] = "Cobalt";
const int kContextIdValue = 1;
const char kContextNameValue[] = "Cobalt";
}  // namespace

RuntimeComponent::RuntimeComponent(
    const base::WeakPtr<DebugServer>& server,
    script::GlobalObjectProxy* global_object_proxy)
    : DebugServer::Component(server),
      runtime_inspector_(new RuntimeInspector(global_object_proxy)) {
  AddCommand(kCallFunctionOn, base::Bind(&RuntimeComponent::CallFunctionOn,
                                         base::Unretained(this)));
  AddCommand(kDisable,
             base::Bind(&RuntimeComponent::Disable, base::Unretained(this)));
  AddCommand(kEnable,
             base::Bind(&RuntimeComponent::Enable, base::Unretained(this)));
  AddCommand(kEvaluate,
             base::Bind(&RuntimeComponent::Evaluate, base::Unretained(this)));
  AddCommand(kGetProperties, base::Bind(&RuntimeComponent::GetProperties,
                                        base::Unretained(this)));
  AddCommand(kReleaseObject, base::Bind(&RuntimeComponent::ReleaseObject,
                                        base::Unretained(this)));
  AddCommand(kReleaseObjectGroup,
             base::Bind(&RuntimeComponent::ReleaseObjectGroup,
                        base::Unretained(this)));
}

JSONObject RuntimeComponent::CallFunctionOn(const JSONObject& params) {
  return RunCommand("callFunctionOn", params);
}

JSONObject RuntimeComponent::Disable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  return JSONObject(new base::DictionaryValue());
}

JSONObject RuntimeComponent::Enable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  OnExecutionContextCreated();
  return JSONObject(new base::DictionaryValue());
}

JSONObject RuntimeComponent::Evaluate(const JSONObject& params) {
  return RunCommand("evaluate", params);
}

JSONObject RuntimeComponent::GetProperties(const JSONObject& params) {
  return RunCommand("getProperties", params);
}

JSONObject RuntimeComponent::ReleaseObjectGroup(const JSONObject& params) {
  return RunCommand("releaseObjectGroup", params);
}

JSONObject RuntimeComponent::ReleaseObject(const JSONObject& params) {
  return RunCommand("releaseObject", params);
}

void RuntimeComponent::OnExecutionContextCreated() {
  JSONObject notification(new base::DictionaryValue());
  notification->SetString(kContextFrameId, kContextFrameIdValue);
  notification->SetInteger(kContextId, kContextIdValue);
  notification->SetString(kContextName, kContextNameValue);
  SendNotification(kExecutionContextCreated, notification);
}

JSONObject RuntimeComponent::RunCommand(const std::string& command,
                                        const JSONObject& params) {
  std::string json_params = JSONStringify(params);
  std::string json_result;
  bool success =
      runtime_inspector_->RunCommand(command, json_params, &json_result);

  JSONObject response(new base::DictionaryValue());
  if (success) {
    JSONObject result = JSONParse(json_result);
    if (result) {
      response->Set(kResult, result.release());
    }
  } else {
    response->SetString(kErrorMessage, json_result);
  }
  return response.Pass();
}

}  // namespace debug
}  // namespace cobalt
