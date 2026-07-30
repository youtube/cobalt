// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_CONTROLLER_BASE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_CONTROLLER_BASE_H_

#include <memory>

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AutofillPopupHideHelper;
class TouchToFillPaymentMethodDelegate;

// Base class for controllers of Touch To Fill surfaces. Provides common
// functionalities for showing and hiding the surface, and interacting with the
// WebContent.
class TouchToFillControllerBase {
 public:
  TouchToFillControllerBase(const TouchToFillControllerBase&) = delete;
  TouchToFillControllerBase& operator=(const TouchToFillControllerBase&) =
      delete;
  virtual ~TouchToFillControllerBase();

  // Hides the surface if it is currently shown.
  virtual void Hide() = 0;

  virtual content::WebContents* GetWebContents() = 0;

 protected:
  TouchToFillControllerBase();

  bool InitHideHelper(TouchToFillPaymentMethodDelegate& delegate);
  bool IsActiveWebContents();

  std::unique_ptr<AutofillPopupHideHelper> hide_helper_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_CONTROLLER_BASE_H_
