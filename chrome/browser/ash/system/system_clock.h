// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_SYSTEM_CLOCK_H_
#define CHROME_BROWSER_ASH_SYSTEM_SYSTEM_CLOCK_H_

#include <memory>

#include "base/callback_list.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/user_manager/user_manager.h"

class PrefChangeRegistrar;

namespace user_manager {
class User;
}

namespace ash {
namespace system {

class SystemClockObserver;

// This is a global singleton (actually a member of BrowserProcessPlatformPart)
// that is responsible for correct time formatting. It listens to events that
// modify on-screen time representation (like ActiveUserChanged) and notifies
// observers.
class SystemClock : public LoginState::Observer,
                    public ProfileObserver,
                    public user_manager::UserManager::UserSessionStateObserver {
 public:
  SystemClock();

  SystemClock(const SystemClock&) = delete;
  SystemClock& operator=(const SystemClock&) = delete;

  ~SystemClock() override;

  // Could be used to temporary set the required clock type. At most one should
  // exist at the time.
  class ScopedHourClockType {
   public:
    explicit ScopedHourClockType(base::WeakPtr<SystemClock> system_clock);
    ~ScopedHourClockType();

    ScopedHourClockType(const ScopedHourClockType&) = delete;
    ScopedHourClockType& operator=(const ScopedHourClockType&) = delete;

    ScopedHourClockType(ScopedHourClockType&&);
    ScopedHourClockType& operator=(ScopedHourClockType&&);

    void UpdateClockType(base::HourClockType clock_type);

   private:
    base::WeakPtr<SystemClock> system_clock_;
  };

  ScopedHourClockType CreateScopedHourClockType(
      base::HourClockType hour_clock_type);

  void AddObserver(SystemClockObserver* observer);
  void RemoveObserver(SystemClockObserver* observer);

  bool ShouldUse24HourClock() const;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

 private:
  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  // LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  void OnSystemPrefChanged();

  void UpdateClockType();

  absl::optional<base::HourClockType> scoped_hour_clock_type_;

  raw_ptr<Profile, ExperimentalAsh> user_profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  std::unique_ptr<PrefChangeRegistrar> user_pref_registrar_;

  base::ObserverList<SystemClockObserver>::Unchecked observer_list_;

  base::CallbackListSubscription device_settings_observer_;

  base::WeakPtrFactory<SystemClock> weak_ptr_factory_{this};
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_SYSTEM_CLOCK_H_
