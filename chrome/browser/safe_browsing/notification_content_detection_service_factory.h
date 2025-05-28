// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_CONTENT_DETECTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_CONTENT_DETECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace safe_browsing {
class NotificationContentDetectionService;

// Singleton that owns NotificationContentDetectionService objects, one for each
// active Profile. It listens to profile destroy events and destroy its
// associated service. It returns nullptr if the profile is in the Incognito
// mode.
class NotificationContentDetectionServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given |profile|.
  // If the service already exists, return its pointer.
  static NotificationContentDetectionService* GetForProfile(Profile* profile);

  // Get the singleton instance.
  static NotificationContentDetectionServiceFactory* GetInstance();

  NotificationContentDetectionServiceFactory(
      const NotificationContentDetectionServiceFactory&) = delete;
  NotificationContentDetectionServiceFactory& operator=(
      const NotificationContentDetectionServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NotificationContentDetectionServiceFactory>;

  NotificationContentDetectionServiceFactory();
  ~NotificationContentDetectionServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_CONTENT_DETECTION_SERVICE_FACTORY_H_
