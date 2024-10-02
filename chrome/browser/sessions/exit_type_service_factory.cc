// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/exit_type_service_factory.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

// static
ExitTypeService* ExitTypeServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ExitTypeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ExitTypeServiceFactory* ExitTypeServiceFactory::GetInstance() {
  static base::NoDestructor<ExitTypeServiceFactory> instance;
  return instance.get();
}

ExitTypeServiceFactory::ExitTypeServiceFactory()
    : ProfileKeyedServiceFactory("ExitTypeServiceFactory",
                                 ProfileSelections::BuildForRegularProfile()) {}

ExitTypeServiceFactory::~ExitTypeServiceFactory() = default;

KeyedService* ExitTypeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // TODO(sky): is this necessary?
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsSigninProfile(profile))
    return nullptr;
#endif
  return new ExitTypeService(profile);
}

bool ExitTypeServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // To ensure value is written to prefs during startup.
  return true;
}
