// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_TYPES_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

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

// Types of database signals that can be queried from the segmentation database.
enum class SignalType {
  kUserAction,
  kHistogramSum,
  kHistogramEnum,
};

// Simplified representation of a database signal to query from the segmentation
// database.
struct SignalDefinition {
  std::string name;
  SignalType type;
  int days;
  std::vector<int32_t> enum_values;  // Only used for kHistogramEnum
};

// Ergonomic helpers to create SignalDefinition structs.
SignalDefinition UserAction(const std::string& name, int days);
SignalDefinition HistogramSum(const std::string& name, int days);
SignalDefinition HistogramEnum(const std::string& name,
                               int days,
                               std::vector<int32_t> values);

// Ranking priority of a Tips feature to resolve multiple eligible candidate
// tips. Lower numerical values represent higher display priority.
enum class TipFeatureRank {
  kEnhancedSafeBrowsing = 0,
  kQuickDelete = 1,
  kGoogleLens = 2,
  kBottomOmnibox = 3,
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_TYPES_H_
