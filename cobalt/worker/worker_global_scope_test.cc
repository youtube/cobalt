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

#include "cobalt/worker/worker_global_scope.h"

#include <memory>

#include "cobalt/base/tokens.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/web/error_event.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "cobalt/worker/testing/test_with_javascript.h"

namespace cobalt {
namespace worker {

using script::testing::FakeScriptValue;
using web::testing::MockEventListener;

class WorkerGlobalScopeTest : public testing::TestWorkersWithJavaScript {
 protected:
  WorkerGlobalScopeTest() {
    fake_event_listener_ = MockEventListener::Create();
  }

  ~WorkerGlobalScopeTest() override {}

  std::unique_ptr<MockEventListener> fake_event_listener_;
};

TEST_P(WorkerGlobalScopeTest, SelfIsExpectedWorkerGlobalScope) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("self", &result));
  if (GetGlobalScopeTypeId() == base::GetTypeId<DedicatedWorkerGlobalScope>()) {
    EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString(
        "DedicatedWorkerGlobalScope", result));
  } else if (GetGlobalScopeTypeId() ==
             base::GetTypeId<ServiceWorkerGlobalScope>()) {
    EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString(
        "ServiceWorkerGlobalScope", result));
  } else {
    FAIL() << "Unknown worker type with global scope : " << result;
  }
}

TEST_P(WorkerGlobalScopeTest, ImportScriptsIsFunction) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.importScripts", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("self.importScripts", &result));
  EXPECT_EQ("function importScripts() { [native code] }", result);
}

TEST_P(WorkerGlobalScopeTest, ErrorEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onerror", &result));
  EXPECT_EQ("object", result);

  worker_global_scope()->AddEventListener(
      "error", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("error", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::ErrorEvent("error"));
}

// Test that when Window's network status change callbacks are triggered,
// corresponding online and offline events are fired to listeners.
TEST_P(WorkerGlobalScopeTest, OfflineEvent) {
  worker_global_scope()->AddEventListener(
      "offline",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("offline", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event(base::Tokens::offline()));
}

TEST_P(WorkerGlobalScopeTest, OnlineEvent) {
  worker_global_scope()->AddEventListener(
      "online", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("online", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event(base::Tokens::online()));
}

INSTANTIATE_TEST_CASE_P(
    WorkerGlobalScopeTests, WorkerGlobalScopeTest,
    ::testing::ValuesIn(testing::TestWorkersWithJavaScript::GetWorkerTypes()));

}  // namespace worker
}  // namespace cobalt
