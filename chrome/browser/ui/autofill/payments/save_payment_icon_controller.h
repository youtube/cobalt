// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_CONTROLLER_H_

#include <string>

#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillBubbleBase;

// The controller for SavePaymentIconView.
class SavePaymentIconController {
 public:
  // The type of the autofill bubble attached to the icon.
  enum class PaymentBubbleType {
    kUnknown = 0,
    // Bubble for credit card.
    kCreditCard = 1,
    // Bubble for save IBAN.
    kSaveIban = 2,
    // Bubble for manage saved IBAN
    kManageSavedIban = 3,
    kMaxValue = kManageSavedIban
  };

  virtual ~SavePaymentIconController() = default;

  // Returns a reference to the SavePaymentIconController associated with the
  // given `web_contents` and `command_id`. If controller does not exist, this
  // returns nullptr.
  static SavePaymentIconController* Get(content::WebContents* web_contents,
                                        int command_id);

  // Once the animation ends, it shows a new bubble if needed.
  virtual void OnAnimationEnded() = 0;

  // Returns true iff upload save failed and the failure badge on the icon
  // should be shown.
  virtual bool ShouldShowSaveFailureBadge() const = 0;

  // Returns true iff the payment saved animation should be shown.
  virtual bool ShouldShowPaymentSavedLabelAnimation() const = 0;

  // Returns true iff upload save is in progress and the saving animation should
  // be shown.
  virtual bool ShouldShowSavingPaymentAnimation() const = 0;

  // Returns true iff the payment icon is visible.
  virtual bool IsIconVisible() const = 0;

  // Returns the currently active save payment or manage saved payment bubble
  // view. Can be nullptr if no bubble is visible.
  virtual AutofillBubbleBase* GetPaymentBubbleView() const = 0;

  virtual PaymentBubbleType GetPaymentBubbleType() const = 0;

  // Returns the tooltip message for the save payment icon.
  virtual std::u16string GetSavePaymentIconTooltipText() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_SAVE_PAYMENT_ICON_CONTROLLER_H_
