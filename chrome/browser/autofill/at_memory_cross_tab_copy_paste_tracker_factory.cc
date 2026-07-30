// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/at_memory_cross_tab_copy_paste_tracker_factory.h"

#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/at_memory_cross_tab_copy_paste_tracker.h"

namespace autofill {

// static
AtMemoryCrossTabCopyPasteTracker*
AtMemoryCrossTabCopyPasteTrackerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AtMemoryCrossTabCopyPasteTracker*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
AtMemoryCrossTabCopyPasteTrackerFactory*
AtMemoryCrossTabCopyPasteTrackerFactory::GetInstance() {
  static base::NoDestructor<AtMemoryCrossTabCopyPasteTrackerFactory> instance;
  return instance.get();
}

AtMemoryCrossTabCopyPasteTrackerFactory::
    AtMemoryCrossTabCopyPasteTrackerFactory()
    : ProfileKeyedServiceFactory(
          "AtMemoryCrossTabCopyPasteTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
}

AtMemoryCrossTabCopyPasteTrackerFactory::
    ~AtMemoryCrossTabCopyPasteTrackerFactory() = default;

std::unique_ptr<KeyedService>
AtMemoryCrossTabCopyPasteTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!autofill::IsAtMemoryFeatureEnabled(
          GoogleGroupsManagerFactory::GetForBrowserContext(context))) {
    return nullptr;
  }
  return std::make_unique<AtMemoryCrossTabCopyPasteTracker>();
}

}  // namespace autofill
