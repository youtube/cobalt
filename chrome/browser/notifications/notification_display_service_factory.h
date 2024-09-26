// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NotificationDisplayService;
class Profile;

class NotificationDisplayServiceFactory : public ProfileKeyedServiceFactory {
 public:
  NotificationDisplayServiceFactory(const NotificationDisplayServiceFactory&) =
      delete;
  NotificationDisplayServiceFactory& operator=(
      const NotificationDisplayServiceFactory&) = delete;
  static NotificationDisplayService* GetForProfile(Profile* profile);
  static NotificationDisplayServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NotificationDisplayServiceFactory>;

  NotificationDisplayServiceFactory();

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_FACTORY_H_
