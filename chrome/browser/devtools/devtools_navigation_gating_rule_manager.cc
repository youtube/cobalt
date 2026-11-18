// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace {

constexpr char kAllowlistKey[] = "allowlist";
constexpr char kBlocklistKey[] = "blocklist";

}  // namespace

// static
const DevToolsNavigationGatingRuleManager&
DevToolsNavigationGatingRuleManager::Get() {
  static const base::NoDestructor<DevToolsNavigationGatingRuleManager> instance(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDevToolsNavigationGatingRules));
  return *instance;
}

// static
DevToolsNavigationGatingRuleManager
DevToolsNavigationGatingRuleManager::CreateForTesting(
    std::string_view rules_json) {
  return DevToolsNavigationGatingRuleManager(rules_json);
}

DevToolsNavigationGatingRuleManager::DevToolsNavigationGatingRuleManager(
    std::string_view rules_json) {
  if (rules_json.empty()) {
    return;
  }

  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      rules_json, base::JSON_PARSE_RFC);
  if (!parsed_json.has_value()) {
    LOG(ERROR) << "Failed to parse DevTools navigation gating rules: "
               << parsed_json.error().message;
    return;
  }

  const base::DictValue* dict = parsed_json->GetIfDict();
  if (!dict) {
    LOG(ERROR) << "DevTools navigation gating rules must be a JSON dictionary.";
    return;
  }

  auto parse_and_add_rules = [&](std::string_view key, ContentSetting setting) {
    const base::Value* val = dict->Find(key);
    if (!val) {
      return;
    }
    const base::ListValue* list = val->GetIfList();
    if (!list) {
      LOG(ERROR) << "DevTools navigation gating rules key '" << key
                 << "' must be a list.";
      return;
    }

    for (const auto& item : *list) {
      if (!item.is_string()) {
        LOG(ERROR) << "DevTools navigation gating list '" << key
                   << "' contains a non-string element.";
        continue;
      }
      ContentSettingsPattern pattern =
          ContentSettingsPattern::FromString(item.GetString());
      if (!pattern.IsValid()) {
        LOG(ERROR) << "DevTools navigation gating list '" << key
                   << "' contains an invalid pattern: '" << item.GetString()
                   << "'";
        continue;
      }
      rules_.SetValue(ContentSettingsPattern::Wildcard(), pattern,
                      content_settings::ContentSettingToValue(setting), {});
    }
    return;
  };

  has_allowlist_ = !!dict->FindList(kAllowlistKey);
  parse_and_add_rules(kAllowlistKey, CONTENT_SETTING_ALLOW);
  parse_and_add_rules(kBlocklistKey, CONTENT_SETTING_BLOCK);
}

DevToolsNavigationGatingRuleManager::~DevToolsNavigationGatingRuleManager() =
    default;

bool DevToolsNavigationGatingRuleManager::MayBlockNavigation() const {
  return !rules_.empty() || has_allowlist_;
}

bool DevToolsNavigationGatingRuleManager::IsNavigationAllowed(
    const GURL& url) const {
  if (!MayBlockNavigation()) {
    return true;
  }

  // GURL() is passed as primary_url to represent the wildcard source pattern.
  const content_settings::RuleEntry* rule_entry = rules_.Find(GURL(), url);

  if (!rule_entry) {
    return !has_allowlist_;
  }

  std::optional<ContentSetting> setting =
      content_settings::ParseContentSettingValue(rule_entry->second.value);
  CHECK(setting.has_value());
  switch (setting.value()) {
    case CONTENT_SETTING_ALLOW:
      return true;
    case CONTENT_SETTING_BLOCK:
      return false;
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_ASK:
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }

  NOTREACHED();
}
