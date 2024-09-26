// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BitmapFetcherService;

class BitmapFetcherServiceFactory : ProfileKeyedServiceFactory {
 public:
  // TODO(groby): Maybe make this GetForProfile?
  static BitmapFetcherService* GetForBrowserContext(
      content::BrowserContext* context);
  static BitmapFetcherServiceFactory* GetInstance();

  BitmapFetcherServiceFactory(const BitmapFetcherServiceFactory&) = delete;
  BitmapFetcherServiceFactory& operator=(const BitmapFetcherServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<BitmapFetcherServiceFactory>;

  BitmapFetcherServiceFactory();
  ~BitmapFetcherServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_FACTORY_H_
