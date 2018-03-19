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

#include "cobalt/dom/window.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/network/network_module.h"
#include "cobalt/network_bridge/net_poster.h"
#include "googleurl/src/gurl.h"
#include "starboard/window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

class WindowTest : public ::testing::Test {
 protected:
  WindowTest()
      : message_loop_(MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(mock_error_callback_)),
        fetcher_factory_(new loader::FetcherFactory(&network_module_)),
        local_storage_database_(NULL),
        url_("about:blank"),
        window_(new Window(
            1920, 1080, 1.f, base::kApplicationStateStarted, css_parser_.get(),
            dom_parser_.get(), fetcher_factory_.get(), NULL, NULL, NULL, NULL,
            NULL, NULL, &local_storage_database_, NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, url_, "", "en-US", "en",
            base::Callback<void(const GURL &)>(),
            base::Bind(&MockErrorCallback::Run,
                       base::Unretained(&mock_error_callback_)),
            NULL, network_bridge::PostSender(), csp::kCSPRequired,
            kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
            base::Closure() /* ran_animation_frame_callbacks */,
            dom::Window::CloseCallback() /* window_close */,
            base::Closure() /* window_minimize */, NULL, NULL, NULL,
            dom::Window::OnStartDispatchEventCallback(),
            dom::Window::OnStopDispatchEventCallback(),
            dom::ScreenshotManager::ProvideScreenshotFunctionCallback())) {}

  ~WindowTest() override {}

  MessageLoop message_loop_;
  MockErrorCallback mock_error_callback_;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  network::NetworkModule network_module_;
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;
  scoped_refptr<Window> window_;
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

}  // namespace dom
}  // namespace cobalt
