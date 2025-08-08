// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_LIFECYCLE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_LIFECYCLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/feature_promo_storage_service.h"

namespace base {
struct Feature;
}

namespace user_education {

class HelpBubble;

// Implements business logic around the lifecycle of an IPH feature promo,
// depending on promo type and subtype. Tracks information about the IPH in
// question through `FeaturePromoStorageService`.
class FeaturePromoLifecycle {
 public:
  using CloseReason = FeaturePromoStorageService::CloseReason;
  using PromoSubtype = FeaturePromoSpecification::PromoSubtype;
  using PromoType = FeaturePromoSpecification::PromoType;

  static constexpr base::TimeDelta kDefaultSnoozeDuration = base::Days(7);

  FeaturePromoLifecycle(FeaturePromoStorageService* storage_service,
                        const base::StringPiece& app_id,
                        const base::Feature* iph_feature,
                        PromoType promo_type,
                        PromoSubtype promo_subtype);
  ~FeaturePromoLifecycle();

  FeaturePromoLifecycle(const FeaturePromoLifecycle&) = delete;
  void operator=(const FeaturePromoLifecycle&) = delete;

  // Returns whether the policy and previous usage of this IPH would allow it to
  // be shown again; for example, a snoozeable IPH cannot show if it is
  // currently in the snooze period.
  FeaturePromoResult CanShow() const;

  // Notifies that the promo was shown. `tracker` will be used to release the
  // feature when the promo ends.
  void OnPromoShown(std::unique_ptr<HelpBubble> help_bubble,
                    feature_engagement::Tracker* tracker);

  // Notifies that the promo is being shown for a demo. No data will be stored.
  void OnPromoShownForDemo(std::unique_ptr<HelpBubble> help_bubble);

  // Notifies that the promo bubble closed. If none of the other dismissed,
  // snoozed, continued, etc. callbacks have been called, assumes that the
  // bubble was closed programmatically without user input.
  //
  // Returns whether the promo was aborted as a result.
  bool OnPromoBubbleClosed();

  // Notifies that the promo was ended for the specified `close_reason`.
  // May result in pref data and/or histogram logging. If `continue_promo` is
  // true, the bubble should be closed but the promo is not ended -
  // `OnContinuedPromoEnded()` should be called when the continuation finishes.
  void OnPromoEnded(CloseReason close_reason, bool continue_promo = false);

  // For custom action, Tutorial, etc. indicates that the
  void OnContinuedPromoEnded(bool completed_successfully);

  HelpBubble* help_bubble() { return help_bubble_.get(); }
  const HelpBubble* help_bubble() const { return help_bubble_.get(); }
  const base::Feature* iph_feature() const { return iph_feature_; }
  bool was_started() const { return state_ != State::kNotStarted; }
  bool is_promo_active() const {
    return state_ == State::kRunning || state_ == State::kContinued;
  }
  bool is_bubble_visible() const { return state_ == State::kRunning; }
  bool is_demo() const { return was_started() && !tracker_; }
  PromoType promo_type() const { return promo_type_; }
  PromoSubtype promo_subtype() const { return promo_subtype_; }

 private:
  enum class State { kNotStarted, kRunning, kContinued, kClosed };

  // Records `PromoData` about the promo closing, unless in demo mode.
  void MaybeWriteClosePromoData(CloseReason close_reason);

  // Records that an IPH was shown, including type and identity.
  void RecordShown();

  // Records user actions and histograms that discern what action was taken to
  // close a promotion. Does not record in demo mode.
  void MaybeRecordCloseReason(CloseReason close_reason);

  // If the promo is running, ends it, possibly dismissing the Tracker.
  bool MaybeEndPromo();

  // The service that stores non-transient data about the IPH.
  const raw_ptr<FeaturePromoStorageService> storage_service_;

  // The current app id, or empty for none.
  const std::string app_id_;

  // Data about the current IPH.
  const raw_ptr<const base::Feature> iph_feature_;
  const PromoType promo_type_;
  const PromoSubtype promo_subtype_;

  // The current state of the promo.
  State state_ = State::kNotStarted;
  bool wrote_close_data_ = false;
  bool tracker_dismissed_ = false;

  // The tracker, in the event the bubble needs to be dismissed.
  raw_ptr<feature_engagement::Tracker> tracker_ = nullptr;
  std::unique_ptr<HelpBubble> help_bubble_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_LIFECYCLE_H_
