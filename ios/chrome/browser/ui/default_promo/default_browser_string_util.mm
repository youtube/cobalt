// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"

#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#import <UIKit/UIKit.h>

using l10n_util::GetNSString;

NSString* GetDefaultBrowserPromoTitle() {
    return GetNSString(IDS_IOS_DEFAULT_BROWSER_TITLE_CTA_EXPERIMENT_SWITCH);
}

NSString* GetDefaultBrowserLearnMoreText() {
    return GetNSString(
        IDS_IOS_DEFAULT_BROWSER_LEARN_MORE_INSTRUCTIONS_MESSAGE_CTA_EXPERIMENT);
}
