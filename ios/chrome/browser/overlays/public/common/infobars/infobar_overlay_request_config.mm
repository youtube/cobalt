// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/ui/badges/badge_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(InfobarOverlayRequestConfig);

InfobarOverlayRequestConfig::InfobarOverlayRequestConfig(
    InfoBarIOS* infobar,
    InfobarOverlayType overlay_type,
    bool is_high_priority)
    : infobar_(infobar->GetWeakPtr()),
      infobar_type_(infobar->infobar_type()),
      has_badge_(BadgeTypeForInfobarType(infobar_type_) != kBadgeTypeNone),
      is_high_priority_(is_high_priority),
      overlay_type_(overlay_type) {}

InfobarOverlayRequestConfig::~InfobarOverlayRequestConfig() = default;
