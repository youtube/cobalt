// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {
namespace printing {
namespace oauth2 {

// static
AuthorizationZonesManagerFactory*
AuthorizationZonesManagerFactory::GetInstance() {
  static base::NoDestructor<AuthorizationZonesManagerFactory> factory;
  return factory.get();
}

// static
AuthorizationZonesManager*
AuthorizationZonesManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AuthorizationZonesManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AuthorizationZonesManagerFactory::AuthorizationZonesManagerFactory()
    : ProfileKeyedServiceFactory(
          "AuthorizationZonesManagerFactory",
          ProfileSelections::BuildRedirectedInIncognito()) {}

AuthorizationZonesManagerFactory::~AuthorizationZonesManagerFactory() = default;

KeyedService* AuthorizationZonesManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return AuthorizationZonesManager::Create(Profile::FromBrowserContext(context))
      .release();
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
