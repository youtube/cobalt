// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_placeholder_request_config.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(InfobarBannerPlaceholderRequestConfig);

InfobarBannerPlaceholderRequestConfig::InfobarBannerPlaceholderRequestConfig(
    infobars::InfoBar* infobar)
    : infobar_(infobar) {}

InfobarBannerPlaceholderRequestConfig::
    ~InfobarBannerPlaceholderRequestConfig() = default;

void InfobarBannerPlaceholderRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  PlaceholderRequestConfig::CreateForUserData(user_data);
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}
