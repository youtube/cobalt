// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_MAC_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_MAC_H_

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

@class WebTextfieldTouchBarController;

namespace autofill {

class AutofillPopupControllerImplMac : public AutofillPopupControllerImpl {
 public:
  AutofillPopupControllerImplMac(base::WeakPtr<AutofillPopupDelegate> delegate,
                                 content::WebContents* web_contents,
                                 gfx::NativeView container_view,
                                 const gfx::RectF& element_bounds,
                                 base::i18n::TextDirection text_direction);

  AutofillPopupControllerImplMac(const AutofillPopupControllerImplMac&) =
      delete;
  AutofillPopupControllerImplMac& operator=(
      const AutofillPopupControllerImplMac&) = delete;

  ~AutofillPopupControllerImplMac() override;

  // Shows the popup, or updates the existing popup with the given values.
  // If the popup contains credit card items, find and set
  // |touchBarController_| and show the credit card autofill touch bar.
  void Show(std::vector<autofill::Suggestion> suggestions,
            AutoselectFirstSuggestion autoselect_first_suggestion) override;

  // Updates the data list values currently shown with the popup. Calls
  // -invalidateTouchBar from |touchBarController_|.
  void UpdateDataListValues(const std::vector<std::u16string>& values,
                            const std::vector<std::u16string>& labels) override;

 protected:
  // Hides the popup and destroys the controller. This also invalidates
  // |delegate_|.
  void HideViewAndDie() override;

 private:
  // The controller providing the autofill touch bar.
  WebTextfieldTouchBarController* touch_bar_controller_;  // weak.

  // True if the popup contains credit card items.
  BOOL is_credit_card_popup_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_MAC_H_
