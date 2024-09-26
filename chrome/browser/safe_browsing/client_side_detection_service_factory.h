// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace safe_browsing {
class ClientSideDetectionService;

// Singleton that owns ClientSideDetectionService objects, one for each active
// Profile. It listens to profile destroy events and destroy its associated
// service. It returns a separate instance if the profile is in Incognito
// mode.
class ClientSideDetectionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static ClientSideDetectionService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static ClientSideDetectionServiceFactory* GetInstance();

  ClientSideDetectionServiceFactory(const ClientSideDetectionServiceFactory&) =
      delete;
  ClientSideDetectionServiceFactory& operator=(
      const ClientSideDetectionServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ClientSideDetectionServiceFactory>;

  ClientSideDetectionServiceFactory();
  ~ClientSideDetectionServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
