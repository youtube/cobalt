
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/origin_keyed_permission_action_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/permissions/origin_keyed_permission_action_service.h"

// static
permissions::OriginKeyedPermissionActionService*
OriginKeyedPermissionActionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::OriginKeyedPermissionActionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OriginKeyedPermissionActionServiceFactory*
OriginKeyedPermissionActionServiceFactory::GetInstance() {
  return base::Singleton<OriginKeyedPermissionActionServiceFactory>::get();
}

OriginKeyedPermissionActionServiceFactory::
    OriginKeyedPermissionActionServiceFactory()
    : ProfileKeyedServiceFactory(
          "OriginKeyedPermissionActionService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

OriginKeyedPermissionActionServiceFactory::
    ~OriginKeyedPermissionActionServiceFactory() = default;

KeyedService*
OriginKeyedPermissionActionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::OriginKeyedPermissionActionService();
}
