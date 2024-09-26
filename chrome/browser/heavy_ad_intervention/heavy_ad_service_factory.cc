// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "content/public/browser/browser_context.h"

namespace {

base::LazyInstance<HeavyAdServiceFactory>::DestructorAtExit g_heavy_ad_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
heavy_ad_intervention::HeavyAdService*
HeavyAdServiceFactory::GetForBrowserContext(content::BrowserContext* context) {
  return static_cast<heavy_ad_intervention::HeavyAdService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HeavyAdServiceFactory* HeavyAdServiceFactory::GetInstance() {
  return g_heavy_ad_factory.Pointer();
}

HeavyAdServiceFactory::HeavyAdServiceFactory()
    : ProfileKeyedServiceFactory(
          "HeavyAdService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

HeavyAdServiceFactory::~HeavyAdServiceFactory() {}

KeyedService* HeavyAdServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new heavy_ad_intervention::HeavyAdService();
}
