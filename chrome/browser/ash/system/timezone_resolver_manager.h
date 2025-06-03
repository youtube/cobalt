// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_

#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace ash {
namespace system {

class TimeZoneResolverManager : public TimeZoneResolver::Delegate {
 public:
  class Observer {
   public:
    // This is always called when UpdateTimezoneResolver() is finished.
    // As UpdateTimezoneResolver() is called once any of the relevant
    // preferences are updated, it can be used to observe all time zone -related
    // preference changes.
    virtual void OnTimeZoneResolverUpdated() = 0;
  };

  // This is stored as a prefs::kResolveTimezoneByGeolocationMethod
  // and prefs::kResolveDeviceTimezoneByGeolocationMethod preferences.
  enum class TimeZoneResolveMethod {
    DISABLED = 0,
    IP_ONLY = 1,
    SEND_WIFI_ACCESS_POINTS = 2,
    SEND_ALL_LOCATION_INFO = 3,
    METHODS_NUMBER = 4
  };

  TimeZoneResolverManager();

  TimeZoneResolverManager(const TimeZoneResolverManager&) = delete;
  TimeZoneResolverManager& operator=(const TimeZoneResolverManager&) = delete;

  ~TimeZoneResolverManager() override;

  // This sets primary_user_prefs_.
  void SetPrimaryUserPrefs(PrefService* pref_service);

  // TimeZoneResolver::Delegate:
  bool ShouldSendWiFiGeolocationData() const override;
  bool ShouldSendCellularGeolocationData() const override;
  bool IsSystemGeolocationAllowed() const override;

  // Starts or stops TimezoneResolver according to current settings.
  void UpdateTimezoneResolver();

  // This class should respect the system geolocation permission. When the
  // permission is disabled, no requests should be dispatched and no responses
  // processed.
  // Called from `ash::Preferences::ApplyPreferences()`.
  void OnSystemGeolocationPermissionChanged(bool enabled);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if result of timezone resolve should be applied to
  // system timezone (preferences might have changed since request was started).
  bool ShouldApplyResolvedTimezone();

  // Returns true if TimeZoneResolver should be running and taking in account
  // all configuration data.
  bool TimeZoneResolverShouldBeRunning();

  // Convert kResolveTimezoneByGeolocationMethod /
  // kResolveDeviceTimezoneByGeolocationMethod preference value to
  // TimeZoneResolveMethod. Defaults to DISABLED for unknown values.
  static TimeZoneResolveMethod TimeZoneResolveMethodFromInt(int value);

  // Returns user preference value if time zone is not managed.
  // Otherwise returns effective time zone resolve method.
  // If |check_policy| is true, effective method calculation will also
  // take into account current policy values.
  static TimeZoneResolveMethod GetEffectiveUserTimeZoneResolveMethod(
      const PrefService* user_prefs,
      bool check_policy);

  // Returns true if time zone resolution settings are policy controlled and
  // thus cannot be changed by user.
  static bool IsTimeZoneResolutionPolicyControlled();

  // Returns true if service should be running for the signin screen.
  static bool IfServiceShouldBeRunningForSigninScreen();

 private:
  // Return the effective policy value for automatic time zone resolution.
  // If static timezone policy is present returns
  // enterprise_management::SystemTimezoneProto::DISABLED.
  // For regular users returns
  // enterprise_management::SystemTimezoneProto::USERS_DECIDE.
  static int GetEffectiveAutomaticTimezoneManagementSetting();

  // Local State initialization observer.
  void OnLocalStateInitialized(bool initialized);

  base::ObserverList<Observer>::Unchecked observers_;

  // This is non-null only after user logs in.
  raw_ptr<PrefService, DanglingUntriaged | ExperimentalAsh>
      primary_user_prefs_ = nullptr;

  // This is used to subscribe to policy preference.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // True if initial policy values are loaded.
  bool local_state_initialized_ = false;

  // True if TimeZoneResolverManager may start/stop on its own.
  // Becomes true after UpdateTimezoneResolver() has been called at least once.
  bool initialized_ = false;

  base::WeakPtrFactory<TimeZoneResolverManager> weak_factory_{this};
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_
