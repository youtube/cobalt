// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

namespace resource_coordinator {

class UsageClock;

class TestLifecycleUnit : public LifecycleUnitBase {
 public:
  using LifecycleUnitBase::OnLifecycleUnitVisibilityChanged;
  using LifecycleUnitBase::SetState;

  TestLifecycleUnit(base::TimeTicks last_focused_time = base::TimeTicks(),
                    base::ProcessHandle process_handle = base::ProcessHandle(),
                    bool can_discard = true);
  TestLifecycleUnit(content::Visibility visibility, UsageClock* usage_clock);
  explicit TestLifecycleUnit(LifecycleUnitSourceBase* source);

  TestLifecycleUnit(const TestLifecycleUnit&) = delete;
  TestLifecycleUnit& operator=(const TestLifecycleUnit&) = delete;

  ~TestLifecycleUnit() override;

  void SetLastFocusedTime(base::TimeTicks last_focused_time) {
    last_focused_time_ = last_focused_time;
  }

  void SetSortKey(LifecycleUnit::SortKey sort_key) { sort_key_ = sort_key; }

  void SetTitle(base::StringPiece16 title) { title_ = std::u16string(title); }

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override;
  std::u16string GetTitle() const override;
  base::TimeTicks GetLastFocusedTime() const override;
  base::ProcessHandle GetProcessHandle() const override;
  SortKey GetSortKey() const override;
  content::Visibility GetVisibility() const override;
  LifecycleUnitLoadingState GetLoadingState() const override;
  bool Load() override;
  int GetEstimatedMemoryFreedOnDiscardKB() const override;
  bool CanDiscard(LifecycleUnitDiscardReason reason,
                  DecisionDetails* decision_details) const override;
  bool Discard(LifecycleUnitDiscardReason discard_reason,
               uint64_t resident_set_size_estimate) override;
  LifecycleUnitDiscardReason GetDiscardReason() const override;

 private:
  std::u16string title_;
  base::TimeTicks last_focused_time_;
  base::ProcessHandle process_handle_;
  LifecycleUnit::SortKey sort_key_;
  bool can_discard_ = true;
};

// Helper funtions for testing CanDiscard policy.
void ExpectCanDiscardTrue(const LifecycleUnit* lifecycle_unit,
                          LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardTrueAllReasons(const LifecycleUnit* lifecycle_unit);
void ExpectCanDiscardFalse(const LifecycleUnit* lifecycle_unit,
                           DecisionFailureReason failure_reason,
                           LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardFalseAllReasons(const LifecycleUnit* lifecycle_unit,
                                     DecisionFailureReason failure_reason);
void ExpectCanDiscardFalseTrivial(const LifecycleUnit* lifecycle_unit,
                                  LifecycleUnitDiscardReason discard_reason);
void ExpectCanDiscardFalseTrivialAllReasons(const LifecycleUnit* lu);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TEST_LIFECYCLE_UNIT_H_
