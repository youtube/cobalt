// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/payments_churned_users_page_action_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

DEFINE_USER_DATA(PaymentsChurnedUsersPageActionController);

PaymentsChurnedUsersPageActionController::
    PaymentsChurnedUsersPageActionController(
        tabs::TabInterface& tab_interface,
        page_actions::PageActionController& page_action_controller)
    : tab_interface_(tab_interface),
      page_action_controller_(page_action_controller),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

PaymentsChurnedUsersPageActionController::
    ~PaymentsChurnedUsersPageActionController() = default;

// static
PaymentsChurnedUsersPageActionController*
PaymentsChurnedUsersPageActionController::From(tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void PaymentsChurnedUsersPageActionController::Show() {
  page_action_controller_->Show(kActionShowPaymentsChurnedUsersBubble);
  page_action_controller_->ShowSuggestionChip(
      kActionShowPaymentsChurnedUsersBubble);
}

void PaymentsChurnedUsersPageActionController::Hide() {
  page_action_controller_->HideSuggestionChip(
      kActionShowPaymentsChurnedUsersBubble);
  page_action_controller_->Hide(kActionShowPaymentsChurnedUsersBubble);
}

}  // namespace autofill
