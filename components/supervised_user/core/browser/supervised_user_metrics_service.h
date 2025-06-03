// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_METRICS_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_METRICS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
}  // namespace base

namespace supervised_user {
class SupervisedUserURLFilter;

// Service to initialize and control metric recorders of supervised users.
class SupervisedUserMetricsService : public KeyedService {
 public:
  // Interface for observing events on the SupervisedUserMetricsService.
  class Observer : public base::CheckedObserver {
   public:
    // Called when we detect a new day. This event can fire sooner or later than
    // 24 hours due to clock or time zone changes.
    virtual void OnNewDay() {}
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Returns the day id for a given time for testing.
  static int GetDayIdForTesting(base::Time time);

  explicit SupervisedUserMetricsService(
      PrefService* pref_service,
      supervised_user::SupervisedUserURLFilter* url_filter);
  SupervisedUserMetricsService(const SupervisedUserMetricsService&) = delete;
  SupervisedUserMetricsService& operator=(const SupervisedUserMetricsService&) =
      delete;
  ~SupervisedUserMetricsService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Helper function to check if a new day has arrived.
  void CheckForNewDay();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const raw_ptr<PrefService> pref_service_;

  // A periodic timer that checks if a new day has arrived.
  base::RepeatingTimer timer_;

  // The |observers_| list is a superset of the |supervised_user_metrics_|.
  base::ObserverList<Observer> observers_;
  std::vector<std::unique_ptr<Observer>> supervised_user_metrics_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_METRICS_SERVICE_H_
