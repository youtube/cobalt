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
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/network_bridge/net_poster.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "starboard/window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace dom {

using ::cobalt::script::testing::FakeScriptValue;
using testing::MockEventListener;

class MockErrorCallback
    : public base::Callback<void(const base::Optional<std::string> &)> {
 public:
  MOCK_METHOD1(Run, void(const base::Optional<std::string> &));
};

class WindowTest : public ::testing::Test {
 protected:
  WindowTest()
      : environment_settings_(new testing::StubEnvironmentSettings),
        message_loop_(base::MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(mock_error_callback_)),
        fetcher_factory_(new loader::FetcherFactory(NULL)),
        local_storage_database_(NULL),
        url_("about:blank") {
    engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = engine_->CreateGlobalEnvironment();

    ViewportSize view_size(1920, 1080);
    window_ = new Window(
        environment_settings_.get(), view_size, base::kApplicationStateStarted,
        css_parser_.get(), dom_parser_.get(), fetcher_factory_.get(), NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, &local_storage_database_, NULL,
        NULL, NULL, NULL, global_environment_->script_value_factory(), NULL,
        NULL, url_, "", NULL, "en-US", "en",
        base::Callback<void(const GURL &)>(),
        base::Bind(&MockErrorCallback::Run,
                   base::Unretained(&mock_error_callback_)),
        NULL, network_bridge::PostSender(), csp::kCSPRequired,
        kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
        base::Closure() /* ran_animation_frame_callbacks */,
        dom::Window::CloseCallback() /* window_close */,
        base::Closure() /* window_minimize */, NULL, NULL,
        dom::Window::OnStartDispatchEventCallback(),
        dom::Window::OnStopDispatchEventCallback(),
        dom::ScreenshotManager::ProvideScreenshotFunctionCallback(), NULL);
    fake_event_listener_ = MockEventListener::Create();
  }

  ~WindowTest() override {}

  const std::unique_ptr<testing::StubEnvironmentSettings> environment_settings_;
  base::MessageLoop message_loop_;
  MockErrorCallback mock_error_callback_;
  std::unique_ptr<css_parser::Parser> css_parser_;
  std::unique_ptr<dom_parser::Parser> dom_parser_;
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  std::unique_ptr<script::JavaScriptEngine> engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  GURL url_;
  scoped_refptr<Window> window_;
  std::unique_ptr<MockEventListener> fake_event_listener_;
};

TEST_F(WindowTest, WindowShouldNotHaveChildren) {
  EXPECT_EQ(window_, window_->window());
  EXPECT_EQ(window_, window_->self());
  EXPECT_EQ(window_, window_->frames());
  EXPECT_EQ(0, window_->length());
  EXPECT_EQ(window_, window_->top());
  EXPECT_EQ(window_, window_->opener());
  EXPECT_EQ(window_, window_->parent());
  EXPECT_FALSE(window_->AnonymousIndexedGetter(0));
}

TEST_F(WindowTest, ViewportSize) {
  EXPECT_FLOAT_EQ(window_->inner_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window_->inner_height(), 1080.0f);
  EXPECT_FLOAT_EQ(window_->screen_x(), 0.0f);
  EXPECT_FLOAT_EQ(window_->screen_y(), 0.0f);
  EXPECT_FLOAT_EQ(window_->outer_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window_->outer_height(), 1080.0f);
  EXPECT_FLOAT_EQ(window_->device_pixel_ratio(), 1.0f);
  EXPECT_FLOAT_EQ(window_->screen()->width(), 1920.0f);
  EXPECT_FLOAT_EQ(window_->screen()->height(), 1080.0f);
  EXPECT_FLOAT_EQ(window_->screen()->avail_width(), 1920.0f);
  EXPECT_FLOAT_EQ(window_->screen()->avail_height(), 1080.0f);
}

// Test that when Window's network status change callbacks are triggered,
// corresponding online and offline events are fired to listeners.
TEST_F(WindowTest, OnlineEvent) {
  window_->AddEventListener(
      "online", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("online", window_);
  window_->OnWindowOnOnlineEvent();
}

TEST_F(WindowTest, OfflineEvent) {
  window_->AddEventListener(
      "offline", FakeScriptValue<EventListener>(fake_event_listener_.get()),
      true);
  fake_event_listener_->ExpectHandleEventCall("offline", window_);
  window_->OnWindowOnOfflineEvent();
}

}  // namespace dom
}  // namespace cobalt
