// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_TYPES_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_TYPES_H_

namespace tips {

// The Chrome feature correlating to each tip notification.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.tips)
enum class TipsNotificationsFeatureType {
  kEnhancedSafeBrowsing = 0,
  kQuickDelete = 1,
  kGoogleLens = 2,
  kBottomOmnibox = 3,
  kPasswordAutofill = 4,
  kSignin = 5,
  kCreateTabGroups = 6,
  kCustomizeMVT = 7,
  kRecentTabs = 8,
  kMaxValue = kRecentTabs
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_TYPES_H_
