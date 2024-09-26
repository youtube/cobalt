// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/features.h"

#import "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kBringYourOwnTabsIOS,
             "BringYourOwnTabsIOS",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kBringYourOwnTabsIOSParam[] = "bottom-message";

BringYourOwnTabsPromptType GetBringYourOwnTabsPromptType() {
  if (base::FeatureList::IsEnabled(kBringYourOwnTabsIOS)) {
    bool showBottomMessagePrompt = base::GetFieldTrialParamByFeatureAsBool(
        kBringYourOwnTabsIOS, kBringYourOwnTabsIOSParam, false);
    return showBottomMessagePrompt ? BringYourOwnTabsPromptType::kBottomMessage
                                   : BringYourOwnTabsPromptType::kHalfSheet;
  }
  return BringYourOwnTabsPromptType::kDisabled;
}
