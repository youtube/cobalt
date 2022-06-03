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

#include "cobalt/worker/dedicated_worker_global_scope.h"

#include <memory>

#include "cobalt/base/tokens.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/web/message_event.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "cobalt/worker/testing/test_with_javascript.h"

namespace cobalt {
namespace worker {

using script::testing::FakeScriptValue;
using web::testing::MockEventListener;

class DedicatedWorkerGlobalScopeTest
    : public testing::TestDedicatedWorkerWithJavaScript {
 protected:
  DedicatedWorkerGlobalScopeTest() {
    fake_event_listener_ = MockEventListener::Create();
  }

  ~DedicatedWorkerGlobalScopeTest() override {}

  std::unique_ptr<MockEventListener> fake_event_listener_;
};

TEST_F(DedicatedWorkerGlobalScopeTest, NameIsString) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof this.name", &result));
  EXPECT_EQ("string", result);

  EXPECT_TRUE(EvaluateScript("this.name", &result));
  EXPECT_EQ("TestWithJavaScriptBase", result);
}

TEST_F(DedicatedWorkerGlobalScopeTest, PostMessageIsFunction) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.postMessage", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("self.postMessage", &result));
  EXPECT_EQ("function postMessage() { [native code] }", result);
}

TEST_F(DedicatedWorkerGlobalScopeTest, CloseIsFunction) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.close", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("self.close", &result));
  EXPECT_EQ("function close() { [native code] }", result);
}

// Note: message and messageerror are tested in WorkerGlobalScopeTest because
// they exist in both DedicatedWorkerGlobalScope and ServiceWorkerGlobalScope.

}  // namespace worker
}  // namespace cobalt
