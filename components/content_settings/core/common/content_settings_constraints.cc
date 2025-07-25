// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_constraints.h"

namespace content_settings {

ContentSettingConstraints::ContentSettingConstraints()
    : ContentSettingConstraints(base::Time::Now()) {}

ContentSettingConstraints::ContentSettingConstraints(base::Time now)
    : created_at_(now) {}

ContentSettingConstraints::ContentSettingConstraints(
    ContentSettingConstraints&& other) = default;
ContentSettingConstraints::ContentSettingConstraints(
    const ContentSettingConstraints& other) = default;
ContentSettingConstraints& ContentSettingConstraints::operator=(
    ContentSettingConstraints&& other) = default;
ContentSettingConstraints& ContentSettingConstraints::operator=(
    const ContentSettingConstraints& other) = default;

ContentSettingConstraints::~ContentSettingConstraints() = default;

bool ContentSettingConstraints::operator==(
    const ContentSettingConstraints& other) const {
  return std::tuple(expiration(), session_model_,
                    track_last_visit_for_autoexpiration_) ==
         std::tuple(other.expiration(), other.session_model_,
                    other.track_last_visit_for_autoexpiration_);
}

bool ContentSettingConstraints::operator!=(
    const ContentSettingConstraints& other) const {
  return !(*this == other);
}

}  // namespace content_settings
