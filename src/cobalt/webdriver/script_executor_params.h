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

#ifndef COBALT_WEBDRIVER_SCRIPT_EXECUTOR_PARAMS_H_
#define COBALT_WEBDRIVER_SCRIPT_EXECUTOR_PARAMS_H_

#if defined(ENABLE_WEBDRIVER)

#include <string>

#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace webdriver {

// An instance of the ScriptExecutorResult is passed to the webdriver script
// execution harness to collect the results of running a script via webdriver.
// The results are forwarded to a ResultHandler instance.
class ScriptExecutorParams : public script::Wrappable {
 public:
  static scoped_refptr<ScriptExecutorParams> Create(
      const scoped_refptr<script::GlobalEnvironment>& global_environment,
      const std::string& function_body, const std::string& json_args) {
    return Create(global_environment, function_body, json_args, base::nullopt);
  }

  static scoped_refptr<ScriptExecutorParams> Create(
      const scoped_refptr<script::GlobalEnvironment>& global_environment,
      const std::string& function_body, const std::string& json_args,
      base::optional<base::TimeDelta> async_timeout);

  const script::OpaqueHandleHolder* function_object() {
    return function_object_ ? &function_object_->referenced_value() : NULL;
  }
  const std::string& json_args() { return json_args_; }
  base::optional<int32_t> async_timeout() { return async_timeout_; }

  DEFINE_WRAPPABLE_TYPE(ScriptExecutorParams);

 private:
  std::string function_body_;
  base::optional<script::OpaqueHandleHolder::Reference> function_object_;
  std::string json_args_;
  base::optional<int32_t> async_timeout_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)

#endif  // COBALT_WEBDRIVER_SCRIPT_EXECUTOR_PARAMS_H_
