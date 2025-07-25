// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_nearby_share_delegate.h"

#include "base/time/time.h"

namespace ash {

TestNearbyShareDelegate::TestNearbyShareDelegate() = default;

TestNearbyShareDelegate::~TestNearbyShareDelegate() = default;

bool TestNearbyShareDelegate::IsEnabled() {
  return is_enabled_;
}

bool TestNearbyShareDelegate::IsPodButtonVisible() {
  return is_pod_button_visible_;
}

bool TestNearbyShareDelegate::IsHighVisibilityOn() {
  return is_high_visibility_on_;
}

bool TestNearbyShareDelegate::IsEnableHighVisibilityRequestActive() const {
  return is_enable_high_visibility_request_active_;
}

base::TimeTicks TestNearbyShareDelegate::HighVisibilityShutoffTime() const {
  return high_visibility_shutoff_time_;
}

void TestNearbyShareDelegate::EnableHighVisibility() {
  method_calls_.emplace_back(Method::kEnableHighVisibility);
}

void TestNearbyShareDelegate::DisableHighVisibility() {
  method_calls_.emplace_back(Method::kDisableHighVisibility);
}

void TestNearbyShareDelegate::ShowNearbyShareSettings() const {}

}  // namespace ash
