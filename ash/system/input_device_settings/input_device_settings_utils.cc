// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include "ash/public/cpp/accelerators_util.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/export_template.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ozone/evdev/keyboard_mouse_combo_device_metrics.h"

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

constexpr bool VendorProductId::operator<(const VendorProductId& other) const {
  return vendor_id == other.vendor_id ? product_id < other.product_id
                                      : vendor_id < other.vendor_id;
}

bool VendorProductId::operator==(const VendorProductId& other) const {
  return vendor_id == other.vendor_id && product_id == other.product_id;
}

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

bool ShouldPersistFkeySetting(
    const mojom::InputDeviceSettingsFkeyPolicyPtr& policy,
    base::StringPiece setting_key,
    absl::optional<ui::mojom::ExtendedFkeysModifier> new_value,
    ui::mojom::ExtendedFkeysModifier default_value,
    const base::Value::Dict* existing_settings_dict) {
  if (!new_value.has_value()) {
    return false;
  }

  if (!policy) {
    return ShouldPersistSetting(setting_key, new_value.value(), default_value,
                                /*force_persistence=*/{},
                                existing_settings_dict);
  }

  switch (policy->policy_status) {
    case mojom::PolicyStatus::kRecommended:
      return ExistingSettingsHasValue(setting_key, existing_settings_dict) ||
             new_value.value() != policy->value;
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

const base::Value::List* GetLoginScreenButtonRemappingList(
    PrefService* local_state,
    AccountId account_id,
    const std::string& pref_name) {
  const auto* list_value =
      user_manager::KnownUser(local_state).FindPath(account_id, pref_name);
  if (!list_value || !list_value->is_list()) {
    return nullptr;
  }
  return &list_value->GetList();
}

bool IsMouseCustomizable(const ui::InputDevice& device) {
  // TODO(wangdanny): Update uncustomizable mice set with devices' vid and pid.
  static constexpr auto kUncustomizableMice =
      base::MakeFixedFlatSet<VendorProductId>({
          {0xffff, 0xffff},  // Fake data for testing.
      });
  return !kUncustomizableMice.contains({device.vendor_id, device.product_id});
}

bool IsKeyboardPretendingToBeMouse(const ui::InputDevice& device) {
  static base::NoDestructor<base::flat_set<VendorProductId>> logged_devices;
  static constexpr auto kKeyboardsPretendingToBeMice =
      base::MakeFixedFlatSet<VendorProductId>({
          {0x03f0, 0x1f41},  // HP OMEN Sequencer
          {0x045e, 0x082c},  // Microsoft Ergonomic Keyboard
          {0x046d, 0x4088},  // Logitech ERGO K860 (Bluetooth)
          {0x046d, 0x408a},  // Logitech MX Keys (Universal Receiver)
          {0x046d, 0xb350},  // Logitech Craft Keyboard
          {0x046d, 0xb359},  // Logitech ERGO K860
          {0x046d, 0xb35b},  // Logitech MX Keys (Bluetooth)
          {0x046d, 0xb35f},  // Logitech G915 TKL (Bluetooth)
          {0x046d, 0xb361},  // Logitech MX Keys for Mac (Bluetooth)
          {0x046d, 0xb364},  // Logitech ERGO 860B
          {0x046d, 0xc336},  // Logitech G213
          {0x046d, 0xc33f},  // Logitech G815 RGB
          {0x046d, 0xc343},  // Logitech G915 TKL (USB)
          {0x05ac, 0x024f},  // EGA MGK2 (Bluetooth) + Keychron K2
          {0x05ac, 0x0256},  // EGA MGK2 (USB)
          {0x0951, 0x16e5},  // HyperX Alloy Origins
          {0x0951, 0x16e6},  // HyperX Alloy Origins Core
          {0x1038, 0x1612},  // SteelSeries Apex 7
          {0x1065, 0x0002},  // SteelSeries Apex 3 TKL
          {0x1532, 0x022a},  // Razer Cynosa Chroma
          {0x1532, 0x025d},  // Razer Ornata V2
          {0x1532, 0x025e},  // Razer Cynosa V2
          {0x1532, 0x026b},  // Razer Huntsman V2 Tenkeyless
          {0x1535, 0x0046},  // Razer Huntsman Elite
          {0x1b1c, 0x1b2d},  // Corsair Gaming K95 RGB Platinum
          {0x28da, 0x1101},  // G.Skill KM780
          {0x29ea, 0x0102},  // Kinesis Freestyle Edge RGB
          {0x2f68, 0x0082},  // Durgod Taurus K320
          {0x320f, 0x5044},  // Glorious GMMK Pro
          {0x3297, 0x1969},  // ZSA Moonlander Mark I
          {0x3297, 0x4974},  // ErgoDox EZ
          {0x3297, 0x4976},  // ErgoDox EZ Glow
          {0x3434, 0x0121},  // Keychron Q3
          {0x3434, 0x0151},  // Keychron Q5
          {0x3434, 0x0163},  // Keychron Q6
          {0x3434, 0x01a1},  // Keychron Q10
          {0x3434, 0x0311},  // Keychron V1
          {0x3496, 0x0006},  // Keyboardio Model 100
          {0x4c44, 0x0040},  // LazyDesigners Dimple
          {0xfeed, 0x1307},  // ErgoDox EZ
      });

  if (kKeyboardsPretendingToBeMice.contains(
          {device.vendor_id, device.product_id})) {
    auto [iter, inserted] =
        logged_devices->insert({device.vendor_id, device.product_id});
    if (inserted) {
      logged_devices->insert({device.vendor_id, device.product_id});
      base::UmaHistogramEnumeration(
          "ChromeOS.Inputs.ComboDeviceClassification",
          ui::ComboDeviceClassification::kKnownMouseImposter);
    }
    return true;
  }

  return false;
}

base::Value::Dict ConvertButtonRemappingToDict(
    const mojom::ButtonRemapping& remapping,
    mojom::CustomizationRestriction customization_restriction) {
  base::Value::Dict dict;

  if (customization_restriction ==
          mojom::CustomizationRestriction::kDisallowCustomizations ||
      (remapping.button->is_vkey() &&
       customization_restriction ==
           mojom::CustomizationRestriction::kDisableKeyEventRewrites)) {
    return dict;
  }

  dict.Set(prefs::kButtonRemappingName, remapping.name);
  if (remapping.button->is_customizable_button()) {
    dict.Set(prefs::kButtonRemappingCustomizableButton,
             static_cast<int>(remapping.button->get_customizable_button()));
  } else if (remapping.button->is_vkey()) {
    dict.Set(prefs::kButtonRemappingKeyboardCode,
             static_cast<int>(remapping.button->get_vkey()));
  }

  if (!remapping.remapping_action) {
    return dict;
  }
  if (remapping.remapping_action->is_key_event()) {
    base::Value::Dict key_event;
    key_event.Set(prefs::kButtonRemappingDomCode,
                  static_cast<int>(
                      remapping.remapping_action->get_key_event()->dom_code));
    key_event.Set(
        prefs::kButtonRemappingDomKey,
        static_cast<int>(remapping.remapping_action->get_key_event()->dom_key));
    key_event.Set(prefs::kButtonRemappingModifiers,
                  static_cast<int>(
                      remapping.remapping_action->get_key_event()->modifiers));
    key_event.Set(
        prefs::kButtonRemappingKeyboardCode,
        static_cast<int>(remapping.remapping_action->get_key_event()->vkey));
    dict.Set(prefs::kButtonRemappingKeyEvent, std::move(key_event));
  } else if (remapping.remapping_action->is_accelerator_action()) {
    dict.Set(
        prefs::kButtonRemappingAcceleratorAction,
        static_cast<int>(remapping.remapping_action->get_accelerator_action()));
  } else if (remapping.remapping_action->is_static_shortcut_action()) {
    dict.Set(prefs::kButtonRemappingStaticShortcutAction,
             static_cast<int>(
                 remapping.remapping_action->get_static_shortcut_action()));
  }

  return dict;
}

base::Value::List ConvertButtonRemappingArrayToList(
    const std::vector<mojom::ButtonRemappingPtr>& remappings,
    mojom::CustomizationRestriction customization_restriction) {
  base::Value::List list;
  for (const auto& remapping : remappings) {
    base::Value::Dict dict =
        ConvertButtonRemappingToDict(*remapping, customization_restriction);
    // Remove empty dicts.
    if (dict.empty()) {
      continue;
    }

    list.Append(std::move(dict));
  }
  return list;
}

std::vector<mojom::ButtonRemappingPtr> ConvertListToButtonRemappingArray(
    const base::Value::List& list,
    mojom::CustomizationRestriction customization_restriction) {
  std::vector<mojom::ButtonRemappingPtr> array;
  for (const auto& element : list) {
    if (!element.is_dict()) {
      continue;
    }
    const auto& dict = element.GetDict();
    auto remapping =
        ConvertDictToButtonRemapping(dict, customization_restriction);
    if (remapping) {
      array.push_back(std::move(remapping));
    }
  }
  return array;
}

mojom::ButtonRemappingPtr ConvertDictToButtonRemapping(
    const base::Value::Dict& dict,
    mojom::CustomizationRestriction customization_restriction) {
  if (customization_restriction ==
      mojom::CustomizationRestriction::kDisallowCustomizations) {
    return nullptr;
  }

  const std::string* name = dict.FindString(prefs::kButtonRemappingName);
  if (!name) {
    return nullptr;
  }

  // button is a union.
  mojom::ButtonPtr button;
  const absl::optional<int> customizable_button =
      dict.FindInt(prefs::kButtonRemappingCustomizableButton);
  const absl::optional<int> key_code =
      dict.FindInt(prefs::kButtonRemappingKeyboardCode);
  // Button can't be both a keyboard key and a customization button.
  if (customizable_button && key_code) {
    return nullptr;
  }
  // Button must exist.
  if (!customizable_button && !key_code) {
    return nullptr;
  }
  // Button can be either a keyboard key or a customization button. If
  // the customization_restriction is not kDisableKeyEventRewrites,
  // the button is allowed to be a keyboard key.
  if (customizable_button) {
    button = mojom::Button::NewCustomizableButton(
        static_cast<mojom::CustomizableButton>(*customizable_button));
  } else if (key_code &&
             customization_restriction !=
                 mojom::CustomizationRestriction::kDisableKeyEventRewrites) {
    button = mojom::Button::NewVkey(static_cast<::ui::KeyboardCode>(*key_code));
  } else {
    return nullptr;
  }

  // remapping_action is an optional union.
  mojom::RemappingActionPtr remapping_action;
  const base::Value::Dict* key_event =
      dict.FindDict(prefs::kButtonRemappingKeyEvent);
  const absl::optional<int> accelerator_action =
      dict.FindInt(prefs::kButtonRemappingAcceleratorAction);
  const absl::optional<int> static_shortcut_action =
      dict.FindInt(prefs::kButtonRemappingStaticShortcutAction);
  // Remapping action can only have one value at most.
  if ((key_event && accelerator_action) ||
      (key_event && static_shortcut_action) ||
      (accelerator_action && static_shortcut_action)) {
    return nullptr;
  }
  // Remapping action can be either a keyboard event or an accelerator action
  // or static shortcut action or null.
  if (key_event) {
    const absl::optional<int> dom_code =
        key_event->FindInt(prefs::kButtonRemappingDomCode);
    const absl::optional<int> vkey =
        key_event->FindInt(prefs::kButtonRemappingKeyboardCode);
    const absl::optional<int> dom_key =
        key_event->FindInt(prefs::kButtonRemappingDomKey);
    const absl::optional<int> modifiers =
        key_event->FindInt(prefs::kButtonRemappingModifiers);
    if (!dom_code || !vkey || !dom_key || !modifiers) {
      return nullptr;
    }
    ui::KeyboardCode vkey_value = static_cast<ui::KeyboardCode>(*vkey);
    remapping_action = mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(vkey_value, *dom_code, *dom_key, *modifiers,
                             base::UTF16ToUTF8(GetKeyDisplay(vkey_value))));
  } else if (accelerator_action) {
    remapping_action = mojom::RemappingAction::NewAcceleratorAction(
        static_cast<ash::AcceleratorAction>(*accelerator_action));
  } else if (static_shortcut_action) {
    remapping_action = mojom::RemappingAction::NewStaticShortcutAction(
        static_cast<mojom::StaticShortcutAction>(*static_shortcut_action));
  }

  return mojom::ButtonRemapping::New(*name, std::move(button),
                                     std::move(remapping_action));
}

bool IsChromeOSKeyboard(const mojom::Keyboard& keyboard) {
  return keyboard.meta_key == mojom::MetaKey::kLauncher ||
         keyboard.meta_key == mojom::MetaKey::kSearch;
}

}  // namespace ash
