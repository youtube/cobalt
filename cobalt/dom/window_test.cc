// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/window.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/network_bridge/net_poster.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/web/error_event.h"
#include "cobalt/web/testing/mock_event_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

using script::testing::FakeScriptValue;
using web::testing::MockEventListener;

class WindowTest : public testing::TestWithJavaScript {
 protected:
  WindowTest() { fake_event_listener_ = MockEventListener::Create(); }

  ~WindowTest() override {}

  std::unique_ptr<MockEventListener> fake_event_listener_;
};

TEST_F(WindowTest, WindowShouldNotHaveChildren) {
  EXPECT_EQ(window(), window()->window());
  EXPECT_EQ(window(), window()->self());
  EXPECT_EQ(window(), window()->frames());
  EXPECT_EQ(0, window()->length());
  EXPECT_EQ(window(), window()->top());
  EXPECT_EQ(window(), window()->opener());
  EXPECT_EQ(window(), window()->parent());
  EXPECT_FALSE(window()->AnonymousIndexedGetter(0));
}

TEST_F(WindowTest, ViewportSize) {
  EXPECT_FLOAT_EQ(window()->inner_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window()->inner_height(), 1080.0f);
  EXPECT_FLOAT_EQ(window()->screen_x(), 0.0f);
  EXPECT_FLOAT_EQ(window()->screen_y(), 0.0f);
  EXPECT_FLOAT_EQ(window()->outer_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window()->outer_height(), 1080.0f);
  EXPECT_FLOAT_EQ(window()->device_pixel_ratio(), 1.0f);
  EXPECT_FLOAT_EQ(window()->screen()->width(), 1920.0f);
  EXPECT_FLOAT_EQ(window()->screen()->height(), 1080.0f);
  EXPECT_FLOAT_EQ(window()->screen()->avail_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window()->screen()->avail_height(), 1080.0f);
}

TEST_F(WindowTest, WindowIsWindow) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));

  EXPECT_TRUE(EvaluateScript("typeof window.window", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.window", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));
}

TEST_F(WindowTest, SelfIsWindow) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.self", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.self", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));
}

TEST_F(WindowTest, DocumentIsDocument) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.document", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.document", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Document", result));
}

TEST_F(WindowTest, LocationIsObject) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.location", &result));
  EXPECT_EQ("object", result);

  // Note: Due the stringifier of URLUtils.href implemented by this attribute,
  // the prototype is not recognized as type Location, and the href is returned.
  EXPECT_TRUE(EvaluateScript("window.location", &result));
  EXPECT_EQ("about:blank", result);
}

TEST_F(WindowTest, HistoryIsHistory) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.history", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.history", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("History", result));
}

TEST_F(WindowTest, FramesIsWindow) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.frames", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.frames", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));
}

TEST_F(WindowTest, LengthIsZero) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.length", &result));
  EXPECT_EQ("number", result);

  EXPECT_TRUE(EvaluateScript("window.length", &result));
  EXPECT_EQ("0", result);
}

TEST_F(WindowTest, TopIsWindow) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.top", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.top", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));
}

TEST_F(WindowTest, OpenerIsWindow) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.opener", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.opener", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));
}

TEST_F(WindowTest, ParentIsWindow) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.parent", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.parent", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Window", result));
}

TEST_F(WindowTest, NavigatorIsNavigator) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.navigator", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.navigator", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Navigator", result));
}

TEST_F(WindowTest, Btoa) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.btoa", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("window.btoa", &result));
  EXPECT_EQ("function btoa() { [native code] }", result);
  EXPECT_TRUE(EvaluateScript("window.btoa('test')", &result));
  EXPECT_EQ("dGVzdA==", result);
}

TEST_F(WindowTest, Atob) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.atob", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("window.atob", &result));
  EXPECT_EQ("function atob() { [native code] }", result);
  EXPECT_TRUE(EvaluateScript("window.atob('dGVzdA==')", &result));
  EXPECT_EQ("test", result);
}

TEST_F(WindowTest, Camera3DIsCamera3D) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.camera3D", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript("window.camera3D", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Camera3D", result));
}

TEST_F(WindowTest, Gc) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.gc", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("window.gc", &result));
  EXPECT_EQ("function gc() { [native code] }", result);
  EXPECT_TRUE(EvaluateScript("window.gc()", &result));
  EXPECT_EQ("undefined", result);
}

TEST_F(WindowTest, Minimize) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.minimize", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("window.minimize", &result));
  EXPECT_EQ("function minimize() { [native code] }", result);
  EXPECT_TRUE(EvaluateScript("window.minimize()", &result));
  EXPECT_EQ("undefined", result);
}

TEST_F(WindowTest, Screenshot) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.screenshot", &result));
  EXPECT_EQ("function", result);

  EXPECT_TRUE(EvaluateScript("window.screenshot", &result));
  EXPECT_EQ("function screenshot() { [native code] }", result);

  // Note we can't call screenshot in this unit test, since StubWindow doesn't
  // pass in the screenshot function callback.
}

TEST_F(WindowTest, ErrorEvent) {
  window()->AddEventListener(
      "error", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("error", window());
  window()->DispatchEvent(new web::ErrorEvent("error"));
}

TEST_F(WindowTest, OnErrorEvent) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof window.onerror", &result));
  EXPECT_EQ("object", result);

  EXPECT_TRUE(EvaluateScript(R"(
    logString = '(empty)';
    self.onerror = function() {
      logString = 'handled';
    };
    window.dispatchEvent(new ErrorEvent('error'));
    logString;
  )",
                             &result));
  EXPECT_EQ("handled", result);
}

// Test that when Window's network status change callbacks are triggered,
// corresponding online and offline events are fired to listeners.
TEST_F(WindowTest, OfflineEvent) {
  window()->AddEventListener(
      "offline",
      FakeScriptValue<web::EventListener>(fake_event_listener_.get()), true);
  fake_event_listener_->ExpectHandleEventCall("offline", window());
  window()->OnWindowOnOfflineEvent();
}

TEST_F(WindowTest, OnlineEvent) {
  window()->AddEventListener(
      "online", FakeScriptValue<web::EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("online", window());
  window()->OnWindowOnOnlineEvent();
}

}  // namespace dom
}  // namespace cobalt
