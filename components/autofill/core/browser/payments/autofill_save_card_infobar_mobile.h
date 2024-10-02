// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_MOBILE_H_

#include <memory>

#include "components/signin/public/identity_manager/account_info.h"

namespace infobars {
class InfoBar;
}

namespace autofill {

class AutofillSaveCardInfoBarDelegateMobile;

// Creates an Infobar for saving a credit card on a mobile device.
std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_SAVE_CARD_INFOBAR_MOBILE_H_
