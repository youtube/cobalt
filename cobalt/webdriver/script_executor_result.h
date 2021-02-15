// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_SCRIPT_EXECUTOR_RESULT_H_
#define COBALT_WEBDRIVER_SCRIPT_EXECUTOR_RESULT_H_

#if defined(ENABLE_WEBDRIVER)

#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace webdriver {

// An instance of the ScriptExecutorResult is passed to the webdriver script
// execution harness to collect the results of running a script via webdriver.
// The results are forwarded to a ResultHandler instance.
class ScriptExecutorResult : public script::Wrappable {
 public:
  // The ScriptExecutorResult class must only be accessed from the main thread,
  // so use an instance of the ResultHandler class to access the results of
  // execution from another thread.
  // Exactly one of OnResult and OnTimeout will be called. After one of these
  // is called, the ScriptExecutorResult object will no longer hold a pointer
  // to the ResultHandler instance, so it's safe for the owning thread to
  // destroy it.
  class ResultHandler {
   public:
    virtual void OnResult(const std::string& result) = 0;
    virtual void OnTimeout() = 0;
  };

  explicit ScriptExecutorResult(ScriptExecutorResult::ResultHandler* handler)
      : result_handler_(handler) {}

  void OnResult(const std::string& result) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (result_handler_) {
      result_handler_->OnResult(result);
      result_handler_ = NULL;
    }
  }

  void OnTimeout() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (result_handler_) {
      result_handler_->OnTimeout();
      result_handler_ = NULL;
    }
  }

  DEFINE_WRAPPABLE_TYPE(ScriptExecutorResult);

 private:
  THREAD_CHECKER(thread_checker_);
  ResultHandler* result_handler_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)

#endif  // COBALT_WEBDRIVER_SCRIPT_EXECUTOR_RESULT_H_
