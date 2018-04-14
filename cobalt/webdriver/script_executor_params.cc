// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/webdriver/script_executor_params.h"

#include "base/stringprintf.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace webdriver {

ScriptExecutorParams::GCPreventedParams ScriptExecutorParams::Create(
    const scoped_refptr<script::GlobalEnvironment>& global_environment,
    const std::string& function_body, const std::string& json_args,
    base::optional<base::TimeDelta> async_timeout) {
  scoped_refptr<ScriptExecutorParams> params(new ScriptExecutorParams());
  global_environment->PreventGarbageCollection(params);
  params->json_args_ = json_args;

  if (async_timeout) {
    int async_timeout_ms = async_timeout->InMilliseconds();
    params->async_timeout_ = async_timeout_ms;
  }

  std::string function =
      StringPrintf("(function() {\n%s\n})", function_body.c_str());
  scoped_refptr<script::SourceCode> function_source =
      script::SourceCode::CreateSourceCode(
          function.c_str(), base::SourceLocation("[webdriver]", 1, 1));

  if (!global_environment->EvaluateScript(function_source, params.get(),

                                          &params->function_object_)) {
    DLOG(ERROR) << "Failed to create Function object";
  }
  return {params, global_environment.get()};
}

}  // namespace webdriver
}  // namespace cobalt
