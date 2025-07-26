// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class SearchPrefetchService;
class Profile;

// LazyInstance that owns all SearchPrefetchServices and associates them
// with Profiles.
class SearchPrefetchServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the SearchPrefetchService for the profile.
  //
  // Returns null if the features is not enabled.
  //
  // Note it also returns null for Incognito by default, but returns
  // SearchPrefetchService if the Incognito support is enabled via the feature
  // param. See crbug.com/394716358.
  static SearchPrefetchService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all SearchPrefetchService(s).
  static SearchPrefetchServiceFactory* GetInstance();

  SearchPrefetchServiceFactory(const SearchPrefetchServiceFactory&) = delete;
  SearchPrefetchServiceFactory& operator=(const SearchPrefetchServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<SearchPrefetchServiceFactory>;

  SearchPrefetchServiceFactory();
  ~SearchPrefetchServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_SERVICE_FACTORY_H_
