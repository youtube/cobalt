// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_event_router_factory.h"

#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
SettingsPrivateEventRouter* SettingsPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<SettingsPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SettingsPrivateEventRouterFactory*
SettingsPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<SettingsPrivateEventRouterFactory>::get();
}

SettingsPrivateEventRouterFactory::SettingsPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "SettingsPrivateEventRouter",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(settings_private::GeneratedPrefsFactory::GetInstance());
  DependsOn(SettingsPrivateDelegateFactory::GetInstance());
}

SettingsPrivateEventRouterFactory::~SettingsPrivateEventRouterFactory() {
}

KeyedService* SettingsPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return SettingsPrivateEventRouter::Create(context);
}

bool SettingsPrivateEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SettingsPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
