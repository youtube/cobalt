// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_SESSION_POLICY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_SESSION_POLICY_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/user_education_data.h"

namespace user_education {

class UserEducationSessionManager;
class FeaturePromoSpecification;
class UserEducationStorageService;

namespace test {
class UserEducationSessionTestUtil;
}

// Provides priority information about a promo based on its type and subtype.
class FeaturePromoPriorityProvider {
 public:
  // Describes whether the promotion is heavyweight (comparable to a non-toast
  // IPH in interactivity).
  enum class PromoWeight { kLight, kHeavy };

  // Describes whether the promotion is high-priority, such as a mandatory
  // legal or privacy notice.
  enum class PromoPriority { kLow, kMedium, kHigh };

  // Information about a promo that is either being shown or is trying to show.
  struct PromoPriorityInfo {
    PromoWeight weight = PromoWeight::kLight;
    PromoPriority priority = PromoPriority::kLow;

    // Used in tests to do comparisons.
    auto operator<=>(const PromoPriorityInfo& other) const = default;
  };

  FeaturePromoPriorityProvider() = default;
  FeaturePromoPriorityProvider(const FeaturePromoPriorityProvider&) = delete;
  void operator=(const FeaturePromoPriorityProvider&) = delete;
  virtual ~FeaturePromoPriorityProvider() = default;

  // Gets a promo info from a specification. Different policies might interpret
  // different specifications differently.
  //
  // The default ties weight to type (toast, legacy, rotating are light) and
  // priority to subtype (legal = high, keyed and actionable = medium, all
  // others low).
  virtual PromoPriorityInfo GetPromoPriorityInfo(
      const FeaturePromoSpecification& spec) const;
};

// Policy that defines how promos are allowed or disallowed due to the state of
// the current session.
class FeaturePromoSessionPolicy : public FeaturePromoPriorityProvider {
 public:
  // Create a new policy which stores its state in `storage_service` and which
  // uses `session_manager` to determine the state of the current session.
  FeaturePromoSessionPolicy();
  ~FeaturePromoSessionPolicy() override;

  // Sets up the policy with its storage service.
  virtual void Init(UserEducationSessionManager* session_manager,
                    UserEducationStorageService* storage_service);

  // Indicates that a promo is being shown. Value must not be `NoPromo`.
  virtual void NotifyPromoShown(const PromoPriorityInfo& promo_shown);

  // Indicates that a promo has been dismissed. Value must not be `NoPromo`.
  virtual void NotifyPromoEnded(const PromoPriorityInfo& promo_ended,
                                FeaturePromoClosedReason close_reason);

  // Determines whether `to_show` (which must not be `NoPromo`) can be shown.
  // The `currently_showing` parameter represents what kind of promo is
  // currently showing if any. Returns `FeaturePromoResult::Success()` if the
  // promo is allowed; returns a reason for rejecting the promo otherwise.
  virtual FeaturePromoResult CanShowPromo(
      PromoPriorityInfo to_show,
      std::optional<PromoPriorityInfo> currently_showing) const;

 protected:
  UserEducationSessionManager* session_manager() const {
    return session_manager_;
  }
  UserEducationStorageService* storage_service() const {
    return storage_service_;
  }

 private:
  friend test::UserEducationSessionTestUtil;

  raw_ptr<UserEducationSessionManager> session_manager_ = nullptr;
  raw_ptr<UserEducationStorageService> storage_service_ = nullptr;
  std::optional<base::Time> current_promo_shown_time_;
};

// Represents the promo policy for User Education Experience V2, above and
// beyond the common promo logic.
class FeaturePromoSessionPolicyV2 : public FeaturePromoSessionPolicy {
 public:
  FeaturePromoSessionPolicyV2();
  ~FeaturePromoSessionPolicyV2() override;

  // FeaturePromoSessionPolicyCommon:
  FeaturePromoResult CanShowPromo(
      PromoPriorityInfo to_show,
      std::optional<PromoPriorityInfo> currently_showing) const override;

 protected:
  FeaturePromoSessionPolicyV2(base::TimeDelta session_start_grace_period,
                              base::TimeDelta heavyweight_promo_cooldown);

 private:
  const base::TimeDelta session_start_grace_period_;
  const base::TimeDelta heavyweight_promo_cooldown_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_SESSION_POLICY_H_
