// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NotifierStateTracker;
class Profile;

class NotifierStateTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static NotifierStateTracker* GetForProfile(Profile* profile);
  static NotifierStateTrackerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NotifierStateTrackerFactory>;

  NotifierStateTrackerFactory();
  NotifierStateTrackerFactory(const NotifierStateTrackerFactory&) = delete;
  NotifierStateTrackerFactory& operator=(const NotifierStateTrackerFactory&) =
      delete;
  ~NotifierStateTrackerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFIER_STATE_TRACKER_FACTORY_H_
