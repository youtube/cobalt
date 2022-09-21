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

#ifndef COBALT_WEB_TESTING_TEST_WITH_JAVASCRIPT_H_
#define COBALT_WEB_TESTING_TEST_WITH_JAVASCRIPT_H_

#include <memory>
#include <string>
#include <vector>

#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/worker/testing/test_with_javascript.h"

namespace cobalt {
namespace web {
namespace testing {

// Helper class for extending the worker tests with JavaScript to include
// testing in Window global scope.
template <class TypeIdProvider>
class TestWithJavaScriptBase
    : public worker::testing::TestWithJavaScriptBase<TypeIdProvider> {
 public:
  TestWithJavaScriptBase() {
    if (TypeIdProvider::GetGlobalScopeTypeId() ==
        base::GetTypeId<dom::Window>()) {
      DCHECK(!this->worker_global_scope());
      this->ClearWebContext();
      window_.reset(new dom::testing::StubWindow());
      window_->InitializeWindow();
    }
  }

  web::Context* web_context() const override {
    if (window_) return window_->web_context();
    return worker::testing::TestWithJavaScriptBase<
        TypeIdProvider>::web_context();
  }

 private:
  std::unique_ptr<dom::testing::StubWindow> window_;
};

// Helper class for running TEST_P() tests on all known worker types.
class TestWebWithJavaScript
    : public TestWithJavaScriptBase<
          worker::testing::GetGlobalScopeTypeIdWithParam> {
 public:
  // Return a vector of values for all known worker types, to be used in the
  // INSTANTIATE_TEST_CASE_P() declaration.
  static std::vector<base::TypeId> GetWebTypes() {
    std::vector<base::TypeId> worker_types =
        worker::testing::TestWorkersWithJavaScript::GetWorkerTypes();
    worker_types.push_back(base::GetTypeId<dom::Window>());
    return worker_types;
  }
  static std::string GetTypeName(::testing::TestParamInfo<base::TypeId> info) {
    if (info.param == base::GetTypeId<dom::Window>()) {
      return "Window";
    }
    return worker::testing::TestWorkersWithJavaScript::GetTypeName(info);
  }
};

}  // namespace testing
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_TESTING_TEST_WITH_JAVASCRIPT_H_
