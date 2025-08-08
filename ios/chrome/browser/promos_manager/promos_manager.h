// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_

#import <Foundation/Foundation.h>

#import <map>

#import "base/containers/flat_set.h"
#import "base/containers/small_map.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

@class ImpressionLimit;

namespace promos_manager {
enum class Promo;
}  // namespace promos_manager

struct PromoConfigComparator {
  using is_transparent = std::true_type;
  constexpr bool operator()(const PromoConfig& lhs,
                            const PromoConfig& rhs) const {
    return lhs.identifier < rhs.identifier;
  }
  constexpr bool operator()(const promos_manager::Promo& lhs,
                            const PromoConfig& rhs) const {
    return lhs < rhs.identifier;
  }
  constexpr bool operator()(const PromoConfig& lhs,
                            const promos_manager::Promo& rhs) const {
    return lhs.identifier < rhs;
  }
};

using PromoConfigsSet = base::flat_set<PromoConfig, PromoConfigComparator>;

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams interested in displaying promos should
// leverage this manager, and only use the following methods:
// 1. RegisterPromoForSingleDisplay
// 2. RegisterPromoForContinuousDisplay
// 3. DeregisterPromo
class PromosManager : public KeyedService {
 public:
  PromosManager();

#pragma mark - Public-facing APIs

  // Registers `promo` for continuous display, and persists registration status
  // across app launches.
  virtual void RegisterPromoForContinuousDisplay(
      promos_manager::Promo promo) = 0;

  // Registers `promo` for single (one-time) display, and persists registration
  // status across app launches.
  virtual void RegisterPromoForSingleDisplay(promos_manager::Promo promo) = 0;

  // Same as `RegisterPromoForSingleDisplay`, except that the promo can only be
  // active after `becomes_active_after_period`. Pending status with time are
  // persisted.
  virtual void RegisterPromoForSingleDisplay(
      promos_manager::Promo promo,
      base::TimeDelta becomes_active_after_period) = 0;

  // Deregisters `promo` (stopping `promo` from being displayed) by removing the
  // promo entry from the single-display and continuous-display active promos
  // lists.
  virtual void DeregisterPromo(promos_manager::Promo promo) = 0;

#pragma mark - Internal APIs

  // Initialize the Promos Manager by restoring state from Prefs. Must be called
  // after creation and before any other operation.
  virtual void Init();

  // Ingests promo-specific impression limits and stores them in-memory for
  // later reference.
  virtual void InitializePromoConfigs(PromoConfigsSet promo_configs) = 0;

  // Records the impression of `promo` in the impression history.
  //
  // NOTE: If `promo` is a single-display promo, it will be automatically
  // deregistered.
  virtual void RecordImpression(promos_manager::Promo promo) = 0;

  // Returns the next promo for display, if any.
  virtual absl::optional<promos_manager::Promo> NextPromoForDisplay() = 0;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
