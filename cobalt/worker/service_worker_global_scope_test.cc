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

#include "cobalt/worker/service_worker_global_scope.h"

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

namespace {

class ServiceWorkerGlobalScopeTest
    : public testing::TestServiceWorkerWithJavaScript {
 protected:
  ServiceWorkerGlobalScopeTest() {
    fake_event_listener_ = MockEventListener::Create();
  }

  ~ServiceWorkerGlobalScopeTest() override {}

  std::unique_ptr<MockEventListener> fake_event_listener_;
};

}  // namespace

TEST_F(ServiceWorkerGlobalScopeTest, RegistrationIsObject) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.registration", &result));
  EXPECT_EQ("object", result);
}

TEST_F(ServiceWorkerGlobalScopeTest, ServiceWorkerIsObject) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.serviceWorker", &result));
  EXPECT_EQ("object", result);
}

TEST_F(ServiceWorkerGlobalScopeTest, SkipWaitingIsFunction) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.skipWaiting", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("self.skipWaiting", &result));
  EXPECT_EQ("function skipWaiting() { [native code] }", result);
}

TEST_F(ServiceWorkerGlobalScopeTest, ActivateEvent) {
  worker_global_scope()->AddEventListener(
      "activate",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("activate",
                                              worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("activate"));
}

TEST_F(ServiceWorkerGlobalScopeTest, OnActivateEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onactivate", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onactivate = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('activate'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_F(ServiceWorkerGlobalScopeTest, FetchEvent) {
  worker_global_scope()->AddEventListener(
      "fetch", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("fetch", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("fetch"));
}

TEST_F(ServiceWorkerGlobalScopeTest, OnFetchEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onfetch", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onfetch = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('fetch'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_F(ServiceWorkerGlobalScopeTest, InstallEvent) {
  worker_global_scope()->AddEventListener(
      "install",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("install", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("install"));
}

TEST_F(ServiceWorkerGlobalScopeTest, OnInstallEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.oninstall", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.oninstall = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('install'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

// Note: message and messageerror are tested in WorkerGlobalScopeTest because
// they exist in both DedicatedWorkerGlobalScope and ServiceWorkerGlobalScope.

}  // namespace worker
}  // namespace cobalt
