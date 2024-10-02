// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context.h"

UsbChooserContextFactory::UsbChooserContextFactory()
    : ProfileKeyedServiceFactory(
          "UsbChooserContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UsbChooserContextFactory::~UsbChooserContextFactory() {}

KeyedService* UsbChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new UsbChooserContext(Profile::FromBrowserContext(context));
}

// static
UsbChooserContextFactory* UsbChooserContextFactory::GetInstance() {
  return base::Singleton<UsbChooserContextFactory>::get();
}

// static
UsbChooserContext* UsbChooserContextFactory::GetForProfile(Profile* profile) {
  return static_cast<UsbChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

UsbChooserContext* UsbChooserContextFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<UsbChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

void UsbChooserContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* usb_chooser_context =
      GetForProfileIfExists(Profile::FromBrowserContext(context));
  if (usb_chooser_context)
    usb_chooser_context->FlushScheduledSaveSettingsCalls();
}
