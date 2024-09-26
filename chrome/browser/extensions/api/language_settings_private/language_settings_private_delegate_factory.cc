// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
LanguageSettingsPrivateDelegate*
LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LanguageSettingsPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
LanguageSettingsPrivateDelegateFactory*
LanguageSettingsPrivateDelegateFactory::GetInstance() {
  return base::Singleton<LanguageSettingsPrivateDelegateFactory>::get();
}

LanguageSettingsPrivateDelegateFactory::LanguageSettingsPrivateDelegateFactory()
    : ProfileKeyedServiceFactory(
          "LanguageSettingsPrivateDelegate",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(SpellcheckServiceFactory::GetInstance());
}

LanguageSettingsPrivateDelegateFactory::
    ~LanguageSettingsPrivateDelegateFactory() {
}

KeyedService* LanguageSettingsPrivateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return LanguageSettingsPrivateDelegate::Create(context);
}

bool LanguageSettingsPrivateDelegateFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
