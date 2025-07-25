// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_BROWSERTEST_BASE_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"

namespace quick_answers {
class QuickAnswersBrowserTestBase : public InProcessBrowserTest {
 public:
  struct ShowMenuParams {
    std::string selected_text;
    int x = 0;
    int y = 0;
    bool is_password_field = false;
  };

  QuickAnswersBrowserTestBase();
  ~QuickAnswersBrowserTestBase() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

 protected:
  void ShowMenu(const ShowMenuParams& params);
  QuickAnswersController* controller() { return QuickAnswersController::Get(); }
};
}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_BROWSERTEST_BASE_H_
