// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/export_template.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"

namespace ash {
namespace {

std::string HexEncode(uint16_t v) {
  // Load the bytes into the bytes array in reverse order as hex number should
  // be read from left to right.
  uint8_t bytes[sizeof(uint16_t)];
  bytes[1] = v & 0xFF;
  bytes[0] = v >> 8;
  return base::ToLowerASCII(base::HexEncode(bytes));
}

bool ExistingSettingsHasValue(base::StringPiece setting_key,
                              const base::Value::Dict* existing_settings_dict) {
  if (!existing_settings_dict) {
    return false;
  }

  return existing_settings_dict->Find(setting_key) != nullptr;
}

}  // namespace

// `kIsoLevel5ShiftMod3` is not a valid modifier value.
bool IsValidModifier(int val) {
  return val >= static_cast<int>(ui::mojom::ModifierKey::kMinValue) &&
         val <= static_cast<int>(ui::mojom::ModifierKey::kMaxValue) &&
         val != static_cast<int>(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3);
}

std::string BuildDeviceKey(const ui::InputDevice& device) {
  return base::StrCat(
      {HexEncode(device.vendor_id), ":", HexEncode(device.product_id)});
}

template <typename T>
bool ShouldPersistSetting(base::StringPiece setting_key,
                          T new_value,
                          T default_value,
                          bool force_persistence,
                          const base::Value::Dict* existing_settings_dict) {
  return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
         new_value != default_value || force_persistence;
}

bool ShouldPersistSetting(const mojom::InputDeviceSettingsPolicyPtr& policy,
                          base::StringPiece setting_key,
                          bool new_value,
                          bool default_value,
                          bool force_persistence,
                          const base::Value::Dict* existing_settings_dict) {
  if (force_persistence) {
    return true;
  }

  if (!policy) {
    return ShouldPersistSetting(setting_key, new_value, default_value,
                                force_persistence, existing_settings_dict);
  }

  switch (policy->policy_status) {
    case mojom::PolicyStatus::kRecommended:
      return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
             new_value != policy->value;
    case mojom::PolicyStatus::kManaged:
      return false;
  }
}

template EXPORT_TEMPLATE_DEFINE(ASH_EXPORT) bool ShouldPersistSetting(
    base::StringPiece setting_key,
    bool new_value,
    bool default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

template EXPORT_TEMPLATE_DEFINE(ASH_EXPORT) bool ShouldPersistSetting(
    base::StringPiece setting_key,
    int value,
    int default_value,
    bool force_persistence,
    const base::Value::Dict* existing_settings_dict);

const base::Value::Dict* GetLoginScreenSettingsDict(
    PrefService* local_state,
    AccountId account_id,
    const std::string& pref_name) {
  const auto* dict_value =
      user_manager::KnownUser(local_state).FindPath(account_id, pref_name);
  if (!dict_value || !dict_value->is_dict()) {
    return nullptr;
  }
  return &dict_value->GetDict();
}

}  // namespace ash
