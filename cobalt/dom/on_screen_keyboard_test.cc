// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/source_code.h"
#include "starboard/window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class MockErrorCallback : public base::Callback<void(const std::string&)> {
 public:
  MOCK_METHOD1(Run, void(const std::string&));
};

using ::testing::InSequence;
using ::testing::Mock;

class OnScreenKeyboardMockBridge : public OnScreenKeyboardBridge {
 public:
  void Show(const char* input_text, int ticket) override {
    ShowMock(input_text);
    last_ticket_ = ticket;
    shown_ = true;
    script::Handle<script::Promise<void>> promise =
        LookupPromiseForShowTicket(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
    DCHECK(window_);
    window_->on_screen_keyboard()->DispatchShowEvent(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
    last_ticket_ = -1;
  }

  void Hide(int ticket) override {
    HideMock();
    last_ticket_ = ticket;
    shown_ = false;

    script::Handle<script::Promise<void>> promise =
        LookupPromiseForHideTicket(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
    DCHECK(window_);
    window_->on_screen_keyboard()->DispatchHideEvent(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
    last_ticket_ = -1;
  }

  void Focus(int ticket) override {
    FocusMock();
    last_ticket_ = ticket;
    script::Handle<script::Promise<void>> promise =
        LookupPromiseForFocusTicket(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
    DCHECK(window_);
    window_->on_screen_keyboard()->DispatchFocusEvent(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
    last_ticket_ = -1;
  }

  void Blur(int ticket) override {
    BlurMock();
    last_ticket_ = ticket;
    script::Handle<script::Promise<void>> promise =
        LookupPromiseForBlurTicket(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
    DCHECK(window_);
    window_->on_screen_keyboard()->DispatchBlurEvent(last_ticket_);
    EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
    last_ticket_ = -1;
  }

  bool IsShown() const override { return shown_; }

  scoped_refptr<DOMRect> BoundingRect() const override {
    return BoundingRectMock();
  }

  bool IsValidTicket(int ticket) const override {
    // The mock bridge will always dispatch events immediately once the
    // show/hide/focus/blur function has been called, meaning we will never need
    // to check the validity of a ticket that was not the last one generated.
    // This method will always return false when called outside one of the
    // show/hide/focus/blur methods.
    return ticket != -1 && ticket == last_ticket_;
  }

  void SetKeepFocus(bool keep_focus) override { SetKeepFocusMock(keep_focus); }

  MOCK_METHOD1(ShowMock, void(std::string));
  MOCK_METHOD0(HideMock, void());
  MOCK_METHOD0(BlurMock, void());
  MOCK_METHOD0(FocusMock, void());
  MOCK_CONST_METHOD0(BoundingRectMock, scoped_refptr<DOMRect>());
  MOCK_METHOD1(SetKeepFocusMock, void(bool));

  // We shortcut the event dispatching and handling for tests.
  dom::Window* window_;

 private:
  // OnScreenKeyboardMockBridge needs to be friends with dom::OnScreenKeyboard
  // to implement these functions.
  script::Handle<script::Promise<void>> LookupPromiseForShowTicket(int ticket) {
    DCHECK(window_);
    const auto& map =
        window_->on_screen_keyboard()->ticket_to_show_promise_map_;
    auto it = map.find(ticket);
    DCHECK(it != map.end());
    return script::Handle<script::Promise<void>>(*it->second);
  }

  script::Handle<script::Promise<void>> LookupPromiseForHideTicket(int ticket) {
    DCHECK(window_);
    const auto& map =
        window_->on_screen_keyboard()->ticket_to_hide_promise_map_;
    auto it = map.find(ticket);
    DCHECK(it != map.end());
    return script::Handle<script::Promise<void>>(*it->second);
  }

  script::Handle<script::Promise<void>> LookupPromiseForFocusTicket(
      int ticket) {
    DCHECK(window_);
    const auto& map =
        window_->on_screen_keyboard()->ticket_to_focus_promise_map_;
    auto it = map.find(ticket);
    DCHECK(it != map.end());
    return script::Handle<script::Promise<void>>(*it->second);
  }

  script::Handle<script::Promise<void>> LookupPromiseForBlurTicket(int ticket) {
    DCHECK(window_);
    const auto& map =
        window_->on_screen_keyboard()->ticket_to_blur_promise_map_;
    auto it = map.find(ticket);
    DCHECK(it != map.end());
    return script::Handle<script::Promise<void>>(*it->second);
  }

  bool shown_ = false;
  int last_ticket_ = -1;
};

namespace {

class OnScreenKeyboardTest : public ::testing::Test {
 public:
  OnScreenKeyboardTest()
      : environment_settings_(new script::EnvironmentSettings),
        message_loop_(MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(mock_error_callback_)),
        fetcher_factory_(new loader::FetcherFactory(&network_module_)),
        local_storage_database_(NULL),
        url_("about:blank"),
        engine_(script::JavaScriptEngine::CreateEngine()),
        global_environment_(engine_->CreateGlobalEnvironment()),
        on_screen_keyboard_bridge_(new OnScreenKeyboardMockBridge()),
        window_(new Window(
            1920, 1080, 1.f, base::kApplicationStateStarted, css_parser_.get(),
            dom_parser_.get(), fetcher_factory_.get(), NULL, NULL, NULL, NULL,
            NULL, NULL, &local_storage_database_, NULL, NULL, NULL, NULL,
            global_environment_
                ->script_value_factory() /* script_value_factory */,
            NULL, NULL, url_, "", "en-US", "en",
            base::Callback<void(const GURL&)>(),
            base::Bind(&MockErrorCallback::Run,
                       base::Unretained(&mock_error_callback_)),
            NULL, network_bridge::PostSender(), csp::kCSPRequired,
            kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
            base::Closure() /* ran_animation_frame_callbacks */,
            dom::Window::CloseCallback() /* window_close */,
            base::Closure() /* window_minimize */,
            on_screen_keyboard_bridge_.get(), NULL, NULL,
            dom::Window::OnStartDispatchEventCallback(),
            dom::Window::OnStopDispatchEventCallback())) {
    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());
    on_screen_keyboard_bridge_->window_ = window_;
  }

  ~OnScreenKeyboardTest() {
    global_environment_->SetReportEvalCallback(base::Closure());
    global_environment_->SetReportErrorCallback(
        script::GlobalEnvironment::ReportErrorCallback());
    window_->DispatchEvent(new dom::Event(base::Tokens::unload()));

    // TODO: figure out how to destruct OSK before global environment.
    window_->ReleaseOnScreenKeyboard();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(on_screen_keyboard_bridge()));

    on_screen_keyboard_bridge_.reset();
    window_ = nullptr;
    global_environment_ = nullptr;
  }

  bool EvaluateScript(const std::string& js_code, std::string* result);

  script::GlobalEnvironment* global_environment() const {
    return global_environment_.get();
  }

  OnScreenKeyboardMockBridge* on_screen_keyboard_bridge() const {
    return on_screen_keyboard_bridge_.get();
  }

  Window* window() const { return window_.get(); }

 private:
  const scoped_ptr<script::EnvironmentSettings> environment_settings_;
  MessageLoop message_loop_;
  MockErrorCallback mock_error_callback_;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  network::NetworkModule network_module_;
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;

  scoped_ptr<script::JavaScriptEngine> engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  scoped_ptr<OnScreenKeyboardMockBridge> on_screen_keyboard_bridge_;
  scoped_refptr<Window> window_;
};

// TODO: refactor this into reusable test utility.
bool OnScreenKeyboardTest::EvaluateScript(const std::string& js_code,
                                          std::string* result) {
  DCHECK(global_environment_);
  scoped_refptr<script::SourceCode> source_code =
      script::SourceCode::CreateSourceCode(
          js_code, base::SourceLocation(__FILE__, __LINE__, 1));

  global_environment_->EnableEval();
  global_environment_->SetReportEvalCallback(base::Closure());
  bool succeeded = global_environment_->EvaluateScript(source_code, result);
  return succeeded;
}

}  // namespace

TEST_F(OnScreenKeyboardTest, ObjectExists) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard", &result));

  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("OnScreenKeyboard",
                                                             result));

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.show", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function show()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.hide", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function hide()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.focus", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function focus()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.blur", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function blur()",
                      result.c_str());

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.keepFocus", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.data", &result));
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onshow", &result));
  EXPECT_STREQ("null", result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onhide", &result));
  EXPECT_STREQ("null", result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onblur", &result));
  EXPECT_STREQ("null", result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onfocus", &result));
  EXPECT_STREQ("null", result.c_str());
}

TEST_F(OnScreenKeyboardTest, ShowAndHide) {
  // Not shown.
  std::string result;
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.shown;", &result));
  EXPECT_EQ("false", result);

  {
    InSequence seq;
    EXPECT_CALL(*(on_screen_keyboard_bridge()),
                ShowMock(window()->on_screen_keyboard()->data()));
    EXPECT_CALL(*(on_screen_keyboard_bridge()), HideMock());
  }
  // Show.
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.show();", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.shown;", &result));
  EXPECT_EQ("true", result);

  // Hide.
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.hide();", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.shown;", &result));
  EXPECT_EQ("false", result);
}

TEST_F(OnScreenKeyboardTest, ShowAndHideMultipleTimes) {
  std::string result;
  {
    InSequence seq;
    EXPECT_CALL(*(on_screen_keyboard_bridge()),
                ShowMock(window()->on_screen_keyboard()->data()))
        .Times(3);
    EXPECT_CALL(*(on_screen_keyboard_bridge()), HideMock()).Times(3);
  }

  // Show multiple times.
  const char show_script[] = R"(
    window.onScreenKeyboard.show();
    window.onScreenKeyboard.show();
    window.onScreenKeyboard.show();
  )";
  EXPECT_TRUE(EvaluateScript(show_script, &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));

  // Hide multiple times.
  const char hide_script[] = R"(
    window.onScreenKeyboard.hide();
    window.onScreenKeyboard.hide();
    window.onScreenKeyboard.hide();
  )";
  EXPECT_TRUE(EvaluateScript(hide_script, &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
}

TEST_F(OnScreenKeyboardTest, Data) {
  std::string result = "(empty)";
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.data;", &result));
  EXPECT_EQ("", result);

  std::string utf8_str = u8"z\u6c34\U0001d10b";
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.data = '" + utf8_str +
                                 "';"
                                 "window.onScreenKeyboard.data",
                             &result));
  EXPECT_EQ(utf8_str, result);
}

TEST_F(OnScreenKeyboardTest, FocusAndBlur) {
  std::string result;

  {
    InSequence seq;
    EXPECT_CALL(*(on_screen_keyboard_bridge()), FocusMock());
    EXPECT_CALL(*(on_screen_keyboard_bridge()), BlurMock());
  }
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.focus();", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.blur();", &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
}
TEST_F(OnScreenKeyboardTest, FocusAndBlurMultipleTimes) {
  std::string result;
  {
    InSequence seq;
    EXPECT_CALL(*(on_screen_keyboard_bridge()), FocusMock()).Times(3);
    EXPECT_CALL(*(on_screen_keyboard_bridge()), BlurMock()).Times(3);
  }

  const char focus_script[] = R"(
    window.onScreenKeyboard.focus();
    window.onScreenKeyboard.focus();
    window.onScreenKeyboard.focus();
  )";
  EXPECT_TRUE(EvaluateScript(focus_script, &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
  const char blur_script[] = R"(
    window.onScreenKeyboard.blur();
    window.onScreenKeyboard.blur();
    window.onScreenKeyboard.blur();
  )";
  EXPECT_TRUE(EvaluateScript(blur_script, &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Object", result) ||
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
}

TEST_F(OnScreenKeyboardTest, ShowEventAttribute) {
  EXPECT_CALL(*(on_screen_keyboard_bridge()),
              ShowMock(window()->on_screen_keyboard()->data()))
      .Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script, NULL));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onshow = function() {
      logString = 'show';
    };
    promise = window.onScreenKeyboard.show();
    logString
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("show", result);
  }
}

TEST_F(OnScreenKeyboardTest, ShowEventListeners) {
  std::string result;
  EXPECT_CALL(*(on_screen_keyboard_bridge()),
              ShowMock(window()->on_screen_keyboard()->data()));
  const char script[] = R"(
    let logString1 = '(empty)';
    let logString2 = '(empty)';
    window.onScreenKeyboard.addEventListener('show',
      function() {
        logString1 = 'show1';
      });
    window.onScreenKeyboard.addEventListener('show',
      function() {
        logString2 = 'show2';
      });
    let promise = window.onScreenKeyboard.show();
    logString1
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("show1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ(result, "show2");
}

TEST_F(OnScreenKeyboardTest, HideEventAttribute) {
  EXPECT_CALL(*(on_screen_keyboard_bridge()), HideMock()).Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script, NULL));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onhide = function() {
      logString = 'hide';
    };
    promise = window.onScreenKeyboard.hide();
    logString
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("hide", result);
  }
}

TEST_F(OnScreenKeyboardTest, HideEventListeners) {
  std::string result;
  EXPECT_CALL(*(on_screen_keyboard_bridge()), HideMock());
  const char script[] = R"(
    let logString1 = '(empty)';
    let logString2 = '(empty)';
    window.onScreenKeyboard.addEventListener('hide',
      function() {
        logString1 = 'hide1';
      });
    window.onScreenKeyboard.addEventListener('hide',
      function() {
        logString2 = 'hide2';
      });
    let promise = window.onScreenKeyboard.hide();
    logString1
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("hide1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ(result, "hide2");
}

TEST_F(OnScreenKeyboardTest, FocusEventAttribute) {
  EXPECT_CALL(*(on_screen_keyboard_bridge()), FocusMock()).Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script, NULL));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onfocus = function() {
      logString = 'focus';
    };
    promise = window.onScreenKeyboard.focus();
    logString
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("focus", result);
  }
}

TEST_F(OnScreenKeyboardTest, FocusEventListeners) {
  std::string result;
  EXPECT_CALL(*(on_screen_keyboard_bridge()), FocusMock());
  const char script[] = R"(
    let logString1 = '(empty)';
    let logString2 = '(empty)';
    window.onScreenKeyboard.addEventListener('focus',
      function() {
        logString1 = 'focus1';
      });
    window.onScreenKeyboard.addEventListener('focus',
      function() {
        logString2 = 'focus2';
      });
    let promise = window.onScreenKeyboard.focus();
    logString1
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("focus1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ("focus2", result);
}

TEST_F(OnScreenKeyboardTest, BlurEventAttribute) {
  EXPECT_CALL(*(on_screen_keyboard_bridge()), BlurMock()).Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script, NULL));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onblur = function() {
      logString = 'blur';
    };
    promise = window.onScreenKeyboard.blur();
    logString
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("blur", result);
  }
}

TEST_F(OnScreenKeyboardTest, BlurEventListeners) {
  std::string result;
  EXPECT_CALL(*(on_screen_keyboard_bridge()), BlurMock());
  const char script[] = R"(
    let logString1 = '(empty)';
    let logString2 = '(empty)';
    window.onScreenKeyboard.addEventListener('blur',
      function() {
        logString1 = 'blur1';
      });
    window.onScreenKeyboard.addEventListener('blur',
      function() {
        logString2 = 'blur2';
      });
    let promise = window.onScreenKeyboard.blur();
    logString1
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("blur1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ(result, "blur2");
}

TEST_F(OnScreenKeyboardTest, BoundingRect) {
  std::string result;
  EXPECT_CALL(*(on_screen_keyboard_bridge()), BoundingRectMock())
      .WillOnce(testing::Return(nullptr));
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.boundingRect;", &result));
  EXPECT_EQ("null", result);
}

TEST_F(OnScreenKeyboardTest, KeepFocus) {
  std::string result;
  {
    InSequence seq;
    EXPECT_CALL(*(on_screen_keyboard_bridge()), SetKeepFocusMock(true));
    EXPECT_CALL(*(on_screen_keyboard_bridge()), SetKeepFocusMock(false));
    EXPECT_CALL(*(on_screen_keyboard_bridge()), SetKeepFocusMock(true));
  }

  // Check initialization.
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.keepFocus;", &result));
  EXPECT_EQ("false", result);

  const char script[] = R"(
    window.onScreenKeyboard.keepFocus = true;
    window.onScreenKeyboard.keepFocus = false;
    window.onScreenKeyboard.keepFocus = true;
  )";
  EXPECT_TRUE(EvaluateScript(script, NULL));
}

}  // namespace dom
}  // namespace cobalt
