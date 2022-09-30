// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>

#include "cobalt/base/tokens.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/dom/window.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::cssom::ViewportSize;
using testing::InSequence;
using testing::Mock;

namespace cobalt {
namespace dom {

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

  bool SuggestionsSupported() const override {
    // TODO: implement and test this.
    SB_NOTIMPLEMENTED();
    return false;
  }

  void UpdateSuggestions(const script::Sequence<std::string>& suggestions,
                         int ticket) override {
    // TODO: implement and test this.
    SB_NOTIMPLEMENTED();
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

  void SetBackgroundColor(uint8 r, uint8 g, uint8 b) override {}

  void SetLightTheme(bool light_theme) override {}

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

class OnScreenKeyboardTest : public testing::TestWithJavaScript {
 public:
  OnScreenKeyboardTest()
      : on_screen_keyboard_bridge_(new OnScreenKeyboardMockBridge()) {
    stub_window()->set_on_screen_keyboard_bridge(
        on_screen_keyboard_bridge_.get());
    on_screen_keyboard_bridge_->window_ = window();
  }

  ~OnScreenKeyboardTest() {
    // TODO: figure out how to destruct OSK before global environment.
    window()->ReleaseOnScreenKeyboard();

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(on_screen_keyboard_bridge()));

    on_screen_keyboard_bridge_.reset();
  }

  OnScreenKeyboardMockBridge* on_screen_keyboard_bridge() const {
    return on_screen_keyboard_bridge_.get();
  }

 private:
  std::unique_ptr<OnScreenKeyboardMockBridge> on_screen_keyboard_bridge_;
};
}  // namespace

bool SkipLocale() {
  bool skipTests = !SbWindowOnScreenKeyboardIsSupported();
  if (skipTests) {
    SB_LOG(INFO) << "On screen keyboard not supported. Test skipped.";
  }
  return skipTests;
}

TEST_F(OnScreenKeyboardTest, ObjectExists) {
  if (SkipLocale()) return;

  std::string result;
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard;", &result));

  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("OnScreenKeyboard",
                                                             result));

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.show;", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function show()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.hide;", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function hide()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.focus;", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function focus()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.blur;", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function blur()",
                      result.c_str());

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.keepFocus;", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.data;", &result));
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onshow;", &result));
  EXPECT_STREQ("null", result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onhide;", &result));
  EXPECT_STREQ("null", result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onblur;", &result));
  EXPECT_STREQ("null", result.c_str());
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.onfocus;", &result));
  EXPECT_STREQ("null", result.c_str());
}

TEST_F(OnScreenKeyboardTest, ShowAndHide) {
  if (SkipLocale()) return;

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
  if (SkipLocale()) return;

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
  if (SkipLocale()) return;

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
  if (SkipLocale()) return;

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
  if (SkipLocale()) return;

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
  if (SkipLocale()) return;

  EXPECT_CALL(*(on_screen_keyboard_bridge()),
              ShowMock(window()->on_screen_keyboard()->data()))
      .Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onshow = function() {
      logString = 'show';
    };
    promise = window.onScreenKeyboard.show();
    logString;
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("show", result);
  }
}

TEST_F(OnScreenKeyboardTest, ShowEventListeners) {
  if (SkipLocale()) return;

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
    logString1;
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("show1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ(result, "show2");
}

TEST_F(OnScreenKeyboardTest, HideEventAttribute) {
  if (SkipLocale()) return;

  EXPECT_CALL(*(on_screen_keyboard_bridge()), HideMock()).Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onhide = function() {
      logString = 'hide';
    };
    promise = window.onScreenKeyboard.hide();
    logString;
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("hide", result);
  }
}

TEST_F(OnScreenKeyboardTest, HideEventListeners) {
  if (SkipLocale()) return;

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
    logString1;
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("hide1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ(result, "hide2");
}

TEST_F(OnScreenKeyboardTest, FocusEventAttribute) {
  if (SkipLocale()) return;

  EXPECT_CALL(*(on_screen_keyboard_bridge()), FocusMock()).Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onfocus = function() {
      logString = 'focus';
    };
    promise = window.onScreenKeyboard.focus();
    logString;
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("focus", result);
  }
}

TEST_F(OnScreenKeyboardTest, FocusEventListeners) {
  if (SkipLocale()) return;

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
    logString1;
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("focus1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ("focus2", result);
}

TEST_F(OnScreenKeyboardTest, BlurEventAttribute) {
  if (SkipLocale()) return;

  EXPECT_CALL(*(on_screen_keyboard_bridge()), BlurMock()).Times(3);
  const char let_script[] = R"(
    let promise;
    let logString;
  )";
  EXPECT_TRUE(EvaluateScript(let_script));
  const char event_script[] = R"(
    logString = '(empty)';
    window.onScreenKeyboard.onblur = function() {
      logString = 'blur';
    };
    promise = window.onScreenKeyboard.blur();
    logString;
  )";
  for (int i = 0; i < 3; ++i) {
    std::string result;
    EXPECT_TRUE(EvaluateScript(event_script, &result));
    EXPECT_EQ("blur", result);
  }
}

TEST_F(OnScreenKeyboardTest, BlurEventListeners) {
  if (SkipLocale()) return;

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
    logString1;
  )";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_EQ("blur1", result);
  EXPECT_TRUE(EvaluateScript("logString2", &result));
  EXPECT_EQ(result, "blur2");
}

TEST_F(OnScreenKeyboardTest, BoundingRect) {
  if (SkipLocale()) return;

  std::string result;
  EXPECT_CALL(*(on_screen_keyboard_bridge()), BoundingRectMock())
      .WillOnce(::testing::Return(nullptr));
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.boundingRect;", &result));
  EXPECT_EQ("null", result);
}

TEST_F(OnScreenKeyboardTest, KeepFocus) {
  if (SkipLocale()) return;

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
  EXPECT_TRUE(EvaluateScript(script));
}

TEST_F(OnScreenKeyboardTest, SetBackgroundColor) {
  if (SkipLocale()) return;

  std::string result;
  EXPECT_TRUE(
      EvaluateScript("window.onScreenKeyboard.backgroundColor;", &result));
  EXPECT_EQ("null", result);

  std::string color_str = "#0000FF";
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.backgroundColor = '" +
                                 color_str +
                                 "';"
                                 "window.onScreenKeyboard.backgroundColor",
                             &result));

  EXPECT_EQ(color_str, result);

  color_str = "rgb(0, 0, 100)";
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.backgroundColor = '" +
                                 color_str +
                                 "';"
                                 "window.onScreenKeyboard.backgroundColor",
                             &result));

  EXPECT_EQ(color_str, result);
}

TEST_F(OnScreenKeyboardTest, SetLightTheme) {
  if (SkipLocale()) return;

  std::string result;
  EXPECT_TRUE(EvaluateScript("window.onScreenKeyboard.lightTheme;", &result));
  EXPECT_EQ("null", result);

  std::string light_theme_str = "true";
  EXPECT_TRUE(
      EvaluateScript("window.onScreenKeyboard.lightTheme = " + light_theme_str +
                         ";"
                         "window.onScreenKeyboard.lightTheme",
                     &result));

  EXPECT_EQ(light_theme_str, result);
}

}  // namespace dom
}  // namespace cobalt
