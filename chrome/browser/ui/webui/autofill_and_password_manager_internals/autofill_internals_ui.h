// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

class AutofillInternalsUI : public content::WebUIController {
 public:
  explicit AutofillInternalsUI(content::WebUI* web_ui);

  AutofillInternalsUI(const AutofillInternalsUI&) = delete;
  AutofillInternalsUI& operator=(const AutofillInternalsUI&) = delete;

  ~AutofillInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_H_
