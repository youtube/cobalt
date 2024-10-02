// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reduce_accept_language/reduce_accept_language_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"

// static
reduce_accept_language::ReduceAcceptLanguageService*
ReduceAcceptLanguageFactory::GetForProfile(Profile* profile) {
  return static_cast<reduce_accept_language::ReduceAcceptLanguageService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ReduceAcceptLanguageFactory* ReduceAcceptLanguageFactory::GetInstance() {
  static base::NoDestructor<ReduceAcceptLanguageFactory> instance;
  return instance.get();
}

ReduceAcceptLanguageFactory::ReduceAcceptLanguageFactory()
    : ProfileKeyedServiceFactory(
          "ReduceAcceptLanguage",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

ReduceAcceptLanguageFactory::~ReduceAcceptLanguageFactory() = default;

KeyedService* ReduceAcceptLanguageFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();
  return new reduce_accept_language::ReduceAcceptLanguageService(
      HostContentSettingsMapFactory::GetForProfile(context), prefs,
      profile->IsIncognitoProfile());
}
