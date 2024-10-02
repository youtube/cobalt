// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TEST_MOCK_TRACKER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TEST_MOCK_TRACKER_H_

#include <memory>
#include <string>

#include "components/feature_engagement/public/tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feature_engagement {
namespace test {

class MockTracker : public Tracker {
 public:
  MockTracker();

  MockTracker(const MockTracker&) = delete;
  MockTracker& operator=(const MockTracker&) = delete;

  ~MockTracker() override;

  // Tracker implememtation.
  MOCK_METHOD1(NotifyEvent, void(const std::string& event));
  MOCK_METHOD1(ShouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_METHOD1(ShouldTriggerHelpUIWithSnooze,
               TriggerDetails(const base::Feature& feature));
  MOCK_CONST_METHOD1(WouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_CONST_METHOD2(HasEverTriggered,
                     bool(const base::Feature& feature, bool from_window));
  MOCK_CONST_METHOD1(GetTriggerState,
                     Tracker::TriggerState(const base::Feature& feature));
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(Dismissed, void(const base::Feature& feature));
  MOCK_METHOD2(DismissedWithSnooze,
               void(const base::Feature& feature,
                    absl::optional<SnoozeAction> snooze_action));
  MOCK_METHOD0(AcquireDisplayLock, std::unique_ptr<DisplayLockHandle>());
  MOCK_METHOD1(SetPriorityNotification, void(const base::Feature&));
  MOCK_METHOD0(GetPendingPriorityNotification, absl::optional<std::string>());
  MOCK_METHOD2(RegisterPriorityNotificationHandler,
               void(const base::Feature&, base::OnceClosure));
  MOCK_METHOD1(UnregisterPriorityNotificationHandler,
               void(const base::Feature&));
  MOCK_METHOD1(AddOnInitializedCallback, void(OnInitializedCallback callback));
};

}  // namespace test
}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TEST_MOCK_TRACKER_H_
