// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PIXEL_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PIXEL_TEST_H_

#include <string>
#include <tuple>
#include <type_traits>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"

namespace autofill {

namespace {
using ::testing::NiceMock;
using ::testing::Return;
}  // namespace

using TestParameterType = std::tuple<bool, bool>;

// A base class to do pixel tests for classes that derive from `PopupBaseView`.
// By default, the test class has two parameters: Dark vs light mode and RTL vs
// LTR for the text direction of the browser language.
template <typename View, typename Controller>
class PopupPixelTest : public UiBrowserTest,
                       public testing::WithParamInterface<TestParameterType> {
 public:
  static_assert(std::is_base_of_v<PopupBaseView, View>);
  static_assert(std::is_base_of_v<AutofillPopupViewDelegate, Controller>);

  PopupPixelTest() = default;
  ~PopupPixelTest() override = default;

  static bool IsDarkModeOn(const TestParameterType& param) {
    return std::get<0>(param);
  }
  static bool IsBrowserLanguageRTL(const TestParameterType& param) {
    return std::get<1>(param);
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<TestParameterType>& param_info) {
    return base::StrCat(
        {IsDarkModeOn(param_info.param) ? "Dark" : "Light",
         IsBrowserLanguageRTL(param_info.param) ? "BrowserRTL" : "BrowserLTR"});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsDarkModeOn(GetParam())) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();
    base::i18n::SetRTLForTesting(IsBrowserLanguageRTL(GetParam()));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ON_CALL(controller(), GetWebContents()).WillByDefault(Return(web_contents));
    ON_CALL(controller(), container_view())
        .WillByDefault(Return(web_contents->GetNativeView()));
  }

  void ShowUi(const std::string& name) override {
    view_ = new View(controller_.GetWeakPtr(),
                     views::Widget::GetWidgetForNativeWindow(
                         browser()->window()->GetNativeWindow()));
  }

  bool VerifyUi() override {
    if (!view_) {
      return false;
    }

    views::Widget* widget = view_->GetWidget();
    if (!widget) {
      return false;
    }

    // VerifyPixelUi works only for these platforms.
    // TODO(crbug.com/958242): Revise this if supported platforms change.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(widget, test_info->test_case_name(),
                         test_info->name());
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {}

  void TearDownOnMainThread() override {
    EXPECT_CALL(controller_, ViewDestroyed());
    view_ = nullptr;
    UiBrowserTest::TearDownOnMainThread();
  }

 protected:
  Controller& controller() { return controller_; }
  raw_ptr<View>& view() { return view_; }

 private:
  NiceMock<Controller> controller_;
  raw_ptr<View> view_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_POPUP_PIXEL_TEST_H_
