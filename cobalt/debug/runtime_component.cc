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
#include "cobalt/base/source_location.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace debug {

namespace {
// Definitions from the set specified here:
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime

// Command "methods" (names):
const char kDisable[] = "Runtime.disable";
const char kEnable[] = "Runtime.enable";
const char kEvaluate[] = "Runtime.evaluate";
const char kReleaseObjectGroup[] = "Runtime.releaseObjectGroup";

// Event "methods" (names):
const char kExecutionContextCreated[] = "Runtime.executionContextCreated";

// Parameter names:
const char kContextFrameId[] = "context.frameId";
const char kContextId[] = "context.id";
const char kContextName[] = "context.name";
const char kExpression[] = "expression";
const char kGeneratePreview[] = "generatePreview";
const char kResultType[] = "result.result.type";
const char kResultValue[] = "result.result.value";
const char kWasThrown[] = "result.wasThrown";

// Constant parameter values:
const char kContextFrameIdValue[] = "Cobalt";
const int kContextIdValue = 1;
const char kContextNameValue[] = "Cobalt";
}  // namespace

RuntimeComponent::RuntimeComponent(
    const base::WeakPtr<DebugServer>& server,
    script::GlobalObjectProxy* global_object_proxy)
    : DebugServer::Component(server),
      global_object_proxy_(global_object_proxy) {
  AddCommand(kDisable,
             base::Bind(&RuntimeComponent::Disable, base::Unretained(this)));
  AddCommand(kEnable,
             base::Bind(&RuntimeComponent::Enable, base::Unretained(this)));
  AddCommand(kEvaluate,
             base::Bind(&RuntimeComponent::Evaluate, base::Unretained(this)));
  AddCommand(kReleaseObjectGroup,
             base::Bind(&RuntimeComponent::ReleaseObjectGroup,
                        base::Unretained(this)));
}

JSONObject RuntimeComponent::Disable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  DCHECK(CalledOnValidThread());
  return JSONObject(new base::DictionaryValue());
}

JSONObject RuntimeComponent::Enable(const JSONObject& params) {
  UNREFERENCED_PARAMETER(params);
  DCHECK(CalledOnValidThread());
  OnExecutionContextCreated();
  return JSONObject(new base::DictionaryValue());
}

JSONObject RuntimeComponent::Evaluate(const JSONObject& params) {
  DCHECK(CalledOnValidThread());

  std::string expression;
  bool got_param = params->GetString(kExpression, &expression);
  DCHECK(got_param);
  bool generate_preview = false;
  got_param = params->GetBoolean(kGeneratePreview, &generate_preview);
  DCHECK(got_param);

  // TODO(***REMOVED***): Come up with something better for the source location.
  base::SourceLocation source_location("[object Devtools]", 1, 1);
  scoped_refptr<script::SourceCode> source_code =
      script::SourceCode::CreateSourceCode(expression, source_location);
  std::string eval_result;
  bool eval_succeeded =
      global_object_proxy_->EvaluateScript(source_code, &eval_result);

  // TODO(***REMOVED***): Currently, |EvalulateScript| always returns a string.
  // This should be extended to return arbitrary data: objects, lists, etc.

  JSONObject result(new base::DictionaryValue());
  result->SetString(kResultValue, eval_result);
  result->SetString(kResultType, "string");
  result->SetBoolean(kWasThrown, !eval_succeeded);
  return result.Pass();
}

JSONObject RuntimeComponent::ReleaseObjectGroup(const JSONObject& params) {
  // Currently does nothing except reduce some logging noise, as this method
  // is called a lot.
  UNREFERENCED_PARAMETER(params);
  DCHECK(CalledOnValidThread());
  return JSONObject(new base::DictionaryValue());
}

void RuntimeComponent::OnExecutionContextCreated() {
  JSONObject notification(new base::DictionaryValue());
  notification->SetString(kContextFrameId, kContextFrameIdValue);
  notification->SetInteger(kContextId, kContextIdValue);
  notification->SetString(kContextName, kContextNameValue);
  SendNotification(kExecutionContextCreated, notification);
}

}  // namespace debug
}  // namespace cobalt
