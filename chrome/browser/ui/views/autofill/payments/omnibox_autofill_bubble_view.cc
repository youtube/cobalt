// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_bubble_view.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

OmniboxAutofillBubbleView::OmniboxAutofillBubbleView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    OmniboxAutofillBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);
}

OmniboxAutofillBubbleView::~OmniboxAutofillBubbleView() = default;

void OmniboxAutofillBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void OmniboxAutofillBubbleView::Hide() {
  CloseBubble();
  WindowClosing();
}

std::u16string OmniboxAutofillBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/490214497): Retrieve the window title via the controller.
  return std::u16string();
}

void OmniboxAutofillBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsUiClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void OmniboxAutofillBubbleView::Init() {
  // TODO(crbug.com/490214497): Add more UI properties once the payment method
  // suggestions are displayed.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

BEGIN_METADATA(OmniboxAutofillBubbleView)
END_METADATA

}  // namespace autofill
