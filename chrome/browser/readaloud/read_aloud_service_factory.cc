// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/read_aloud_service.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

// static
ReadAloudService* ReadAloudServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ReadAloudService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
ReadAloudServiceFactory* ReadAloudServiceFactory::GetInstance() {
  static base::NoDestructor<ReadAloudServiceFactory> instance;
  return instance.get();
}

ReadAloudServiceFactory::ReadAloudServiceFactory()
    : ProfileKeyedServiceFactory(
          "ReadAloudService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

ReadAloudServiceFactory::~ReadAloudServiceFactory() = default;

std::unique_ptr<KeyedService>
ReadAloudServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!features::IsReadAloudNativeEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return std::make_unique<ReadAloudService>(profile);
}

}  // namespace readaloud
