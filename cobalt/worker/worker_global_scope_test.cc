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
#include "cobalt/web/message_event.h"
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

TEST_P(WorkerGlobalScopeTest, LocationIsObject) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.location", &result));
  EXPECT_EQ("object", result);

  // Note: Due the stringifier of URLUtils.href implemented by this attribute,
  // the prototype is not recognized as type Location, and the href is returned.
  EXPECT_TRUE(EvaluateScript("self.location", &result));
  EXPECT_EQ("about:blank", result);
}

TEST_P(WorkerGlobalScopeTest, NavigatorIsWorkerNavigator) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.navigator", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("self.navigator", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("WorkerNavigator",
                                                             result));
}

TEST_P(WorkerGlobalScopeTest, ImportScriptsIsFunction) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.importScripts", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("self.importScripts", &result));
  EXPECT_EQ("function importScripts() { [native code] }", result);
}

TEST_P(WorkerGlobalScopeTest, MessageEvent) {
  worker_global_scope()->AddEventListener(
      "message",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("message", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::MessageEvent("message"));
}

TEST_P(WorkerGlobalScopeTest, OnMessageEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onmessage", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onmessage = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('message'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_P(WorkerGlobalScopeTest, MessageErrorEvent) {
  worker_global_scope()->AddEventListener(
      "messageerror",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("messageerror",
                                              worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::MessageEvent("messageerror"));
}

TEST_P(WorkerGlobalScopeTest, OnMessageErrorEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onmessageerror", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onmessageerror = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('messageerror'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_P(WorkerGlobalScopeTest, ErrorEvent) {
  worker_global_scope()->AddEventListener(
      "error", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("error", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::ErrorEvent());
}

TEST_P(WorkerGlobalScopeTest, OnErrorEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onerror", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onerror = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new ErrorEvent('error'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_P(WorkerGlobalScopeTest, LanguagechangeEvent) {
  worker_global_scope()->AddEventListener(
      "languagechange",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("languagechange",
                                              worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("languagechange"));
}

TEST_P(WorkerGlobalScopeTest, OnLanguagechangeEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onlanguagechange", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onlanguagechange = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('languagechange'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

// Test that when Worker's network status change callbacks are triggered,
// corresponding online and offline events are fired to listeners.
TEST_P(WorkerGlobalScopeTest, OfflineEvent) {
  worker_global_scope()->AddEventListener(
      "offline",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("offline", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event(base::Tokens::offline()));
}

TEST_P(WorkerGlobalScopeTest, OfflineEventDispatch) {
  worker_global_scope()->AddEventListener(
      "offline",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("offline", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("offline"));
}

TEST_P(WorkerGlobalScopeTest, OnOfflineEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onoffline", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onoffline = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('offline'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_P(WorkerGlobalScopeTest, OnlineEvent) {
  worker_global_scope()->AddEventListener(
      "online", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("online", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event(base::Tokens::online()));
}

TEST_P(WorkerGlobalScopeTest, OnlineEventDispatch) {
  worker_global_scope()->AddEventListener(
      "online", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("online", worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("online"));
}

TEST_P(WorkerGlobalScopeTest, OnOnlineEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.ononline", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.ononline = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('online'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_P(WorkerGlobalScopeTest, RejectionhandledEvent) {
  worker_global_scope()->AddEventListener(
      "rejectionhandled",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("rejectionhandled",
                                              worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("rejectionhandled"));
}

TEST_P(WorkerGlobalScopeTest, OnRejectionhandledEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onrejectionhandled", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onrejectionhandled = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('rejectionhandled'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

TEST_P(WorkerGlobalScopeTest, UnhandledrejectionEvent) {
  worker_global_scope()->AddEventListener(
      "unhandledrejection",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("unhandledrejection",
                                              worker_global_scope());
  worker_global_scope()->DispatchEvent(new web::Event("unhandledrejection"));
}

TEST_P(WorkerGlobalScopeTest, OnUnhandledrejectionEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof self.onunhandledrejection", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onunhandledrejection = function() {
      logString = 'handled';
    };
    self.dispatchEvent(new Event('unhandledrejection'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

INSTANTIATE_TEST_CASE_P(
    WorkerGlobalScopeTests, WorkerGlobalScopeTest,
    ::testing::ValuesIn(testing::TestWorkersWithJavaScript::GetWorkerTypes()),
    testing::TestWorkersWithJavaScript::GetTypeName);

}  // namespace worker
}  // namespace cobalt
