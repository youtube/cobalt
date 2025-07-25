// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace prefs {

const char kPrivacyBudgetGeneration[] = "privacy_budget.generation";
const char kPrivacyBudgetReportedReidBlocks[] =
    "privacy_budget.reported_reid_blocks";
const char kPrivacyBudgetSeenSurfaces[] = "privacy_budget.seen";
const char kPrivacyBudgetSelectedOffsets[] = "privacy_budget.selected";
const char kPrivacyBudgetSelectedBlock[] = "privacy_budget.block_offset";

void RegisterPrivacyBudgetPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kPrivacyBudgetGeneration, 0);
  registry->RegisterStringPref(kPrivacyBudgetReportedReidBlocks, std::string());
  registry->RegisterStringPref(kPrivacyBudgetSeenSurfaces, std::string());
  registry->RegisterStringPref(kPrivacyBudgetSelectedOffsets, std::string());
  registry->RegisterIntegerPref(kPrivacyBudgetSelectedBlock, -1);
}

}  // namespace prefs
