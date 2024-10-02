// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace blink {

TestInterestGroupBuilder::TestInterestGroupBuilder(url::Origin owner,
                                                   std::string name)
    : interest_group_(
          /*expiry=*/base::Time::Now() + base::Days(30),
          std::move(owner),
          std::move(name),
          /*priority=*/0.0,
          /*enable_bidding_signals_prioritization=*/false,
          /*priority_vector=*/absl::nullopt,
          /*priority_signals_overrides=*/absl::nullopt,
          /*seller_capabilities=*/absl::nullopt,
          /*all_sellers_capabilities=*/
          {}, /*execution_mode=*/
          blink::InterestGroup::ExecutionMode::kCompatibilityMode,
          /*bidding_url=*/absl::nullopt,
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt,
          /*ad_sizes=*/{},
          /*size_groups=*/{}) {}

TestInterestGroupBuilder::~TestInterestGroupBuilder() = default;

InterestGroup TestInterestGroupBuilder::Build() {
  return std::move(interest_group_);
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetExpiry(
    base::Time expiry) {
  interest_group_.expiry = expiry;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetPriority(
    double priority) {
  interest_group_.priority = priority;
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetEnableBiddingSignalsPrioritization(
    bool enable_bidding_signals_prioritization) {
  interest_group_.enable_bidding_signals_prioritization =
      enable_bidding_signals_prioritization;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetPriorityVector(
    absl::optional<base::flat_map<std::string, double>> priority_vector) {
  interest_group_.priority_vector = std::move(priority_vector);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetPrioritySignalsOverrides(
    absl::optional<base::flat_map<std::string, double>>
        priority_signals_overrides) {
  interest_group_.priority_signals_overrides =
      std::move(priority_signals_overrides);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetSellerCapabilities(
    absl::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>
        seller_capabilities) {
  interest_group_.seller_capabilities = std::move(seller_capabilities);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAllSellerCapabilities(
    SellerCapabilitiesType all_sellers_capabilities) {
  interest_group_.all_sellers_capabilities =
      std::move(all_sellers_capabilities);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetExecutionMode(
    InterestGroup::ExecutionMode execution_mode) {
  interest_group_.execution_mode = execution_mode;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetBiddingUrl(
    absl::optional<GURL> bidding_url) {
  interest_group_.bidding_url = std::move(bidding_url);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetBiddingWasmHelperUrl(
    absl::optional<GURL> bidding_wasm_helper_url) {
  interest_group_.bidding_wasm_helper_url = std::move(bidding_wasm_helper_url);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetUpdateUrl(
    absl::optional<GURL> update_url) {
  interest_group_.update_url = std::move(update_url);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetTrustedBiddingSignalsUrl(
    absl::optional<GURL> trusted_bidding_signals_url) {
  interest_group_.trusted_bidding_signals_url =
      std::move(trusted_bidding_signals_url);
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetTrustedBiddingSignalsKeys(
    absl::optional<std::vector<std::string>> trusted_bidding_signals_keys) {
  interest_group_.trusted_bidding_signals_keys =
      std::move(trusted_bidding_signals_keys);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetUserBiddingSignals(
    absl::optional<std::string> user_bidding_signals) {
  interest_group_.user_bidding_signals = std::move(user_bidding_signals);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAds(
    absl::optional<std::vector<InterestGroup::Ad>> ads) {
  interest_group_.ads = std::move(ads);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAdComponents(
    absl::optional<std::vector<InterestGroup::Ad>> ad_components) {
  interest_group_.ad_components = std::move(ad_components);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAdSizes(
    absl::optional<base::flat_map<std::string, blink::AdSize>> ad_sizes) {
  interest_group_.ad_sizes = std::move(ad_sizes);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetSizeGroups(
    absl::optional<base::flat_map<std::string, std::vector<std::string>>>
        size_groups) {
  interest_group_.size_groups = std::move(size_groups);
  return *this;
}

}  // namespace blink
