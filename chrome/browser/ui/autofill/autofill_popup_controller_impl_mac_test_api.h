// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_MAC_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_MAC_TEST_API_H_

#import "base/memory/raw_ptr.h"
#import "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"
#import "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"

@class WebTextfieldTouchBarController;

namespace autofill {

class AutofillPopupControllerImplMacTestApi
    : public AutofillPopupControllerImplTestApi {
 public:
  explicit AutofillPopupControllerImplMacTestApi(
      AutofillPopupControllerImplMac* controller)
      : AutofillPopupControllerImplTestApi(controller),
        controller_(controller) {}

  void set_touch_bar_controller(WebTextfieldTouchBarController* controller) {
    controller_->touch_bar_controller_ = controller;
  }

  WebTextfieldTouchBarController* touch_bar_controller() {
    return controller_->touch_bar_controller_;
  }

 private:
  raw_ptr<AutofillPopupControllerImplMac> controller_;
};

inline AutofillPopupControllerImplMacTestApi test_api(
    AutofillPopupControllerImplMac& controller) {
  return AutofillPopupControllerImplMacTestApi(&controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_MAC_TEST_API_H_
