// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_LARGE_ICON_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FAVICON_LARGE_ICON_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace favicon {
class LargeIconService;
}

// Singleton that owns all LargeIconService and associates them with
// BrowserContext instances.
class LargeIconServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static favicon::LargeIconService* GetForBrowserContext(
      content::BrowserContext* context);

  static LargeIconServiceFactory* GetInstance();

  LargeIconServiceFactory(const LargeIconServiceFactory&) = delete;
  LargeIconServiceFactory& operator=(const LargeIconServiceFactory&) = delete;

  // Returns the icon size requested from server. The returned value takes into
  // account the state of `features::kLargeFaviconFromGoogle`.
  static int desired_size_in_dip_for_server_requests();

 private:
  friend struct base::DefaultSingletonTraits<LargeIconServiceFactory>;

  LargeIconServiceFactory();
  ~LargeIconServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_FAVICON_LARGE_ICON_SERVICE_FACTORY_H_
