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
  TestWithJavaScript() {}
  ~TestWithJavaScript() {
    if (stub_window_) {
      stub_window_->global_environment()->SetReportEvalCallback(
          base::Closure());
      stub_window_->global_environment()->SetReportErrorCallback(
          script::GlobalEnvironment::ReportErrorCallback());
      DCHECK(window());
      window()->DispatchEvent(new web::Event(base::Tokens::unload()));
      stub_window_.reset();
    }
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }

  void set_stub_window(StubWindow* stub_window) {
    stub_window_.reset(stub_window);
  }

  Window* window() {
    if (!stub_window_) stub_window_.reset(new StubWindow());
    return stub_window_->window().get();
  }

  bool EvaluateScript(const std::string& js_code, std::string* result) {
    if (!stub_window_) stub_window_.reset(new StubWindow());
    DCHECK(stub_window_->global_environment());
    scoped_refptr<script::SourceCode> source_code =
        script::SourceCode::CreateSourceCode(
            js_code, base::SourceLocation(__FILE__, __LINE__, 1));

    stub_window_->global_environment()->EnableEval();
    stub_window_->global_environment()->SetReportEvalCallback(base::Closure());
    bool succeeded =
        stub_window_->global_environment()->EvaluateScript(source_code, result);
    return succeeded;
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
  std::unique_ptr<StubWindow> stub_window_;
  ::testing::StrictMock<script::testing::MockExceptionState> exception_state_;
  base::EventDispatcher event_dispatcher_;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_TEST_WITH_JAVASCRIPT_H_
