// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_FACTORY_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

class RecentModel;

class RecentModelFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the RecentModel for |profile|, creating it if not created yet.
  static RecentModel* GetForProfile(Profile* profile);

  // Returns the singleton RecentModelFactory instance.
  static RecentModelFactory* GetInstance();

  RecentModelFactory(const RecentModelFactory&) = delete;
  RecentModelFactory& operator=(const RecentModelFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<RecentModelFactory>;

  RecentModelFactory();
  ~RecentModelFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_MODEL_FACTORY_H_
