// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_ADAPTIVE_QUIET_NOTIFICATION_PERMISSION_UI_ENABLER_H_
#define CHROME_BROWSER_PERMISSIONS_ADAPTIVE_QUIET_NOTIFICATION_PERMISSION_UI_ENABLER_H_

#include <memory>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_util.h"

class PrefChangeRegistrar;
class Profile;

// Keeps track of past user interactions with notification permission requests,
// and adaptively enables the quiet permission UX if various heuristics estimate
// the a posteriori probability of the user accepting the subsequent permission
// prompts to be low.
class AdaptiveQuietNotificationPermissionUiEnabler : public KeyedService {
 public:
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static AdaptiveQuietNotificationPermissionUiEnabler* GetForProfile(
        Profile* profile);

    static AdaptiveQuietNotificationPermissionUiEnabler::Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
  };

  static AdaptiveQuietNotificationPermissionUiEnabler* GetForProfile(
      Profile* profile);

  AdaptiveQuietNotificationPermissionUiEnabler() = delete;
  AdaptiveQuietNotificationPermissionUiEnabler(
      const AdaptiveQuietNotificationPermissionUiEnabler&) = delete;
  AdaptiveQuietNotificationPermissionUiEnabler& operator=(
      const AdaptiveQuietNotificationPermissionUiEnabler&) = delete;

  // Called after a notification permission prompt was resolved.
  void PermissionPromptResolved();

  // Only used for testing.
  void BackfillEnablingMethodIfMissingForTesting() {
    BackfillEnablingMethodIfMissing();
  }

 private:
  explicit AdaptiveQuietNotificationPermissionUiEnabler(Profile* profile);
  ~AdaptiveQuietNotificationPermissionUiEnabler() override;

  // Called when the quiet UI state is updated in preferences.
  void OnQuietUiStateChanged();

  // Retroactively backfills the enabling method, which was not populated
  // before M88.
  void BackfillEnablingMethodIfMissing();

  raw_ptr<Profile> profile_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  bool is_enabling_adaptively_ = false;
};

#endif  // CHROME_BROWSER_PERMISSIONS_ADAPTIVE_QUIET_NOTIFICATION_PERMISSION_UI_ENABLER_H_
