// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class SafeBrowsingNavigationObserverManager;

// Singleton that owns SafeBrowsingNavigationObserverManager objects, one for
// each active Profile. It listens to profile destroy events and destroy its
// associated service. It returns a separate instance if the profile is in the
// Incognito mode.
class SafeBrowsingNavigationObserverManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given
  // |browser_context|. If the service already exists, return its pointer.
  static SafeBrowsingNavigationObserverManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Get the singleton instance.
  static SafeBrowsingNavigationObserverManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      SafeBrowsingNavigationObserverManagerFactory>;

  SafeBrowsingNavigationObserverManagerFactory();
  ~SafeBrowsingNavigationObserverManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_FACTORY_H_
