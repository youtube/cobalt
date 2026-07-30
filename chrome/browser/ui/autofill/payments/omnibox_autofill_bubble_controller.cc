// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"

#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace autofill {

DEFINE_USER_DATA(OmniboxAutofillBubbleController);

OmniboxAutofillBubbleController::OmniboxAutofillBubbleController(
    tabs::TabInterface& tab_interface,
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
}

OmniboxAutofillBubbleController::~OmniboxAutofillBubbleController() = default;

// static
OmniboxAutofillBubbleController* OmniboxAutofillBubbleController::From(
    tabs::TabInterface& tab_interface) {
  return Get(tab_interface.GetUnownedUserDataHost());
}

BubbleType OmniboxAutofillBubbleController::GetBubbleType() const {
  return BubbleType::kOmniboxAutofill;
}

base::WeakPtr<BubbleControllerBase>
OmniboxAutofillBubbleController::GetBubbleControllerBaseWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OmniboxAutofillBubbleController::DoShowBubble() {
  // TODO(crbug.com/490214497): Display payment method suggestion list.
}

}  // namespace autofill
