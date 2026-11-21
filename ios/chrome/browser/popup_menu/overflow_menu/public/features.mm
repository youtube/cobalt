// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/popup_menu/overflow_menu/public/features.h"

#import "ios/chrome/browser/shared/public/features/features.h"

BASE_FEATURE(kOverflowMenuNTPRefactor, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOverflowMenuNTPRefactorEnabled() {
  if (!IsChromeNextIaEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kOverflowMenuNTPRefactor);
}

BASE_FEATURE(kOverflowMenuHomeCustomizationEntrypoint,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOverflowMenuHomeCustomizationEntrypointEnabled() {
  if (!IsOverflowMenuNTPRefactorEnabled()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kOverflowMenuHomeCustomizationEntrypoint);
}
