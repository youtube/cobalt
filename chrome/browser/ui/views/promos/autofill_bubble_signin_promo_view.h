// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_AUTOFILL_BUBBLE_SIGNIN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_AUTOFILL_BUBBLE_SIGNIN_PROMO_VIEW_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"
#include "chrome/browser/ui/views/promos/bubble_signin_promo_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace signin_metrics {
enum class AccessPoint;
}
namespace content {
class WebContents;
}
namespace autofill {
class AutofillBubbleSignInPromoController;
}
class AutofillBubbleSignInPromoView;

// A view that can show up after saving a piece of autofill data without being
// signed in to offer signing users in so they can access their credentials
// across devices.
class AutofillBubbleSignInPromoView : public views::View,
                                      public views::WidgetObserver {
  METADATA_HEADER(AutofillBubbleSignInPromoView, views::View)

 public:
  explicit AutofillBubbleSignInPromoView(
      content::WebContents* web_contents,
      signin_metrics::AccessPoint access_point,
      syncer::LocalDataItemModel::DataId data_id);
  AutofillBubbleSignInPromoView(const AutofillBubbleSignInPromoView&) = delete;
  AutofillBubbleSignInPromoView& operator=(
      const AutofillBubbleSignInPromoView&) = delete;
  ~AutofillBubbleSignInPromoView() override;

  View* GetSignInButton() const;

 private:
  // Delegate for the personalized sign in promo view used when desktop identity
  // consistency is enabled.
  class DiceSigninPromoDelegate;

  // views::WidgetObserver:
  // Records that the bubble has been dismissed.
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::View:
  void AddedToWidget() override;

  autofill::AutofillBubbleSignInPromoController controller_;
  const signin_metrics::AccessPoint access_point_;
  std::unique_ptr<DiceSigninPromoDelegate> dice_sign_in_promo_delegate_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};
  raw_ptr<BubbleSignInPromoView> bubble_sign_in_promo_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_AUTOFILL_BUBBLE_SIGNIN_PROMO_VIEW_H_
