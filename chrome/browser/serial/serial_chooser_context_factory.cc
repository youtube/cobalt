// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"

SerialChooserContextFactory::SerialChooserContextFactory()
    : ProfileKeyedServiceFactory(
          "SerialChooserContext",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

SerialChooserContextFactory::~SerialChooserContextFactory() {}

KeyedService* SerialChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SerialChooserContext(Profile::FromBrowserContext(context));
}

// static
SerialChooserContextFactory* SerialChooserContextFactory::GetInstance() {
  return base::Singleton<SerialChooserContextFactory>::get();
}

// static
SerialChooserContext* SerialChooserContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SerialChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SerialChooserContext* SerialChooserContextFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SerialChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

void SerialChooserContextFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  auto* serial_chooser_context =
      GetForProfileIfExists(Profile::FromBrowserContext(context));
  if (serial_chooser_context)
    serial_chooser_context->FlushScheduledSaveSettingsCalls();
}
