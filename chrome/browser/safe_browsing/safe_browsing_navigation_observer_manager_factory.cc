// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

// static
SafeBrowsingNavigationObserverManager*
SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SafeBrowsingNavigationObserverManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SafeBrowsingNavigationObserverManagerFactory*
SafeBrowsingNavigationObserverManagerFactory::GetInstance() {
  return base::Singleton<SafeBrowsingNavigationObserverManagerFactory>::get();
}

SafeBrowsingNavigationObserverManagerFactory::
    SafeBrowsingNavigationObserverManagerFactory()
    : ProfileKeyedServiceFactory(
          "SafeBrowsingNavigationObserverManager",
          ProfileSelections::BuildForRegularAndIncognito()) {}

KeyedService*
SafeBrowsingNavigationObserverManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SafeBrowsingNavigationObserverManager(profile->GetPrefs());
}

}  // namespace safe_browsing
