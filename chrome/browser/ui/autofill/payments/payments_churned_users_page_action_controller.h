// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace autofill {

class PaymentsChurnedUsersPageActionController {
 public:
  DECLARE_USER_DATA(PaymentsChurnedUsersPageActionController);

  PaymentsChurnedUsersPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~PaymentsChurnedUsersPageActionController();

  PaymentsChurnedUsersPageActionController(
      const PaymentsChurnedUsersPageActionController&) = delete;
  PaymentsChurnedUsersPageActionController& operator=(
      const PaymentsChurnedUsersPageActionController&) = delete;

  static PaymentsChurnedUsersPageActionController* From(
      tabs::TabInterface& tab);

  // Shows the payments churned users page action icon.
  void Show();

  // Hides the payments churned users page action icon.
  void Hide();

 private:
  const raw_ref<tabs::TabInterface> tab_interface_;

  const raw_ref<page_actions::PageActionController> page_action_controller_;

  ui::ScopedUnownedUserData<PaymentsChurnedUsersPageActionController>
      scoped_unowned_user_data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_PAYMENTS_CHURNED_USERS_PAGE_ACTION_CONTROLLER_H_
