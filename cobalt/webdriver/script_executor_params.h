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
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace webdriver {

// An instance of the ScriptExecutorResult is passed to the webdriver script
// execution harness to collect the results of running a script via webdriver.
// The results are forwarded to a ResultHandler instance.
class ScriptExecutorParams : public script::Wrappable {
 public:
  // |ScriptExecutorParams| will typically be used as a "root" Wrappable.  In
  // order to prevent the underlying JavaScript object that |function_object_|
  // wraps from being garbage collected, we must ensure the the |Wrappable|
  // that references it (which is us) is reachable, which we accomplish by
  // immediately calling |PreventGarbageCollection| on it.
  // |GCPreventedParams| then manages this pre-gc-prevented
  // |ScriptExecutorParams|.
  struct GCPreventedParams {
    GCPreventedParams(const scoped_refptr<ScriptExecutorParams>& params,
                      script::GlobalEnvironment* global_environment)
        : params(params), global_environment(global_environment) {}
    GCPreventedParams(const GCPreventedParams&) = delete;
    GCPreventedParams& operator=(const GCPreventedParams&) = delete;
    GCPreventedParams(GCPreventedParams&& other)
        : params(other.params), global_environment(other.global_environment) {
      other.params = nullptr;
    }
    GCPreventedParams& operator=(GCPreventedParams&& other) {
      global_environment->AllowGarbageCollection(params);
      params = other.params;
      other.params = nullptr;
      return *this;
    }
    ~GCPreventedParams() {
      if (params) {
        global_environment->AllowGarbageCollection(params);
      }
    }

    scoped_refptr<ScriptExecutorParams> params;
    script::GlobalEnvironment* global_environment;
  };

  static GCPreventedParams Create(
      const scoped_refptr<script::GlobalEnvironment>& global_environment,
      const std::string& function_body, const std::string& json_args) {
    return Create(global_environment, function_body, json_args, base::nullopt);
  }

  static GCPreventedParams Create(
      const scoped_refptr<script::GlobalEnvironment>& global_environment,
      const std::string& function_body, const std::string& json_args,
      base::optional<base::TimeDelta> async_timeout);

  const script::ValueHandleHolder* function_object() {
    return function_object_ ? &function_object_->referenced_value() : NULL;
  }
  const std::string& json_args() { return json_args_; }
  base::optional<int32_t> async_timeout() { return async_timeout_; }

  DEFINE_WRAPPABLE_TYPE(ScriptExecutorParams);

 private:
  std::string function_body_;
  base::optional<script::ValueHandleHolder::Reference> function_object_;
  std::string json_args_;
  base::optional<int32_t> async_timeout_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)

#endif  // COBALT_WEBDRIVER_SCRIPT_EXECUTOR_PARAMS_H_
