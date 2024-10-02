// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ContentIndexProviderImpl;
class Profile;

class ContentIndexProviderFactory : public ProfileKeyedServiceFactory {
 public:
  static ContentIndexProviderImpl* GetForProfile(Profile* profile);
  static ContentIndexProviderFactory* GetInstance();

  ContentIndexProviderFactory(const ContentIndexProviderFactory&) = delete;
  ContentIndexProviderFactory& operator=(const ContentIndexProviderFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<ContentIndexProviderFactory>;

  ContentIndexProviderFactory();
  ~ContentIndexProviderFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_FACTORY_H_
