// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/navigation_data.h"

#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace webapps {

NavigationData::NavigationData()
    : launch_source_(apps::LaunchSource::kUnknown) {}
NavigationData::~NavigationData() = default;
NavigationData::NavigationData(NavigationData&&) = default;

bool NavigationData::navigation_capturing_force_off() const {
  return navigation_capturing_force_off_;
}

void NavigationData::SetNavigationCapturingForceOff(bool force_off) {
  navigation_capturing_force_off_ = force_off;
}

std::optional<LaunchParams> NavigationData::launch_params() const {
  return launch_params_;
}

void NavigationData::SetLaunchParams(LaunchParams launch_params) {
  launch_params_ = std::move(launch_params);
}

apps::LaunchSource NavigationData::launch_source() const {
  return launch_source_;
}

void NavigationData::SetLaunchSource(apps::LaunchSource launch_source) {
  launch_source_ = launch_source;
}

}  // namespace webapps
