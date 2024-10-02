// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/content_settings/content_settings_store.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/features.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/content_settings/content_settings_helpers.h"

using content::BrowserThread;
using content_settings::ConcatenationIterator;
using content_settings::Rule;
using content_settings::RuleIterator;
using content_settings::OriginIdentifierValueMap;

namespace extensions {

struct ContentSettingsStore::ExtensionEntry {
  // Extension id.
  std::string id;
  // Installation time.
  base::Time install_time;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Content settings.
  OriginIdentifierValueMap settings;
  // Persistent incognito content settings.
  OriginIdentifierValueMap incognito_persistent_settings;
  // Session-only incognito content settings.
  OriginIdentifierValueMap incognito_session_only_settings;
};

ContentSettingsStore::ContentSettingsStore() {
  DCHECK(OnCorrectThread());
}

ContentSettingsStore::~ContentSettingsStore() = default;

constexpr char ContentSettingsStore::kContentSettingKey[];
constexpr char ContentSettingsStore::kContentSettingsTypeKey[];
constexpr char ContentSettingsStore::kPrimaryPatternKey[];
constexpr char ContentSettingsStore::kSecondaryPatternKey[];

std::unique_ptr<RuleIterator> ContentSettingsStore::GetRuleIterator(
    ContentSettingsType type,
    bool incognito) const {
  std::vector<std::unique_ptr<RuleIterator>> iterators;

  // The individual |RuleIterators| shouldn't lock; pass |lock_| to the
  // |ConcatenationIterator| in a locked state.
  std::unique_ptr<base::AutoLock> auto_lock =
      std::make_unique<base::AutoLock>(lock_);

  // Iterate the extensions based on install time (most-recently installed
  // items first).
  for (const auto& entry : entries_) {
    if (!entry->enabled)
      continue;

    std::unique_ptr<RuleIterator> rule_it;
    if (incognito) {
      rule_it =
          entry->incognito_session_only_settings.GetRuleIterator(type, nullptr);
      if (rule_it)
        iterators.push_back(std::move(rule_it));
      rule_it =
          entry->incognito_persistent_settings.GetRuleIterator(type, nullptr);
      if (rule_it)
        iterators.push_back(std::move(rule_it));
    } else {
      rule_it = entry->settings.GetRuleIterator(type, nullptr);
      if (rule_it)
        iterators.push_back(std::move(rule_it));
    }
  }
  if (iterators.empty())
    return nullptr;

  return std::make_unique<ConcatenationIterator>(std::move(iterators),
                                                 auto_lock.release());
}

void ContentSettingsStore::SetExtensionContentSetting(
    const std::string& ext_id,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    ContentSetting setting,
    ExtensionPrefsScope scope) {
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    if (setting == CONTENT_SETTING_DEFAULT) {
      map->DeleteValue(primary_pattern, secondary_pattern, type);
    } else {
      // Do not set a timestamp for extension settings.
      map->SetValue(primary_pattern, secondary_pattern, type,
                    base::Value(setting), {});
    }
  }

  // Send notification that content settings changed. (Note: This is responsible
  // for updating the pref store, so cannot be skipped even if the setting would
  // be masked by another extension.)
  NotifyOfContentSettingChanged(ext_id,
                                scope != kExtensionPrefsScopeRegular);
}

void ContentSettingsStore::RegisterExtension(
    const std::string& ext_id,
    const base::Time& install_time,
    bool is_enabled) {
  base::AutoLock lock(lock_);
  auto i = FindIterator(ext_id);
  ExtensionEntry* entry = nullptr;
  if (i != entries_.end()) {
    entry = i->get();
  } else {
    entry = new ExtensionEntry;
    entry->install_time = install_time;

    // Insert in reverse-chronological order to maintain the invariant.
    auto unique_entry = base::WrapUnique(entry);
    auto location =
        std::upper_bound(entries_.begin(), entries_.end(), unique_entry,
                         [](const std::unique_ptr<ExtensionEntry>& a,
                            const std::unique_ptr<ExtensionEntry>& b) {
                           return a->install_time > b->install_time;
                         });
    entries_.insert(location, std::move(unique_entry));
  }

  entry->id = ext_id;
  entry->enabled = is_enabled;
}

void ContentSettingsStore::UnregisterExtension(
    const std::string& ext_id) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    auto i = FindIterator(ext_id);
    if (i == entries_.end())
      return;
    notify = !(*i)->settings.empty();
    notify_incognito = !(*i)->incognito_persistent_settings.empty() ||
                       !(*i)->incognito_session_only_settings.empty();

    entries_.erase(i);
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

void ContentSettingsStore::SetExtensionState(
    const std::string& ext_id, bool is_enabled) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntry* entry = FindEntry(ext_id);
    if (!entry)
      return;

    notify = !entry->settings.empty();
    notify_incognito = !entry->incognito_persistent_settings.empty() ||
                       !entry->incognito_session_only_settings.empty();

    entry->enabled = is_enabled;
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

OriginIdentifierValueMap* ContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  const OriginIdentifierValueMap* result =
      static_cast<const ContentSettingsStore*>(this)->GetValueMap(ext_id,
                                                                  scope);
  return const_cast<OriginIdentifierValueMap*>(result);
}

const OriginIdentifierValueMap* ContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) const {
  ExtensionEntry* entry = FindEntry(ext_id);
  if (!entry)
    return nullptr;

  switch (scope) {
    case kExtensionPrefsScopeRegular:
      return &(entry->settings);
    case kExtensionPrefsScopeRegularOnly:
      // TODO(bauerb): Implement regular-only content settings.
      NOTREACHED();
      return nullptr;
    case kExtensionPrefsScopeIncognitoPersistent:
      return &(entry->incognito_persistent_settings);
    case kExtensionPrefsScopeIncognitoSessionOnly:
      return &(entry->incognito_session_only_settings);
  }

  NOTREACHED();
  return nullptr;
}

void ContentSettingsStore::ClearContentSettingsForExtension(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  bool notify = false;
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    DCHECK(map);
    notify = !map->empty();
    map->clear();
  }
  if (notify) {
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
  }
}

void ContentSettingsStore::ClearContentSettingsForExtensionAndContentType(
    const std::string& ext_id,
    ExtensionPrefsScope scope,
    ContentSettingsType content_type) {
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    DCHECK(map);

    if (map->find(content_type) == map->end())
      return;

    map->DeleteValues(content_type);
  }
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
}

base::Value::List ContentSettingsStore::GetSettingsForExtension(
    const std::string& extension_id,
    ExtensionPrefsScope scope) const {
  base::AutoLock lock(lock_);
  const OriginIdentifierValueMap* map = GetValueMap(extension_id, scope);
  if (!map)
    return {};

  base::Value::List settings;
  for (const auto& it : *map) {
    const auto& key = it.first;
    std::unique_ptr<RuleIterator> rule_iterator(
        map->GetRuleIterator(key,
                             nullptr));  // We already hold the lock.
    if (!rule_iterator)
      continue;

    while (rule_iterator->HasNext()) {
      const Rule& rule = rule_iterator->Next();
      base::Value::Dict setting_dict;
      setting_dict.Set(kPrimaryPatternKey, rule.primary_pattern.ToString());
      setting_dict.Set(kSecondaryPatternKey, rule.secondary_pattern.ToString());
      setting_dict.Set(
          kContentSettingsTypeKey,
          content_settings_helpers::ContentSettingsTypeToString(key));
      ContentSetting content_setting =
          content_settings::ValueToContentSetting(rule.value);
      DCHECK_NE(CONTENT_SETTING_DEFAULT, content_setting);

      std::string setting_string =
          content_settings::ContentSettingToString(content_setting);
      DCHECK(!setting_string.empty());

      setting_dict.Set(kContentSettingKey, setting_string);
      settings.Append(std::move(setting_dict));
    }
  }
  return settings;
}

#define LOG_INVALID_EXTENSION_PREFERENCE_DETAILS          \
  LOG(ERROR) << "Found invalid extension pref: " << value \
             << " extension id: " << extension_id

void ContentSettingsStore::SetExtensionContentSettingFromList(
    const std::string& extension_id,
    const base::Value::List& list,
    ExtensionPrefsScope scope) {
  for (const base::Value& value : list) {
    if (!value.is_dict()) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }

    const base::Value::Dict& dict = value.GetDict();
    const std::string* primary_pattern_str =
        dict.FindString(kPrimaryPatternKey);
    if (!primary_pattern_str) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(*primary_pattern_str);
    if (!primary_pattern.IsValid()) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }

    const std::string* secondary_pattern_str =
        dict.FindString(kSecondaryPatternKey);
    if (!secondary_pattern_str) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::FromString(*secondary_pattern_str);
    if (!secondary_pattern.IsValid()) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }

    auto* content_settings_type_str = dict.FindString(kContentSettingsTypeKey);
    if (!content_settings_type_str) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }
    ContentSettingsType content_settings_type =
        content_settings_helpers::StringToContentSettingsType(
            *content_settings_type_str);
    if (content_settings_type == ContentSettingsType::DEFAULT) {
      // We'll end up with DEFAULT here if the type string isn't recognised.
      // This could be if it's a string from an old settings type that has been
      // deleted. DCHECK to make sure this is the case (not some random string).
      DCHECK(*content_settings_type_str == "fullscreen" ||
             *content_settings_type_str == "mouselock" ||
             *content_settings_type_str == "plugins");

      // In this case, we just skip over that setting, effectively deleting it
      // from the in-memory model. This will implicitly delete these old
      // settings from the pref store when it is written back.
      continue;
    }

    const content_settings::ContentSettingsInfo* info =
        content_settings::ContentSettingsRegistry::GetInstance()->Get(
            content_settings_type);

    if (secondary_pattern == primary_pattern &&
        info->website_settings_info()->SupportsSecondaryPattern()) {
      secondary_pattern = ContentSettingsPattern::Wildcard();
    }

    if (primary_pattern != secondary_pattern &&
        secondary_pattern != ContentSettingsPattern::Wildcard() &&
        !info->website_settings_info()->SupportsSecondaryPattern()) {
      // Some types may have had embedded exceptions written even though they
      // aren't supported. This will implicitly delete these old settings from
      // the pref store when it is written back.
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }

    ContentSetting setting;
    const std::string* content_setting_str =
        dict.FindString(kContentSettingKey);
    if (!content_setting_str) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }

    bool result = content_settings::ContentSettingFromString(
        *content_setting_str, &setting);
    // The content settings extensions API does not support setting any content
    // settings to |CONTENT_SETTING_DEFAULT|.
    if (!result != CONTENT_SETTING_DEFAULT) {
      LOG_INVALID_EXTENSION_PREFERENCE_DETAILS;
      continue;
    }

    SetExtensionContentSetting(extension_id, primary_pattern, secondary_pattern,
                               content_settings_type, setting, scope);
  }
}

#undef LOG_INVALID_EXTENSION_PREFERENCE_DETAILS

void ContentSettingsStore::AddObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.AddObserver(observer);
}

void ContentSettingsStore::RemoveObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.RemoveObserver(observer);
}

void ContentSettingsStore::NotifyOfContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  for (auto& observer : observers_)
    observer.OnContentSettingChanged(extension_id, incognito);
}

bool ContentSettingsStore::OnCorrectThread() {
  // If there is no UI thread, we're most likely in a unit test.
  return !BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI);
}

ContentSettingsStore::ExtensionEntry* ContentSettingsStore::FindEntry(
    const std::string& ext_id) const {
  auto iter = base::ranges::find(entries_, ext_id, &ExtensionEntry::id);
  return iter == entries_.end() ? nullptr : iter->get();
}

ContentSettingsStore::ExtensionEntries::iterator
ContentSettingsStore::FindIterator(const std::string& ext_id) {
  return base::ranges::find(entries_, ext_id, &ExtensionEntry::id);
}

}  // namespace extensions
