// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class OmniboxAutofillBubbleController;

class OmniboxAutofillBubbleView : public AutofillLocationBarBubble {
  METADATA_HEADER(OmniboxAutofillBubbleView, AutofillLocationBarBubble)

 public:
  OmniboxAutofillBubbleView(views::BubbleAnchor anchor_view,
                            content::WebContents* web_contents,
                            OmniboxAutofillBubbleController* controller);
  OmniboxAutofillBubbleView(const OmniboxAutofillBubbleView&) = delete;
  OmniboxAutofillBubbleView& operator=(const OmniboxAutofillBubbleView&) =
      delete;
  ~OmniboxAutofillBubbleView() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

 protected:
  // LocationBarBubbleDelegateView:
  void Init() override;

 private:
  base::WeakPtr<OmniboxAutofillBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_OMNIBOX_AUTOFILL_BUBBLE_VIEW_H_
