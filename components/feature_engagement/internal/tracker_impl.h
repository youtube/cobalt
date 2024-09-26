// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TRACKER_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TRACKER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feature_engagement {
class AvailabilityModel;
class ConditionValidator;
class Configuration;
class DisplayLockController;
class DisplayLockHandle;
class EventModel;
class TimeProvider;

namespace test {
class ScopedIphFeatureList;
}

// The internal implementation of the Tracker.
class TrackerImpl : public Tracker {
 public:
  TrackerImpl(std::unique_ptr<EventModel> event_model,
              std::unique_ptr<AvailabilityModel> availability_model,
              std::unique_ptr<Configuration> configuration,
              std::unique_ptr<DisplayLockController> display_lock_controller,
              std::unique_ptr<ConditionValidator> condition_validator,
              std::unique_ptr<TimeProvider> time_provider,
              base::WeakPtr<TrackerEventExporter> event_exporter);

  TrackerImpl(const TrackerImpl&) = delete;
  TrackerImpl& operator=(const TrackerImpl&) = delete;

  ~TrackerImpl() override;

  // Tracker implementation.
  void NotifyEvent(const std::string& event) override;
  bool ShouldTriggerHelpUI(const base::Feature& feature) override;
  TriggerDetails ShouldTriggerHelpUIWithSnooze(
      const base::Feature& feature) override;
  bool WouldTriggerHelpUI(const base::Feature& feature) const override;
  Tracker::TriggerState GetTriggerState(
      const base::Feature& feature) const override;
  bool HasEverTriggered(const base::Feature& feature,
                        bool from_window) const override;
  void Dismissed(const base::Feature& feature) override;
  void DismissedWithSnooze(const base::Feature& feature,
                           absl::optional<SnoozeAction> snooze_action) override;
  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override;
  bool IsInitialized() const override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;
  void SetPriorityNotification(const base::Feature& feature) override;
  absl::optional<std::string> GetPendingPriorityNotification() override;
  void RegisterPriorityNotificationHandler(const base::Feature& feature,
                                           base::OnceClosure callback) override;
  void UnregisterPriorityNotificationHandler(
      const base::Feature& feature) override;

 private:
  friend test::ScopedIphFeatureList;

  // Invoked by the EventModel when it has been initialized.
  void OnEventModelInitializationFinished(bool success);

  // Invoked by the AvailabilityModel when it has been initialized.
  void OnAvailabilityModelInitializationFinished(bool success);

  // Invoked by the TrackerEventExporter if it has any events to
  // migrate.
  void OnReceiveExportedEvents(
      std::vector<TrackerEventExporter::EventData> events);

  // Returns whether both underlying models have finished initializing.
  // This returning true does not mean the initialization was a success, just
  // that it is finished.
  bool IsInitializationFinished() const;

  // Posts the results to the OnInitializedCallbacks if
  // IsInitializationFinished() returns true.
  void MaybePostInitializedCallbacks();

  // Computes and records the duration since one of the `ShouldTriggerHelpUI`
  // methods were called and returned true. This logs a time histogram based on
  // the feature name.
  void RecordShownTime(const base::Feature& feature);

  // Gets internal data used by test::ScopedIphFeatureList.
  static std::map<const base::Feature*, size_t>& GetAllowedTestFeatureMap();

  // Returns whether a feature engagement feature is blocked by
  // test::ScopedIphFeatureList.
  static bool IsFeatureBlockedByTest(const base::Feature& feature);

  // The currently recorded start times (one per feature currently presented).
  std::map<std::string, base::Time> start_times_;

  // The current model for all events.
  std::unique_ptr<EventModel> event_model_;

  // The current model for when particular features were enabled.
  std::unique_ptr<AvailabilityModel> availability_model_;

  // The current configuration for all features.
  std::unique_ptr<Configuration> configuration_;

  // The DisplayLockController provides functionality for letting API users hold
  // a lock to ensure no feature enlightenment is happening while any lock is
  // held.
  std::unique_ptr<DisplayLockController> display_lock_controller_;

  // The ConditionValidator provides functionality for knowing when to trigger
  // help UI.
  std::unique_ptr<ConditionValidator> condition_validator_;

  // A utility for retriving time-related information.
  std::unique_ptr<TimeProvider> time_provider_;

  // The exporter for any new events to migrate into the tracker.
  base::WeakPtr<TrackerEventExporter> event_exporter_;

  // Whether the initialization of the underlying EventModel has finished.
  bool event_model_initialization_finished_;

  // Whether the initialization of the underlying AvailabilityModel has
  // finished.
  bool availability_model_initialization_finished_;

  // Whether event migration has been finished.
  bool event_migration_finished_ = false;

  // The list of callbacks to invoke when initialization has finished. This
  // is cleared after the initialization has happened.
  std::vector<OnInitializedCallback> on_initialized_callbacks_;

  // Registered priority notification handlers for various features.
  std::map<std::string, base::OnceClosure> priority_notification_handlers_;

  base::WeakPtrFactory<TrackerImpl> weak_ptr_factory_{this};
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_TRACKER_IMPL_H_
