// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

VirtualCardEnrollIconView::VirtualCardEnrollIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater,
                         IDC_VIRTUAL_CARD_ENROLL,
                         icon_label_bubble_delegate,
                         delegate,
                         "VirtualCardEnroll") {
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_FALLBACK_ICON_TOOLTIP));
}

VirtualCardEnrollIconView::~VirtualCardEnrollIconView() = default;

views::BubbleDialogDelegate* VirtualCardEnrollIconView::GetBubble() const {
  VirtualCardEnrollBubbleController* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::VirtualCardEnrollBubbleViews*>(
      controller->GetVirtualCardEnrollBubbleView());
}

void VirtualCardEnrollIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  // |controller| may be nullptr due to lazy initialization.
  VirtualCardEnrollBubbleController* controller = GetController();
  bool command_enabled = controller && controller->IsIconVisible();
  SetVisible(SetCommandEnabled(command_enabled));
}

void VirtualCardEnrollIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& VirtualCardEnrollIconView::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? kCreditCardChromeRefreshIcon
             : kCreditCardIcon;
}

VirtualCardEnrollBubbleController* VirtualCardEnrollIconView::GetController()
    const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  return VirtualCardEnrollBubbleControllerImpl::FromWebContents(web_contents);
}

BEGIN_METADATA(VirtualCardEnrollIconView, PageActionIconView)
END_METADATA

}  // namespace autofill
