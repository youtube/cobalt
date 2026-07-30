// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

namespace autofill {

class OmniboxAutofillBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxAutofillBubbleViewBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOmniboxAutofill);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest, ShowBubble) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  auto* controller = OmniboxAutofillBubbleController::From(*tab);
  ASSERT_TRUE(controller);

  EXPECT_EQ(controller->GetBubbleView(), nullptr);

  controller->QueueOrShowBubble();

  EXPECT_NE(controller->GetBubbleView(), nullptr);
}

}  // namespace autofill
