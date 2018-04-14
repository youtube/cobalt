// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_BINDINGS_TEST_BASE_H_
#define COBALT_BINDINGS_TESTING_BINDINGS_TEST_BASE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/bindings/testing/window.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace testing {
// Define a default gmock value for scoped_refptr<T> types. This must be
// declared in gmock's namespace.
template <typename T>
class DefaultValue<scoped_refptr<T> > {
 public:
  static bool Exists() { return true; }
  static scoped_refptr<T> Get() { return NULL; }
};
}  // namespace testing

namespace cobalt {
namespace bindings {
namespace testing {

class BindingsTestBase : public ::testing::Test {
 protected:
  BindingsTestBase()
      : environment_settings_(new script::EnvironmentSettings),
        engine_(script::JavaScriptEngine::CreateEngine()),
        global_environment_(engine_->CreateGlobalEnvironment()),
        window_(new Window()) {
    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());
  }

  explicit BindingsTestBase(const scoped_refptr<Window> window)
      : environment_settings_(new script::EnvironmentSettings),
        engine_(script::JavaScriptEngine::CreateEngine()),
        global_environment_(engine_->CreateGlobalEnvironment()),
        window_(window) {
    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());
  }

  bool EvaluateScript(const std::string& script,
                      std::string* out_result = NULL) {
    scoped_refptr<script::SourceCode> source =
        script::SourceCode::CreateSourceCode(
            script, base::SourceLocation("[object BindingsTestBase]", 1, 1));
    return global_environment_->EvaluateScript(source, out_result);
  }

  bool EvaluateScript(const std::string& script,
                      const scoped_refptr<script::Wrappable>& owning_object,
                      base::optional<script::ValueHandleHolder::Reference>*
                          out_value_handle = NULL) {
    scoped_refptr<script::SourceCode> source =
        script::SourceCode::CreateSourceCode(
            script, base::SourceLocation("[object BindingsTestBase]", 1, 1));
    return global_environment_->EvaluateScript(source, owning_object,
                                               out_value_handle);
  }

  void CollectGarbage() { engine_->CollectGarbage(); }

  Window* window() { return window_.get(); }

 protected:
  const scoped_ptr<script::EnvironmentSettings> environment_settings_;
  const scoped_ptr<script::JavaScriptEngine> engine_;
  const scoped_refptr<script::GlobalEnvironment> global_environment_;
  const scoped_refptr<Window> window_;
};

// Use this fixture to create a new MockT object with a BaseClass wrapper, and
// bind the wrapper to the javascript variable "test".
template <class MockT, class BaseClass = MockT>
class InterfaceBindingsTest : public BindingsTestBase {
 public:
  InterfaceBindingsTest()
      // Use StrictMock so TESTING will fail if unexpected method is called.
      : test_mock_(new ::testing::StrictMock<MockT>()) {
    global_environment_->Bind("test",
                              make_scoped_refptr<BaseClass>((test_mock_)));
  }

  MockT& test_mock() { return *test_mock_.get(); }

  const scoped_refptr<MockT> test_mock_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_BINDINGS_TEST_BASE_H_
