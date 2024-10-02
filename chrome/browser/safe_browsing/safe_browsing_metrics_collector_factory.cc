// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
SafeBrowsingMetricsCollector*
SafeBrowsingMetricsCollectorFactory::GetForProfile(Profile* profile) {
  return static_cast<SafeBrowsingMetricsCollector*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
SafeBrowsingMetricsCollectorFactory*
SafeBrowsingMetricsCollectorFactory::GetInstance() {
  return base::Singleton<SafeBrowsingMetricsCollectorFactory>::get();
}

SafeBrowsingMetricsCollectorFactory::SafeBrowsingMetricsCollectorFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingMetricsCollector",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

KeyedService* SafeBrowsingMetricsCollectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SafeBrowsingMetricsCollector(profile->GetPrefs());
}

}  // namespace safe_browsing
