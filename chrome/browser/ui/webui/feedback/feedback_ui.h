// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "ui/web_dialogs/web_dialog_ui.h"

// The implementation for the chrome://feedback page.
class FeedbackUI : public ui::WebDialogUI {
 public:
  explicit FeedbackUI(content::WebUI* web_ui);
  FeedbackUI(const FeedbackUI&) = delete;
  FeedbackUI& operator=(const FeedbackUI&) = delete;
  ~FeedbackUI() override;

  static bool IsFeedbackEnabled(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_FEEDBACK_UI_H_
