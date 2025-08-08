// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace extensions {

class NetworkingPrivateEventRouter;

// This is a factory class used by the BrowserContextDependencyManager
// to instantiate the networking event router per profile (since the extension
// event router is per profile).
class NetworkingPrivateEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  NetworkingPrivateEventRouterFactory(
      const NetworkingPrivateEventRouterFactory&) = delete;
  NetworkingPrivateEventRouterFactory& operator=(
      const NetworkingPrivateEventRouterFactory&) = delete;

  // Returns the NetworkingPrivateEventRouter for |profile|, creating it if
  // it is not yet created.
  static NetworkingPrivateEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the NetworkingPrivateEventRouterFactory instance.
  static NetworkingPrivateEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend struct base::DefaultSingletonTraits<
      NetworkingPrivateEventRouterFactory>;

  NetworkingPrivateEventRouterFactory();
  ~NetworkingPrivateEventRouterFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_FACTORY_H_
