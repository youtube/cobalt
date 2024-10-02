// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_metadata.h"

namespace content_settings {

MockProvider::MockProvider() : read_only_(false) {}

MockProvider::MockProvider(bool read_only) : read_only_(read_only) {}

MockProvider::~MockProvider() = default;

std::unique_ptr<RuleIterator> MockProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, nullptr);
}

bool MockProvider::SetWebsiteSetting(
    const ContentSettingsPattern& requesting_url_pattern,
    const ContentSettingsPattern& embedding_url_pattern,
    ContentSettingsType content_type,
    base::Value&& in_value,
    const ContentSettingConstraints& constraints) {
  if (read_only_)
    return false;
  if (!in_value.is_none()) {
    value_map_.SetValue(requesting_url_pattern, embedding_url_pattern,
                        content_type, std::move(in_value),
                        {.expiration = constraints.expiration,
                         .session_model = constraints.session_model});
  } else {
    base::Value value(std::move(in_value));
    value_map_.DeleteValue(requesting_url_pattern, embedding_url_pattern,
                           content_type);
  }
  return true;
}

void MockProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
}

}  // namespace content_settings
