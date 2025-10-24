// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/language/model/language_model_manager_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/language/core/browser/language_model.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/language/core/common/language_experiments.h"
#import "components/language/core/language_model/fluent_language_model.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

void PrepareLanguageModels(ProfileIOS* const profile,
                           language::LanguageModelManager* const manager) {
  // Create and set the primary Language Model to use based on the state of
  // experiments. Note: there are currently no such experiments on iOS.
  manager->AddModel(
      language::LanguageModelManager::ModelType::FLUENT,
      std::make_unique<language::FluentLanguageModel>(profile->GetPrefs()));
  manager->SetPrimaryModel(language::LanguageModelManager::ModelType::FLUENT);
}

}  // namespace

// static
LanguageModelManagerFactory* LanguageModelManagerFactory::GetInstance() {
  static base::NoDestructor<LanguageModelManagerFactory> instance;
  return instance.get();
}

// static
language::LanguageModelManager* LanguageModelManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<language::LanguageModelManager>(
      profile, /*create=*/true);
}

// Use the original profile's language model even in Incognito mode.
LanguageModelManagerFactory::LanguageModelManagerFactory()
    : ProfileKeyedServiceFactoryIOS("LanguageModelManager",
                                    ProfileSelection::kRedirectedInIncognito) {}

LanguageModelManagerFactory::~LanguageModelManagerFactory() {}

std::unique_ptr<KeyedService>
LanguageModelManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* const state) const {
  ProfileIOS* const profile = ProfileIOS::FromBrowserState(state);
  std::unique_ptr<language::LanguageModelManager> manager =
      std::make_unique<language::LanguageModelManager>(
          profile->GetPrefs(), GetApplicationContext()->GetApplicationLocale());
  PrepareLanguageModels(profile, manager.get());
  return manager;
}
