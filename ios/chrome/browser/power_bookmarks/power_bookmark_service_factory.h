// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_SERVICE_FACTORY_H_

#import "base/memory/singleton.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace power_bookmarks {
class PowerBookmarkService;
}

// Factory to create one PowerBookmarkService per browser state.
class PowerBookmarkServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static power_bookmarks::PowerBookmarkService* GetForBrowserState(
      web::BrowserState* state);
  static PowerBookmarkServiceFactory* GetInstance();

  PowerBookmarkServiceFactory(const PowerBookmarkServiceFactory&) = delete;
  PowerBookmarkServiceFactory& operator=(const PowerBookmarkServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<PowerBookmarkServiceFactory>;

  PowerBookmarkServiceFactory();
  ~PowerBookmarkServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_POWER_BOOKMARKS_POWER_BOOKMARK_SERVICE_FACTORY_H_
