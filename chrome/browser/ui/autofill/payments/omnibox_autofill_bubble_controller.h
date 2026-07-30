// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace autofill {

// Controller class that exposes functionality to omnibox autofill bubbles.
// Owned by TabFeatures.
class OmniboxAutofillBubbleController : public AutofillBubbleControllerBase {
 public:
  DECLARE_USER_DATA(OmniboxAutofillBubbleController);

  explicit OmniboxAutofillBubbleController(tabs::TabInterface& tab_interface,
                                           content::WebContents* web_contents);
  OmniboxAutofillBubbleController(const OmniboxAutofillBubbleController&) =
      delete;
  OmniboxAutofillBubbleController& operator=(
      const OmniboxAutofillBubbleController&) = delete;
  ~OmniboxAutofillBubbleController() override;

  static OmniboxAutofillBubbleController* From(
      tabs::TabInterface& tab_interface);

  // BubbleControllerBase:
  void OnBubbleDiscarded() override {}
  BubbleType GetBubbleType() const override;
  base::WeakPtr<BubbleControllerBase> GetBubbleControllerBaseWeakPtr() override;

 protected:
  void DoShowBubble() override;

 private:
  ui::ScopedUnownedUserData<OmniboxAutofillBubbleController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<OmniboxAutofillBubbleController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_CONTROLLER_H_
