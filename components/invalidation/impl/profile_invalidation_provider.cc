// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/profile_invalidation_provider.h"

#include <utility>

#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider(
    std::unique_ptr<IdentityProvider> identity_provider,
    CustomSenderInvalidationServiceFactory
        custom_sender_invalidation_service_factory)
    : identity_provider_(std::move(identity_provider)),
      custom_sender_invalidation_service_factory_(
          std::move(custom_sender_invalidation_service_factory)) {}

ProfileInvalidationProvider::~ProfileInvalidationProvider() = default;

IdentityProvider* ProfileInvalidationProvider::GetIdentityProvider() {
  return identity_provider_.get();
}

InvalidationService*
ProfileInvalidationProvider::GetInvalidationServiceForCustomSender(
    const std::string& sender_id) {
  DCHECK(custom_sender_invalidation_service_factory_);

  std::unique_ptr<InvalidationService>& invalidation_service =
      custom_sender_invalidation_services_[sender_id];
  if (!invalidation_service) {
    invalidation_service =
        custom_sender_invalidation_service_factory_.Run(sender_id);
  }
  return invalidation_service.get();
}

void ProfileInvalidationProvider::Shutdown() {
  custom_sender_invalidation_services_.clear();
  custom_sender_invalidation_service_factory_.Reset();
}

// static
void ProfileInvalidationProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kInvalidationClientIDCache);
}

}  // namespace invalidation
