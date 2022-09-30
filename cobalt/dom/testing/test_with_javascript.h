// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_TEST_WITH_JAVASCRIPT_H_
#define COBALT_DOM_TESTING_TEST_WITH_JAVASCRIPT_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/context.h"

namespace cobalt {
namespace dom {
namespace testing {

// Helper class for running tests in a Window JavaScript context.
class TestWithJavaScript : public ::testing::Test {
 public:
  TestWithJavaScript() { stub_window_.reset(new StubWindow()); }
  ~TestWithJavaScript() {
    stub_window_.reset();
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }

  StubWindow* stub_window() const { return stub_window_.get(); }
  web::testing::StubWebContext* stub_web_context() const {
    DCHECK(stub_window_);
    return stub_window_->web_context();
  }

  Window* window() const {
    DCHECK(stub_window_);
    return stub_window_->window().get();
  }

  web::Context* web_context() const { return stub_web_context(); }

  scoped_refptr<script::GlobalEnvironment> global_environment() const {
    DCHECK(web_context());
    return web_context()->global_environment();
  }

  bool EvaluateScript(const std::string& js_code,
                      std::string* result = nullptr) {
    DCHECK(global_environment());
    return global_environment()->EvaluateScript(
        CreateSourceCodeAndPrepareEval(js_code), result);
  }

  bool EvaluateScript(
      const std::string& js_code,
      base::Optional<script::ValueHandleHolder::Reference>* result) {
    DCHECK(global_environment());
    return global_environment()->EvaluateScript(
        CreateSourceCodeAndPrepareEval(js_code),
        web_context()->GetWindowOrWorkerGlobalScope(), result);
  }

  ::testing::StrictMock<script::testing::MockExceptionState>*
  exception_state() {
    return &exception_state_;
  }

  scoped_refptr<script::GlobalEnvironment> global_environment() {
    return stub_window_->global_environment();
  }

  base::EventDispatcher* event_dispatcher() { return &event_dispatcher_; }

 private:
  scoped_refptr<script::SourceCode> CreateSourceCodeAndPrepareEval(
      const std::string& js_code) {
    DCHECK(global_environment());
    global_environment()->EnableEval();
    global_environment()->SetReportEvalCallback(base::Closure());
    return script::SourceCode::CreateSourceCode(
        js_code, base::SourceLocation(__FILE__, __LINE__, 1));
  }

  std::unique_ptr<StubWindow> stub_window_;
  ::testing::StrictMock<script::testing::MockExceptionState> exception_state_;
  base::EventDispatcher event_dispatcher_;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_TEST_WITH_JAVASCRIPT_H_
