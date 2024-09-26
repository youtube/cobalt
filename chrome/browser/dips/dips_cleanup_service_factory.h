// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_CLEANUP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DIPS_DIPS_CLEANUP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class DIPSCleanupService;

class DIPSCleanupServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static DIPSCleanupServiceFactory* GetInstance();
  static DIPSCleanupService* GetForBrowserContext(
      content::BrowserContext* context);

  static ProfileSelections CreateProfileSelections();

 private:
  friend struct base::DefaultSingletonTraits<DIPSCleanupServiceFactory>;

  DIPSCleanupServiceFactory();
  ~DIPSCleanupServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_CLEANUP_SERVICE_FACTORY_H_
