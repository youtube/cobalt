// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace permissions {
class PermissionActionsHistory;
}

class PermissionActionsHistoryFactory : public ProfileKeyedServiceFactory {
 public:
  PermissionActionsHistoryFactory(const PermissionActionsHistoryFactory&) =
      delete;
  PermissionActionsHistoryFactory& operator=(
      const PermissionActionsHistoryFactory&) = delete;
  static permissions::PermissionActionsHistory* GetForProfile(Profile* profile);
  static PermissionActionsHistoryFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PermissionActionsHistoryFactory>;

  PermissionActionsHistoryFactory();
  ~PermissionActionsHistoryFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_FACTORY_H_
