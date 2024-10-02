// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_pref.h"

#include "base/observer_list.h"
#include "chrome/common/extensions/api/settings_private.h"

namespace settings_api = extensions::api::settings_private;

namespace extensions {
namespace settings_private {

GeneratedPref::Observer::Observer() = default;
GeneratedPref::Observer::~Observer() = default;

GeneratedPref::GeneratedPref() = default;
GeneratedPref::~GeneratedPref() = default;

void GeneratedPref::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GeneratedPref::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GeneratedPref::NotifyObservers(const std::string& pref_name) {
  for (Observer& observer : observers_)
    observer.OnGeneratedPrefChanged(pref_name);
}

/* static */
void GeneratedPref::ApplyControlledByFromPref(
    api::settings_private::PrefObject* pref_object,
    const PrefService::Preference* pref) {
  if (pref->IsManaged()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    return;
  }

  if (pref->IsExtensionControlled()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    return;
  }

  if (pref->IsManagedByCustodian()) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    return;
  }

  NOTREACHED();
}

/* static */
void GeneratedPref::ApplyControlledByFromContentSettingSource(
    api::settings_private::PrefObject* pref_object,
    content_settings::SettingSource setting_source) {
  switch (setting_source) {
    case content_settings::SETTING_SOURCE_POLICY:
      pref_object->controlled_by =
          settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
      break;
    case content_settings::SETTING_SOURCE_EXTENSION:
      pref_object->controlled_by =
          settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
      break;
    case content_settings::SETTING_SOURCE_SUPERVISED:
      pref_object->controlled_by =
          settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
      break;
    default:
      NOTREACHED();
  }
}

/* static */
void GeneratedPref::AddUserSelectableValue(
    settings_api::PrefObject* pref_object,
    int value) {
  if (!pref_object->user_selectable_values) {
    pref_object->user_selectable_values.emplace();
  }
  pref_object->user_selectable_values->push_back(base::Value(value));
}

}  // namespace settings_private
}  // namespace extensions
