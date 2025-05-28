// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace glic {

class GlicKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static GlicKeyedServiceFactory* GetInstance();

  static GlicKeyedService* GetGlicKeyedService(
      content::BrowserContext* browser_context,
      bool create = true);

  GlicKeyedServiceFactory(const GlicKeyedServiceFactory&) = delete;
  GlicKeyedServiceFactory& operator=(const GlicKeyedServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<GlicKeyedServiceFactory>;

  GlicKeyedServiceFactory();
  ~GlicKeyedServiceFactory() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_FACTORY_H_
