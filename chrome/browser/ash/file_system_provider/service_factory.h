// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace file_system_provider {

class Service;

// Creates services per profile.
class ServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns a service instance singleton, after creating it (if necessary).
  static Service* Get(content::BrowserContext* context);

  // Returns a service instance for the context if exists. Otherwise, returns
  // NULL.
  static Service* FindExisting(content::BrowserContext* context);

  // Gets a singleton instance of the factory.
  static ServiceFactory* GetInstance();

  ServiceFactory(const ServiceFactory&) = delete;
  ServiceFactory& operator=(const ServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ServiceFactory>;

  ServiceFactory();
  ~ServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_SERVICE_FACTORY_H_
