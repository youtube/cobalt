// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_DRIVE_DRIVE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_DRIVE_DRIVE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DriveService;
class Profile;

class DriveServiceFactory : ProfileKeyedServiceFactory {
 public:
  static DriveService* GetForProfile(Profile* profile);
  static DriveServiceFactory* GetInstance();
  DriveServiceFactory(const DriveServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<DriveServiceFactory>;
  DriveServiceFactory();
  ~DriveServiceFactory() override;

  // Uses BrowserContextKeyedServiceFactory to build a DriveService.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_DRIVE_DRIVE_SERVICE_FACTORY_H_
