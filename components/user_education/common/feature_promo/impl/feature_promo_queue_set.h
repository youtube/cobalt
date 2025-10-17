// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_SET_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_SET_H_

#include <functional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_queue.h"

namespace user_education::internal {

// Represents a set of queues, sorted by priority.
//
// Promos may be queued and dequeued, and will be cleaned up when they expire
// or fail required conditions.
class FeaturePromoQueueSet {
 public:
  using Priority = FeaturePromoPriorityProvider::PromoPriority;
  using PromoInfo = std::pair<raw_ref<const base::Feature>, Priority>;

  FeaturePromoQueueSet(const FeaturePromoPriorityProvider& priority_provider,
                       const UserEducationTimeProvider& time_provider);
  FeaturePromoQueueSet(FeaturePromoQueueSet&&) noexcept;
  FeaturePromoQueueSet& operator=(FeaturePromoQueueSet&&) noexcept;
  ~FeaturePromoQueueSet();

  // Adds a queue at `priority` with the given precondition providers and
  // timeout.
  void AddQueue(Priority priority,
                const PreconditionListProvider& required_preconditions_provider,
                const PreconditionListProvider& wait_for_preconditions_provider,
                base::TimeDelta queue_timeout);

  // Returns true if no queues have promos.
  bool IsEmpty() const;

  // Returns the total number of queued promos.
  size_t GetTotalQueuedCount() const;

  // Returns the number of queued promos for `priority`.
  size_t GetQueuedCount(Priority priority) const;

  // Returns whether `iph_feature` is queued.
  bool IsQueued(const base::Feature& iph_feature) const;

  // Returns whether the feature described by `spec` could be queued with
  // `promo_params`. Potentially as expensive as actually queueing the promo,
  // so use with care.
  FeaturePromoResult CanQueue(const FeaturePromoSpecification& spec,
                              const FeaturePromoParams& promo_params) const;

  // Returns whether the feature described by `spec` could be shown immediately
  // `promo_params`. Potentially more expensive than actually queueing the
  // promo, so use with extreme care.
  FeaturePromoResult CanShow(const FeaturePromoSpecification& spec,
                             const FeaturePromoParams& promo_params) const;

  // Attempts to queue a new promo defined by `spec` with `promo_params`. If
  // queueing the promo fails, for any reason the "show promo result" callback
  // will be posted with an appropriate failure code and the promo discarded.
  void TryToQueue(const FeaturePromoSpecification& spec,
                  FeaturePromoParams promo_params);

  // Cancels promo for `iph_feature` if it is queued; returns whether it was
  // canceled.
  bool Cancel(const base::Feature& iph_feature);

  // Removes any ineligible promos from the queue and then returns the feature
  // of the next entry that is eligible to show along with its priority, or
  // nullopt if there is none.
  //
  // Does not remove the found entry (if any) from the queue.
  std::optional<PromoInfo> UpdateAndIdentifyNextEligiblePromo();

  // Pops and returns the promo `to_unqueue` from the queue (which must be
  // present). Call `UpdateAndIdentifyNextEligiblePromo()` to get the promo info
  // to unqueue.
  EligibleFeaturePromo UnqueueEligiblePromo(const PromoInfo& to_unqueue);

  // Clear ineligible promos out of the queues, sending appropriate failure
  // messages.
  void RemoveIneligiblePromos();

  // Fails all promos in the queue with the given `failure_reason`.
  void FailAll(FeaturePromoResult::Failure failure_reason);

 private:
  // List of queues, sorted in descending order of priority.
  base::flat_map<Priority, internal::FeaturePromoQueue, std::greater<>> queues_;
  raw_ref<const FeaturePromoPriorityProvider> priority_provider_;
  raw_ref<const UserEducationTimeProvider> time_provider_;
};

}  // namespace user_education::internal

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_IMPL_FEATURE_PROMO_QUEUE_SET_H_
