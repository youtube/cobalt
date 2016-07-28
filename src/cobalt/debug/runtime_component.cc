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
// File to load JavaScript runtime implementation from.
const char kScriptFile[] = "runtime.js";

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

// Constant parameter values:
const char kContextFrameIdValue[] = "Cobalt";
const int kContextIdValue = 1;
const char kContextNameValue[] = "Cobalt";
}  // namespace

RuntimeComponent::RuntimeComponent(ComponentConnector* connector)
    : connector_(connector) {
  DCHECK(connector_);
  if (!connector_->RunScriptFile(kScriptFile)) {
    DLOG(WARNING) << "Cannot execute Runtime initialization script.";
  }

  connector_->AddCommand(
      kCallFunctionOn,
      base::Bind(&RuntimeComponent::CallFunctionOn, base::Unretained(this)));
  connector_->AddCommand(
      kDisable, base::Bind(&RuntimeComponent::Disable, base::Unretained(this)));
  connector_->AddCommand(
      kEnable, base::Bind(&RuntimeComponent::Enable, base::Unretained(this)));
  connector_->AddCommand(kEvaluate, base::Bind(&RuntimeComponent::Evaluate,
                                               base::Unretained(this)));
  connector_->AddCommand(
      kGetProperties,
      base::Bind(&RuntimeComponent::GetProperties, base::Unretained(this)));
  connector_->AddCommand(
      kReleaseObject,
      base::Bind(&RuntimeComponent::ReleaseObject, base::Unretained(this)));
  connector_->AddCommand(kReleaseObjectGroup,
                         base::Bind(&RuntimeComponent::ReleaseObjectGroup,
                                    base::Unretained(this)));
}

JSONObject RuntimeComponent::CallFunctionOn(const JSONObject& params) {
  return connector_->RunScriptCommand("runtime.callFunctionOn", params);
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
  return connector_->RunScriptCommand("runtime.evaluate", params);
}

JSONObject RuntimeComponent::GetProperties(const JSONObject& params) {
  return connector_->RunScriptCommand("runtime.getProperties", params);
}

JSONObject RuntimeComponent::ReleaseObjectGroup(const JSONObject& params) {
  return connector_->RunScriptCommand("runtime.releaseObjectGroup", params);
}

JSONObject RuntimeComponent::ReleaseObject(const JSONObject& params) {
  return connector_->RunScriptCommand("runtime.releaseObject", params);
}

void RuntimeComponent::OnExecutionContextCreated() {
  JSONObject event(new base::DictionaryValue());
  event->SetString(kContextFrameId, kContextFrameIdValue);
  event->SetInteger(kContextId, kContextIdValue);
  event->SetString(kContextName, kContextNameValue);
  connector_->SendEvent(kExecutionContextCreated, event);
}

}  // namespace debug
}  // namespace cobalt
