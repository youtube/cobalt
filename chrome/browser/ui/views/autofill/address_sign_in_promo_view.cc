// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_sign_in_promo_view.h"

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AddressSignInPromoView::AddressSignInPromoView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    const AutofillProfile& autofill_profile)
    : AddressBubbleBaseView(anchor_view, web_contents) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(IDS_AUTOFILL_SIGNIN_PROMO_TITLE_ADDRESS);
  SetShowCloseButton(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  // Show the sign in promo.
  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kAddressBubble,
      syncer::LocalDataItemModel::DataId(autofill_profile.guid())));

  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}

AddressSignInPromoView::~AddressSignInPromoView() = default;

void AddressSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kBubbleFrameViewId);
  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS),
          ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS_DARK),
          base::BindRepeating(&views::BubbleDialogDelegate::background_color,
                              base::Unretained(this))));
}

void AddressSignInPromoView::Hide() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void AddressSignInPromoView::WindowClosing() {
  if (!web_contents()) {
    return;
  }

  AddressBubblesController::FromWebContents(web_contents())->OnBubbleClosed();
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AddressSignInPromoView,
                                      kBubbleFrameViewId);

}  // namespace autofill
