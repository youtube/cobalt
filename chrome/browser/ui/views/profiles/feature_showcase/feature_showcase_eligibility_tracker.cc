// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_eligibility_tracker.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_step_eligibility_checker.h"

namespace {

constexpr size_t kMaxFeatureShowcaseSteps = 3;
constexpr base::TimeDelta kEvaluationTimeout = base::Seconds(2);

}  // namespace

FeatureShowcaseEligibilityTracker::FeatureShowcaseEligibilityTracker(
    std::vector<std::unique_ptr<FeatureShowcaseStepEligibilityChecker>>
        checkers)
    : checkers_(std::move(checkers)) {}

FeatureShowcaseEligibilityTracker::~FeatureShowcaseEligibilityTracker() =
    default;

void FeatureShowcaseEligibilityTracker::EvaluateEligibleSteps(
    Profile& profile,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  if (checkers_.empty()) {
    // Return asynchronously to match the behavior of the async path below.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<std::string>()));
    return;
  }

  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  eligible_results_.clear();
  completed_checkers_ = 0;

  if (on_eligibility_evaluated_callback_) {
    // Return an empty result for the previous call to avoid silently dropping
    // the callback.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_eligibility_evaluated_callback_),
                                  std::vector<std::string>()));
  }

  on_eligibility_evaluated_callback_ = std::move(callback);

  timeout_timer_.Start(
      FROM_HERE, kEvaluationTimeout,
      base::BindOnce(&FeatureShowcaseEligibilityTracker::FinishEvaluation,
                     weak_ptr_factory_.GetWeakPtr()));

  size_t priority = 0;
  for (const auto& checker : checkers_) {
    checker->CheckEligibility(
        profile,
        base::BindOnce(
            &FeatureShowcaseEligibilityTracker::OnStepEligibilityDetermined,
            weak_ptr_factory_.GetWeakPtr(), priority++,
            checker->GetStepIdentifier()));
  }
}

void FeatureShowcaseEligibilityTracker::OnStepEligibilityDetermined(
    size_t priority,
    std::string identifier,
    bool is_eligible) {
  ++completed_checkers_;
  if (is_eligible) {
    eligible_results_.push_back({priority, std::move(identifier)});
  }

  if (completed_checkers_ == checkers_.size()) {
    FinishEvaluation();
  }
}

void FeatureShowcaseEligibilityTracker::FinishEvaluation() {
  timeout_timer_.Stop();
  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::ranges::sort(eligible_results_, {}, &Result::priority);

  std::vector<std::string> eligible_steps;
  for (const auto& result : eligible_results_) {
    eligible_steps.push_back(result.identifier);
  }

  if (eligible_steps.size() > kMaxFeatureShowcaseSteps) {
    eligible_steps.resize(kMaxFeatureShowcaseSteps);
  }

  // `PostTask` to prevent a potential use-after-free. If checks complete
  // synchronously, invoking the callback could synchronously destroy this
  // tracker while `EvaluateEligibleSteps` is still iterating over `checkers_`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_eligibility_evaluated_callback_),
                                std::move(eligible_steps)));
}
