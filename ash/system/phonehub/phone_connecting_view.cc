// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_connecting_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_interstitial_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

using phone_hub_metrics::InterstitialScreenEvent;
using phone_hub_metrics::Screen;

PhoneConnectingView::PhoneConnectingView() {
  SetID(PhoneHubViewID::kPhoneConnectingView);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  content_view_ = AddChildView(
      std::make_unique<PhoneHubInterstitialView>(/*show_progress=*/true));
  content_view_->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_PHONE_HUB_CONNECTING_IMAGE));
  content_view_->SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_PHONE_CONNECTING_DIALOG_TITLE));
  content_view_->SetDescription(l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_PHONE_CONNECTING_DIALOG_DESCRIPTION));

  LogInterstitialScreenEvent(InterstitialScreenEvent::kShown);
}

PhoneConnectingView::~PhoneConnectingView() = default;

phone_hub_metrics::Screen PhoneConnectingView::GetScreenForMetrics() const {
  return Screen::kPhoneConnecting;
}

BEGIN_METADATA(PhoneConnectingView, views::View)
END_METADATA

}  // namespace ash
