// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "content/public/browser/browser_context.h"

namespace ax {

// static
AccessibilityServiceRouter*
AccessibilityServiceRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccessibilityServiceRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccessibilityServiceRouterFactory*
AccessibilityServiceRouterFactory::GetInstance() {
  static base::NoDestructor<AccessibilityServiceRouterFactory> instance;
  return instance.get();
}

AccessibilityServiceRouterFactory::AccessibilityServiceRouterFactory()
    : ProfileKeyedServiceFactory(
          "AccessibilityService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

AccessibilityServiceRouterFactory::~AccessibilityServiceRouterFactory() =
    default;

KeyedService* AccessibilityServiceRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AccessibilityServiceRouter();
}

// static
void AccessibilityServiceRouterFactory::EnsureFactoryBuilt() {
  GetInstance();
}

}  // namespace ax
