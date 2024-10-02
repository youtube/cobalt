// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/event_rewriter_ash.h"

#include <fcntl.h>
#include <stddef.h>
#include <cstdint>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "device/udev_linux/scoped_udev.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/types/event_type.h"

namespace ui {

namespace {

// Flag masks for remapping alt+click or search+click to right click.
constexpr int kAltLeftButton = (EF_ALT_DOWN | EF_LEFT_MOUSE_BUTTON);
constexpr int kSearchLeftButton = (EF_COMMAND_DOWN | EF_LEFT_MOUSE_BUTTON);

// Index of the remapped flags in the auto repeat usage metric.
enum class AutoRepeatUsageModifierFlag : uint32_t {
  kMeta = 0,
  kControl = 1,
  kAlt = 2,
  kShift = 3,
  kCapsLock = 4,
  kNumModifierFlags
};

// Mapping between event flag to `AutoRepeatUsageModifierFlag` index for the
// auto repeat usage metric.
constexpr struct AutoRepeatUsageModifierMapping {
  int event_flag;
  AutoRepeatUsageModifierFlag auto_repeat_flag;
} kEventFlagToAutoRepeatMetricFlag[] = {
    {EF_COMMAND_DOWN, AutoRepeatUsageModifierFlag::kMeta},
    {EF_CONTROL_DOWN, AutoRepeatUsageModifierFlag::kControl},
    {EF_ALT_DOWN, AutoRepeatUsageModifierFlag::kAlt},
    {EF_SHIFT_DOWN, AutoRepeatUsageModifierFlag::kShift},
    {EF_CAPS_LOCK_ON, AutoRepeatUsageModifierFlag::kCapsLock},
};

// Amount to shift the bitset of modifiers to so they are left aligned at the
// top of the 32 bit int.
constexpr int kAutoRepeatUsageAmountToShiftModifierFlags =
    ((sizeof(uint32_t) * 8) -
     static_cast<int>(AutoRepeatUsageModifierFlag::kNumModifierFlags));

// Number of bits reserved for potential new keyboard codes in the future
// without breaking the auto repeat usage metric.
constexpr int kAutoRepeatUsageNumBitsReservedForKeyboardCode = 16;
static_assert(kAutoRepeatUsageAmountToShiftModifierFlags >=
              kAutoRepeatUsageNumBitsReservedForKeyboardCode);

// Records the usage of auto repeat in a sparse histogram. Keeps track of the
// keyboard code + the modifier flags.
// To decode:
// Top `AutoRepeatUsageModifierFlag::kNumModifierFlags` bits are used to denote
// modifier flags. See `AutoRepeatUsageModifierFlag` to decode the top bits.
// The rest of the bits are used to store the `KeyboardCode` for the repeated
// keypress. Currently, `kAutoRepeatUsageNumBitsReservedForKeyboardCode` are
// reserved to allow the number of `KeyboardCode` values to expand in the future
// without breaking this metric.
void RecordAutoRepeatUsageMetric(
    const KeyEvent& key_event,
    const std::unique_ptr<Event>& rewritten_event) {
  // Use original event if the event has not been rewritten.
  const KeyEvent* auto_repeat_event = &key_event;
  if (rewritten_event) {
    auto_repeat_event = rewritten_event.get()->AsKeyEvent();
  }

  // Only want to record metrics if its a repeated keypressed event.
  if (!(auto_repeat_event->flags() & EF_IS_REPEAT) ||
      !(auto_repeat_event->type() & ET_KEY_PRESSED)) {
    return;
  }

  // Apply remapped auto repeat event flags.
  uint32_t auto_repeat_usage_modifier_flags = 0;
  for (const auto& [event_flag, auto_repeat_flag] :
       kEventFlagToAutoRepeatMetricFlag) {
    if (auto_repeat_event->flags() & event_flag) {
      auto_repeat_usage_modifier_flags |=
          (1u << static_cast<uint32_t>(auto_repeat_flag));
    }
  }

  UMA_HISTOGRAM_SPARSE("ChromeOS.Inputs.AutoRepeatUsage",
                       ((auto_repeat_usage_modifier_flags
                         << kAutoRepeatUsageAmountToShiftModifierFlags) +
                        auto_repeat_event->key_code()));
}

using ModifierKeyUsageMetric = EventRewriterAsh::ModifierKeyUsageMetric;
constexpr struct ModifierKeyUsageMapping {
  DomCode code;
  ModifierKeyUsageMetric modifier_key_enum;
} modifier_key_usage_mappings[] = {
    {DomCode::CONTROL_LEFT, ModifierKeyUsageMetric::kControlLeft},
    {DomCode::CONTROL_RIGHT, ModifierKeyUsageMetric::kControlRight},
    {DomCode::META_LEFT, ModifierKeyUsageMetric::kMetaLeft},
    {DomCode::META_RIGHT, ModifierKeyUsageMetric::kMetaRight},
    {DomCode::ALT_LEFT, ModifierKeyUsageMetric::kAltLeft},
    {DomCode::ALT_RIGHT, ModifierKeyUsageMetric::kAltRight},
    {DomCode::SHIFT_LEFT, ModifierKeyUsageMetric::kShiftLeft},
    {DomCode::SHIFT_RIGHT, ModifierKeyUsageMetric::kShiftRight},
    {DomCode::BACKSPACE, ModifierKeyUsageMetric::kBackspace},
    {DomCode::ESCAPE, ModifierKeyUsageMetric::kEscape},
    {DomCode::CAPS_LOCK, ModifierKeyUsageMetric::kCapsLock},
    {DomCode::LAUNCH_ASSISTANT, ModifierKeyUsageMetric::kAssistant}};

// Table of properties of remappable keys and/or remapping targets (not
// strictly limited to "modifiers").
//
// This is used in two distinct ways: for rewriting key up/down events,
// and for rewriting modifier EventFlags on any kind of event.
//
// For the first case, rewriting key up/down events, |RewriteModifierKeys()|
// determines the preference name |prefs::kLanguageRemap...KeyTo| for the
// incoming key and, using |GetRemappedKey()|, gets the user preference
// value |input_method::k...Key| for the incoming key, and finally finds that
// value in this table to obtain the |result| properties of the target key.
//
// For the second case, rewriting modifier EventFlags,
// |GetRemappedModifierMasks()| processes every table entry whose |flag|
// is set in the incoming event. Using the |pref_name| in the table entry,
// it likewise uses |GetRemappedKey()| to find the properties of the
// user preference target key, and replaces the flag accordingly.
constexpr struct ModifierRemapping {
  int flag;
  ui::mojom::ModifierKey remap_to;
  const char* pref_name;
  EventRewriterAsh::MutableKeyState result;
} kModifierRemappings[] = {
    {EF_CONTROL_DOWN,
     ui::mojom::ModifierKey::kControl,
     prefs::kLanguageRemapControlKeyTo,
     {EF_CONTROL_DOWN, DomCode::CONTROL_LEFT, DomKey::CONTROL, VKEY_CONTROL}},
    {EF_MOD3_DOWN | EF_ALTGR_DOWN,
     mojom::ModifierKey::kIsoLevel5ShiftMod3,
     nullptr,
     {EF_MOD3_DOWN | EF_ALTGR_DOWN, DomCode::CAPS_LOCK, DomKey::ALT_GRAPH,
      VKEY_ALTGR}},
    {EF_COMMAND_DOWN,
     ui::mojom::ModifierKey::kMeta,
     prefs::kLanguageRemapSearchKeyTo,
     {EF_COMMAND_DOWN, DomCode::META_LEFT, DomKey::META, VKEY_LWIN}},
    {EF_ALT_DOWN,
     ui::mojom::ModifierKey::kAlt,
     prefs::kLanguageRemapAltKeyTo,
     {EF_ALT_DOWN, DomCode::ALT_LEFT, DomKey::ALT, VKEY_MENU}},
    {EF_NONE,
     ui::mojom::ModifierKey::kVoid,
     nullptr,
     {EF_NONE, DomCode::NONE, DomKey::NONE, VKEY_UNKNOWN}},
    {EF_MOD3_DOWN,
     ui::mojom::ModifierKey::kCapsLock,
     prefs::kLanguageRemapCapsLockKeyTo,
     {EF_MOD3_DOWN, DomCode::CAPS_LOCK, DomKey::CAPS_LOCK, VKEY_CAPITAL}},
    {EF_NONE,
     ui::mojom::ModifierKey::kEscape,
     prefs::kLanguageRemapEscapeKeyTo,
     {EF_NONE, DomCode::ESCAPE, DomKey::ESCAPE, VKEY_ESCAPE}},
    {EF_NONE,
     ui::mojom::ModifierKey::kBackspace,
     prefs::kLanguageRemapBackspaceKeyTo,
     {EF_NONE, DomCode::BACKSPACE, DomKey::BACKSPACE, VKEY_BACK}},
    {EF_NONE,
     ui::mojom::ModifierKey::kAssistant,
     prefs::kLanguageRemapAssistantKeyTo,
     {EF_NONE, DomCode::LAUNCH_ASSISTANT, DomKey::LAUNCH_ASSISTANT,
      VKEY_ASSISTANT}}};

// Finds the remapping for Neo Mod3 in the list. Used only to set the value of
// |kModifierRemappingIsoLevel5ShiftMod3|.
constexpr const ModifierRemapping* GetModifierRemappingNeoMod3() {
  for (auto& remapping : kModifierRemappings) {
    if (remapping.remap_to == mojom::ModifierKey::kIsoLevel5ShiftMod3) {
      return &remapping;
    }
  }
  return nullptr;
}
constexpr const ModifierRemapping* kModifierRemappingIsoLevel5ShiftMod3 =
    GetModifierRemappingNeoMod3();
static_assert(kModifierRemappingIsoLevel5ShiftMod3 != nullptr);

const EventRewriterAsh::MutableKeyState kCustomTopRowLayoutFKeys[] = {
    {EF_NONE, DomCode::F1, DomKey::F1, VKEY_F1},
    {EF_NONE, DomCode::F2, DomKey::F2, VKEY_F2},
    {EF_NONE, DomCode::F3, DomKey::F3, VKEY_F3},
    {EF_NONE, DomCode::F4, DomKey::F4, VKEY_F4},
    {EF_NONE, DomCode::F5, DomKey::F5, VKEY_F5},
    {EF_NONE, DomCode::F6, DomKey::F6, VKEY_F6},
    {EF_NONE, DomCode::F7, DomKey::F7, VKEY_F7},
    {EF_NONE, DomCode::F8, DomKey::F8, VKEY_F8},
    {EF_NONE, DomCode::F9, DomKey::F9, VKEY_F9},
    {EF_NONE, DomCode::F10, DomKey::F10, VKEY_F10},
    {EF_NONE, DomCode::F11, DomKey::F11, VKEY_F11},
    {EF_NONE, DomCode::F12, DomKey::F12, VKEY_F12},
    {EF_NONE, DomCode::F13, DomKey::F13, VKEY_F13},
    {EF_NONE, DomCode::F14, DomKey::F14, VKEY_F14},
    {EF_NONE, DomCode::F15, DomKey::F15, VKEY_F15},
};
constexpr KeyboardCode kMaxCustomTopRowLayoutFKeyCode = VKEY_F15;

bool IsCustomLayoutFunctionKey(KeyboardCode key_code) {
  return key_code >= VKEY_F1 && key_code <= kMaxCustomTopRowLayoutFKeyCode;
}

// Gets a remapped key for |pref_name| key or |modifier_key| and |device_id| if
// per-device settings are enabled. For example, to find out which key Ctrl is
// currently remapped to, call the function with
// prefs::kLanguageRemapControlKeyTo, mojom::ModifierKey::kControl, and the
// device id.
// Note: For the Search key, call GetSearchRemappedKey().
const ModifierRemapping* GetRemappedKey(int device_id,
                                        mojom::ModifierKey modifier_key,
                                        const std::string& pref_name,
                                        EventRewriterAsh::Delegate* delegate) {
  if (!delegate) {
    return nullptr;
  }

  const auto remapped_modifier_key = delegate->GetKeyboardRemappedModifierValue(
      device_id, modifier_key, pref_name);
  if (!remapped_modifier_key) {
    return nullptr;
  }

  for (auto& remapping : kModifierRemappings) {
    if (remapped_modifier_key.value() == remapping.remap_to) {
      return &remapping;
    }
  }

  return nullptr;
}

// Gets a remapped key for the Search key based on the |keyboard_type| of the
// last event. Internal Search key, Command key on external Apple keyboards, and
// Meta key (either Search or Windows) on external non-Apple keyboards can all
// be remapped separately.
const ModifierRemapping* GetSearchRemappedKey(
    EventRewriterAsh::Delegate* delegate,
    int device_id,
    KeyboardCapability::DeviceType keyboard_type) {
  std::string pref_name;
  switch (keyboard_type) {
    case KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      pref_name = prefs::kLanguageRemapExternalCommandKeyTo;
      break;

    case KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case KeyboardCapability::DeviceType::kDeviceExternalUnknown:
      pref_name = prefs::kLanguageRemapExternalMetaKeyTo;
      break;

    case KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
    case KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
    case KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
    case KeyboardCapability::DeviceType::kDeviceUnknown:
      // Use the preference for internal Search key remapping.
      pref_name = prefs::kLanguageRemapSearchKeyTo;
      break;
  }

  return GetRemappedKey(device_id, mojom::ModifierKey::kMeta, pref_name,
                        delegate);
}

bool ShouldRewriteMetaTopRowKeyComboEvents(EventRewriterAsh::Delegate* delegate,
                                           int device_id) {
  return delegate && delegate->RewriteMetaTopRowKeyComboEvents(device_id);
}

bool IsISOLevel5ShiftUsedByCurrentInputMethod() {
  // Since both German Neo2 XKB layout and Caps Lock depend on Mod3Mask,
  // it's not possible to make both features work. For now, we don't remap
  // Mod3Mask when Neo2 is in use.
  // TODO(yusukes): Remove the restriction.
  auto* manager = ash::input_method::InputMethodManager::Get();
  return manager->IsISOLevel5ShiftUsedByCurrentInputMethod();
}

struct KeyboardRemapping {
  // MatchKeyboardRemapping() succeeds if the tested has all of the specified
  // flags (and possibly other flags), and either the key_code matches or the
  // condition's key_code is VKEY_UNKNOWN.
  struct Condition {
    int flags;
    KeyboardCode key_code;
  } condition;
  // ApplyRemapping(), which is the primary user of this structure,
  // conditionally sets the output fields from the |result| here.
  // - |dom_code| is set if |result.dom_code| is not NONE.
  // - |dom_key| and |character| are set if |result.dom_key| is not NONE.
  // -|key_code| is set if |result.key_code| is not VKEY_UNKNOWN.
  // - |flags| are always set from |result.flags|, but this can be |EF_NONE|.
  EventRewriterAsh::MutableKeyState result;
};

// If |strict| is true, the flags must match exactly the same. In other words,
// the event will be rewritten only if the exactly specified modifier is
// pressed.  If false, it can match even if other modifiers are pressed.
bool MatchKeyboardRemapping(const EventRewriterAsh::MutableKeyState& suspect,
                            const KeyboardRemapping::Condition& test,
                            bool strict = false) {
  // Reset non modifier key event related flags for strict mode.
  constexpr int kKeyEventModifiersMask = EF_SHIFT_DOWN | EF_CONTROL_DOWN |
                                         EF_ALT_DOWN | EF_COMMAND_DOWN |
                                         EF_ALTGR_DOWN | EF_MOD3_DOWN;

  const int suspect_flags_for_strict = suspect.flags & kKeyEventModifiersMask;
  const bool flag_matched = strict
                                ? suspect_flags_for_strict == test.flags
                                : ((suspect.flags & test.flags) == test.flags);
  return flag_matched && ((test.key_code == VKEY_UNKNOWN) ||
                          (test.key_code == suspect.key_code));
}

void ApplyRemapping(const EventRewriterAsh::MutableKeyState& changes,
                    EventRewriterAsh::MutableKeyState* state) {
  state->flags |= changes.flags;
  if (changes.code != DomCode::NONE) {
    state->code = changes.code;
  }
  if (changes.key != DomKey::NONE) {
    state->key = changes.key;
  }
  if (changes.key_code != VKEY_UNKNOWN) {
    state->key_code = changes.key_code;
  }
}

// Given a set of KeyboardRemapping structs, finds a matching struct
// if possible, and updates the remapped event values. Returns true if a
// remapping was found and remapped values were updated.
// See MatchKeyboardRemapping() for |strict|.
bool RewriteWithKeyboardRemappings(
    const KeyboardRemapping* mappings,
    size_t num_mappings,
    const EventRewriterAsh::MutableKeyState& input_state,
    EventRewriterAsh::MutableKeyState* remapped_state,
    bool strict = false) {
  for (size_t i = 0; i < num_mappings; ++i) {
    const KeyboardRemapping& map = mappings[i];
    if (MatchKeyboardRemapping(input_state, map.condition, strict)) {
      remapped_state->flags = (input_state.flags & ~map.condition.flags);
      ApplyRemapping(map.result, remapped_state);
      return true;
    }
  }
  return false;
}

// Given a set of KeyboardRemapping structs, finds a matching struct
// if possible, then returns the KeyboardCode that would have been the
// result of the remapping. If there is no match then VKEY_UNKNOWN
// is returned. No remapping actually occurs in either case.
ui::KeyboardCode MatchedDeprecatedRemapping(
    const KeyboardRemapping* mappings,
    size_t num_mappings,
    const EventRewriterAsh::MutableKeyState& input_state) {
  for (size_t i = 0; i < num_mappings; ++i) {
    const KeyboardRemapping& map = mappings[i];
    if (MatchKeyboardRemapping(input_state, map.condition, /*strict=*/false)) {
      return map.result.key_code;
    }
  }
  return VKEY_UNKNOWN;
}

void SetMeaningForLayout(EventType type,
                         EventRewriterAsh::MutableKeyState* state) {
  // Currently layout is applied by creating a temporary key event with the
  // current physical state, and extracting the layout results.
  KeyEvent key(type, state->key_code, state->code, state->flags);
  state->key = key.GetDomKey();
}

DomCode RelocateModifier(DomCode code, DomKeyLocation location) {
  bool right = (location == DomKeyLocation::RIGHT);
  switch (code) {
    case DomCode::CONTROL_LEFT:
    case DomCode::CONTROL_RIGHT:
      return right ? DomCode::CONTROL_RIGHT : DomCode::CONTROL_LEFT;
    case DomCode::SHIFT_LEFT:
    case DomCode::SHIFT_RIGHT:
      return right ? DomCode::SHIFT_RIGHT : DomCode::SHIFT_LEFT;
    case DomCode::ALT_LEFT:
    case DomCode::ALT_RIGHT:
      return right ? DomCode::ALT_RIGHT : DomCode::ALT_LEFT;
    case DomCode::META_LEFT:
    case DomCode::META_RIGHT:
      return right ? DomCode::META_RIGHT : DomCode::META_LEFT;
    default:
      break;
  }
  return code;
}

// Returns true if |mouse_event| was generated from a touchpad device.
bool IsFromTouchpadDevice(const MouseEvent& mouse_event) {
  for (const InputDevice& touchpad :
       DeviceDataManager::GetInstance()->GetTouchpadDevices()) {
    if (touchpad.id == mouse_event.source_device_id()) {
      return true;
    }
  }

  return false;
}

// Returns whether |key_code| appears as one of the key codes that might be
// remapped by table mappings.
bool IsKeyCodeInMappings(KeyboardCode key_code,
                         const KeyboardRemapping* mappings,
                         size_t num_mappings) {
  for (size_t i = 0; i < num_mappings; ++i) {
    const KeyboardRemapping& map = mappings[i];
    if (key_code == map.condition.key_code) {
      return true;
    }
  }
  return false;
}

// Returns true if all bits in |flag_mask| are set in |flags|.
bool AreFlagsSet(int flags, int flag_mask) {
  return (flags & flag_mask) == flag_mask;
}

// Records a user action when the user press search plus a digit to
// generate an F-key.
void RecordSearchPlusDigitFKeyRewrite(ui::EventType event_type,
                                      ui::KeyboardCode key_code) {
  if (event_type != ET_KEY_PRESSED) {
    return;
  }

  switch (key_code) {
    case ui::VKEY_F1:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F1"));
      break;
    case ui::VKEY_F2:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F2"));
      break;
    case ui::VKEY_F3:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F3"));
      break;
    case ui::VKEY_F4:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F4"));
      break;
    case ui::VKEY_F5:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F5"));
      break;
    case ui::VKEY_F6:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F6"));
      break;
    case ui::VKEY_F7:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F7"));
      break;
    case ui::VKEY_F8:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F8"));
      break;
    case ui::VKEY_F9:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F9"));
      break;
    case ui::VKEY_F10:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F10"));
      break;
    case ui::VKEY_F11:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F11"));
      break;
    case ui::VKEY_F12:
      base::RecordAction(base::UserMetricsAction("SearchPlusDigitRewrite_F12"));
      break;
    default:
      NOTREACHED();
      break;
  }
}

// Records metrics for the Alt and Search based variants of keys in the
// "six pack" eg. Home, End, PageUp, PageDown, Delete, Insert.
void RecordSixPackEventRewrites(ui::EventType event_type,
                                ui::KeyboardCode key_code,
                                bool legacy_variant) {
  if (event_type != ET_KEY_PRESSED) {
    return;
  }

  if (!legacy_variant) {
    switch (key_code) {
      case ui::VKEY_DELETE:
        base::RecordAction(
            base::UserMetricsAction("SearchBasedKeyRewrite_Delete"));
        break;
      case ui::VKEY_INSERT:
        base::RecordAction(base::UserMetricsAction(
            "SearchBasedKeyRewrite_Insert_ViaSearchShiftBackspace"));
        break;
      case ui::VKEY_HOME:
        base::RecordAction(
            base::UserMetricsAction("SearchBasedKeyRewrite_Home"));
        break;
      case ui::VKEY_END:
        base::RecordAction(
            base::UserMetricsAction("SearchBasedKeyRewrite_End"));
        break;
      case ui::VKEY_PRIOR:
        base::RecordAction(
            base::UserMetricsAction("SearchBasedKeyRewrite_PageUp"));
        break;
      case ui::VKEY_NEXT:
        base::RecordAction(
            base::UserMetricsAction("SearchBasedKeyRewrite_PageDown"));
        break;
      default:
        NOTREACHED();
        break;
    }
  } else {
    switch (key_code) {
      case ui::VKEY_DELETE:
        base::RecordAction(
            base::UserMetricsAction("AltBasedKeyRewrite_Delete"));
        break;
      case ui::VKEY_INSERT:
        base::RecordAction(
            base::UserMetricsAction("SearchBasedKeyRewrite_Insert"));
        break;
      case ui::VKEY_HOME:
        base::RecordAction(base::UserMetricsAction("AltBasedKeyRewrite_Home"));
        break;
      case ui::VKEY_END:
        base::RecordAction(base::UserMetricsAction("AltBasedKeyRewrite_End"));
        break;
      case ui::VKEY_PRIOR:
        base::RecordAction(
            base::UserMetricsAction("AltBasedKeyRewrite_PageUp"));
        break;
      case ui::VKEY_NEXT:
        base::RecordAction(
            base::UserMetricsAction("AltBasedKeyRewrite_PageDown"));
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

EventRewriterAsh::MutableKeyState::MutableKeyState(const KeyEvent* key_event)
    : MutableKeyState(key_event->flags(),
                      key_event->code(),
                      key_event->GetDomKey(),
                      key_event->key_code()) {}

///////////////////////////////////////////////////////////////////////////////

EventRewriterAsh::EventRewriterAsh(Delegate* delegate,
                                   KeyboardCapability* keyboard_capability,
                                   EventRewriter* sticky_keys_controller,
                                   bool privacy_screen_supported)
    : EventRewriterAsh(
          delegate,
          keyboard_capability,
          sticky_keys_controller,
          privacy_screen_supported,
          ash::input_method::InputMethodManager::Get()->GetImeKeyboard()) {}

EventRewriterAsh::EventRewriterAsh(Delegate* delegate,
                                   KeyboardCapability* keyboard_capability,
                                   EventRewriter* sticky_keys_controller,
                                   bool privacy_screen_supported,
                                   ash::input_method::ImeKeyboard* ime_keyboard)
    : last_keyboard_device_id_(ED_UNKNOWN_DEVICE),
      delegate_(delegate),
      sticky_keys_controller_(sticky_keys_controller),
      privacy_screen_supported_(privacy_screen_supported),
      pressed_modifier_latches_(EF_NONE),
      latched_modifier_latches_(EF_NONE),
      used_modifier_latches_(EF_NONE),
      keyboard_capability_(keyboard_capability),
      ime_keyboard_(ime_keyboard) {}

EventRewriterAsh::~EventRewriterAsh() = default;

void EventRewriterAsh::ResetStateForTesting() {
  pressed_key_states_.clear();

  pressed_modifier_latches_ = latched_modifier_latches_ =
      used_modifier_latches_ = EF_NONE;
}

void EventRewriterAsh::RewriteMouseButtonEventForTesting(
    const MouseEvent& event,
    const Continuation continuation) {
  RewriteMouseButtonEvent(event, continuation);
}

EventDispatchDetails EventRewriterAsh::RewriteEvent(
    const Event& event,
    const Continuation continuation) {
  if ((event.type() == ET_KEY_PRESSED) || (event.type() == ET_KEY_RELEASED)) {
    std::unique_ptr<Event> rewritten_event;
    const base::Time key_rewrite_start_time = base::Time::Now();
    DCHECK((&event)->AsKeyEvent());
    const EventRewriteStatus status =
        RewriteKeyEvent(*((&event)->AsKeyEvent()), &rewritten_event);
    RecordAutoRepeatUsageMetric(*((&event)->AsKeyEvent()), rewritten_event);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "ChromeOS.Inputs.EventRewriter.KeyRewriteLatency",
        base::Time::Now() - key_rewrite_start_time, base::Microseconds(1),
        base::Milliseconds(100), 100);
    return RewriteKeyEventInContext(*((&event)->AsKeyEvent()),
                                    std::move(rewritten_event), status,
                                    continuation);
  }
  if ((event.type() == ET_MOUSE_PRESSED) ||
      (event.type() == ET_MOUSE_RELEASED)) {
    return RewriteMouseButtonEvent(static_cast<const MouseEvent&>(event),
                                   continuation);
  }
  if (event.type() == ET_MOUSEWHEEL) {
    return RewriteMouseWheelEvent(static_cast<const MouseWheelEvent&>(event),
                                  continuation);
  }
  if ((event.type() == ET_TOUCH_PRESSED) ||
      (event.type() == ET_TOUCH_RELEASED)) {
    return RewriteTouchEvent(static_cast<const TouchEvent&>(event),
                             continuation);
  }
  if (event.IsScrollEvent()) {
    return RewriteScrollEvent(static_cast<const ScrollEvent&>(event),
                              continuation);
  }

  return SendEvent(continuation, &event);
}

void EventRewriterAsh::BuildRewrittenKeyEvent(
    const KeyEvent& key_event,
    const MutableKeyState& state,
    std::unique_ptr<Event>* rewritten_event) {
  auto key_event_ptr = std::make_unique<KeyEvent>(
      key_event.type(), state.key_code, state.code, state.flags, state.key,
      key_event.time_stamp());
  key_event_ptr->set_scan_code(key_event.scan_code());
  *rewritten_event = std::move(key_event_ptr);
}

// static
bool EventRewriterAsh::HasAssistantKeyOnKeyboard(
    const InputDevice& keyboard_device,
    bool* has_assistant_key) {
  std::unique_ptr<EventDeviceInfo> devinfo =
      KeyboardCapability::CreateEventDeviceInfoFromInputDevice(keyboard_device);
  if (!devinfo) {
    return false;
  }

  *has_assistant_key = devinfo->HasKeyEvent(KEY_ASSISTANT);
  return true;
}

bool EventRewriterAsh::RewriteModifierKeys(const KeyEvent& key_event,
                                           MutableKeyState* state) {
  DCHECK(key_event.type() == ET_KEY_PRESSED ||
         key_event.type() == ET_KEY_RELEASED);

  if (!delegate_ || !delegate_->RewriteModifierKeys()) {
    return false;
  }

  // Preserve a copy of the original before rewriting |state| based on
  // user preferences, device configuration, and certain IME properties.
  MutableKeyState incoming = *state;
  state->flags = EF_NONE;
  int characteristic_flag = EF_NONE;
  bool exact_event = false;

  // First, remap the key code.
  const ModifierRemapping* remapped_key = nullptr;
  // Remapping based on DomKey.
  switch (incoming.key) {
    case DomKey::ALT_GRAPH:
      // The Neo2 codes modifiers such that CapsLock appears as VKEY_ALTGR,
      // but AltGraph (right Alt) also appears as VKEY_ALTGR in Neo2,
      // as it does in other layouts. Neo2's "Mod3" is represented in
      // EventFlags by a combination of AltGr+Mod3, while its "Mod4" is
      // AltGr alone.
      if (IsISOLevel5ShiftUsedByCurrentInputMethod()) {
        if (incoming.code == DomCode::CAPS_LOCK) {
          characteristic_flag = EF_ALTGR_DOWN | EF_MOD3_DOWN;
          remapped_key = GetRemappedKey(
              last_keyboard_device_id_, mojom::ModifierKey::kCapsLock,
              prefs::kLanguageRemapCapsLockKeyTo, delegate_);
        } else {
          characteristic_flag = EF_ALTGR_DOWN;
          remapped_key = GetSearchRemappedKey(
              delegate_, last_keyboard_device_id_, GetLastKeyboardType());
        }
      }
      if (remapped_key && remapped_key->result.key_code == VKEY_CAPITAL) {
        remapped_key = kModifierRemappingIsoLevel5ShiftMod3;
      }
      break;
    case DomKey::ALT_GRAPH_LATCH:
      if (key_event.type() == ET_KEY_PRESSED) {
        pressed_modifier_latches_ |= EF_ALTGR_DOWN;
      } else {
        pressed_modifier_latches_ &= ~EF_ALTGR_DOWN;
        if (used_modifier_latches_ & EF_ALTGR_DOWN) {
          used_modifier_latches_ &= ~EF_ALTGR_DOWN;
        } else {
          latched_modifier_latches_ |= EF_ALTGR_DOWN;
        }
      }
      // Rewrite to AltGraph. When this key is used like a regular modifier,
      // the web-exposed result looks like a use of the regular modifier.
      // When it's used as a latch, the web-exposed result is a vacuous
      // modifier press-and-release, which should be harmless, but preserves
      // the event for applications using the |code| (e.g. remoting).
      state->key = DomKey::ALT_GRAPH;
      state->key_code = VKEY_ALTGR;
      exact_event = true;
      break;
    default:
      break;
  }

  // Remapping based on DomCode.
  switch (incoming.code) {
    // On Chrome OS, Caps_Lock with Mod3Mask is sent when Caps Lock is pressed
    // (with one exception: when IsISOLevel5ShiftUsedByCurrentInputMethod() is
    // true, the key generates XK_ISO_Level3_Shift with Mod3Mask, not
    // Caps_Lock).
    case DomCode::CAPS_LOCK:
      // This key is already remapped to Mod3 in remapping based on DomKey. Skip
      // more remapping.
      if (IsISOLevel5ShiftUsedByCurrentInputMethod() && remapped_key) {
        break;
      }

      characteristic_flag = EF_CAPS_LOCK_ON;
      remapped_key = GetRemappedKey(
          last_keyboard_device_id_, mojom::ModifierKey::kCapsLock,
          prefs::kLanguageRemapCapsLockKeyTo, delegate_);
      break;
    case DomCode::META_LEFT:
    case DomCode::META_RIGHT:
      characteristic_flag = EF_COMMAND_DOWN;
      remapped_key = GetSearchRemappedKey(delegate_, last_keyboard_device_id_,
                                          GetLastKeyboardType());
      // Default behavior is Super key, hence don't remap the event if the pref
      // is unavailable.
      break;
    case DomCode::CONTROL_LEFT:
    case DomCode::CONTROL_RIGHT:
      characteristic_flag = EF_CONTROL_DOWN;
      remapped_key =
          GetRemappedKey(last_keyboard_device_id_, mojom::ModifierKey::kControl,
                         prefs::kLanguageRemapControlKeyTo, delegate_);
      break;
    case DomCode::ALT_LEFT:
    case DomCode::ALT_RIGHT:
      // ALT key
      characteristic_flag = EF_ALT_DOWN;
      remapped_key =
          GetRemappedKey(last_keyboard_device_id_, mojom::ModifierKey::kAlt,
                         prefs::kLanguageRemapAltKeyTo, delegate_);
      break;
    case DomCode::ESCAPE:
      remapped_key =
          GetRemappedKey(last_keyboard_device_id_, mojom::ModifierKey::kEscape,
                         prefs::kLanguageRemapEscapeKeyTo, delegate_);
      break;
    case DomCode::BACKSPACE:
      remapped_key = GetRemappedKey(
          last_keyboard_device_id_, mojom::ModifierKey::kBackspace,
          prefs::kLanguageRemapBackspaceKeyTo, delegate_);
      break;
    case DomCode::LAUNCH_ASSISTANT:
      remapped_key = GetRemappedKey(
          last_keyboard_device_id_, mojom::ModifierKey::kAssistant,
          prefs::kLanguageRemapAssistantKeyTo, delegate_);
      break;
    default:
      break;
  }

  if (remapped_key) {
    state->key_code = remapped_key->result.key_code;
    state->code = remapped_key->result.code;
    state->key = remapped_key->result.key;
    incoming.flags |= characteristic_flag;
    characteristic_flag = remapped_key->flag;

    // If the internal state of CapLocks is enabled, we should not remove
    // the modifier flag. This is important for the case in which the user
    // remaps the CapsLock key to another key (e.g. Search) and CapsLock is
    // enabled. If the user were to press the CapsLock key (remapped to Search),
    // we risk removing the CapsLock modifier and accidentally disabling
    // CapsLocks.
    if (incoming.key_code == VKEY_CAPITAL &&
        !ime_keyboard_->CapsLockIsEnabled()) {
      // We remove the CapsLock modifier here because we do not want to
      // turn on the Capslock modifier when the key has been remapped.
      incoming.flags &= ~EF_CAPS_LOCK_ON;
    }
    if (remapped_key->remap_to == ui::mojom::ModifierKey::kCapsLock) {
      characteristic_flag |= EF_CAPS_LOCK_ON;
    }
    state->code = RelocateModifier(
        state->code, KeycodeConverter::DomCodeToLocation(incoming.code));
  }

  // Next, remap modifier bits.
  state->flags |= GetRemappedModifierMasks(key_event, incoming.flags);

  // If the DomKey is not a modifier before remapping but is after, set the
  // modifier latches for the later non-modifier key's modifier states.
  bool non_modifier_to_modifier =
      !KeycodeConverter::IsDomKeyForModifier(incoming.key) &&
      KeycodeConverter::IsDomKeyForModifier(state->key);
  if (key_event.type() == ET_KEY_PRESSED) {
    state->flags |= characteristic_flag;
    if (non_modifier_to_modifier) {
      // Edge case: User remaps key while still holding it. Remove the
      // previously mapped latch.
      if (previous_non_modifier_latches_.contains(incoming.code)) {
        pressed_modifier_latches_ &=
            ~previous_non_modifier_latches_[incoming.code];
      }
      pressed_modifier_latches_ |= characteristic_flag;
      previous_non_modifier_latches_[incoming.code] = characteristic_flag;
    }
  } else {
    state->flags &= ~characteristic_flag;
    if (non_modifier_to_modifier) {
      pressed_modifier_latches_ &= ~characteristic_flag;
    }
    if (previous_non_modifier_latches_.contains(incoming.code)) {
      pressed_modifier_latches_ &=
          ~previous_non_modifier_latches_[incoming.code];
      previous_non_modifier_latches_.erase(incoming.code);
    }
  }

  if (key_event.type() == ET_KEY_PRESSED) {
    if (!KeycodeConverter::IsDomKeyForModifier(state->key)) {
      used_modifier_latches_ |= pressed_modifier_latches_;
      latched_modifier_latches_ = EF_NONE;
    }
  }

  // Implement the Caps Lock modifier here, rather than in the
  // AcceleratorController, so that the event is visible to apps (see
  // crbug.com/775743).
  if (key_event.type() == ET_KEY_RELEASED && state->key_code == VKEY_CAPITAL) {
    ime_keyboard_->SetCapsLockEnabled(!ime_keyboard_->CapsLockIsEnabled());
  }
  return exact_event;
}

void EventRewriterAsh::DeviceKeyPressedOrReleased(int device_id) {
  KeyboardCapability::DeviceType type =
      keyboard_capability_->GetDeviceType(device_id);

  // Ignore virtual Xorg keyboard (magic that generates key repeat
  // events). Pretend that the previous real keyboard is the one that is still
  // in use.
  if (type == KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard) {
    return;
  }

  last_keyboard_device_id_ = device_id;
}

bool EventRewriterAsh::IsHotrodRemote() const {
  return IsLastKeyboardOfType(
      KeyboardCapability::DeviceType::kDeviceHotrodRemote);
}

bool EventRewriterAsh::IsLastKeyboardOfType(
    KeyboardCapability::DeviceType device_type) const {
  return GetLastKeyboardType() == device_type;
}

KeyboardCapability::DeviceType EventRewriterAsh::GetLastKeyboardType() const {
  if ((last_keyboard_device_id_ == ED_UNKNOWN_DEVICE) ||
      (last_keyboard_device_id_ == ED_REMOTE_INPUT_DEVICE)) {
    return KeyboardCapability::DeviceType::kDeviceUnknown;
  }

  return keyboard_capability_->GetDeviceType(last_keyboard_device_id_);
}

int EventRewriterAsh::GetRemappedModifierMasks(const Event& event,
                                               int original_flags) const {
  int unmodified_flags = original_flags;
  int rewritten_flags = pressed_modifier_latches_ | latched_modifier_latches_;
  for (size_t i = 0; unmodified_flags && (i < std::size(kModifierRemappings));
       ++i) {
    const ModifierRemapping* remapped_key = nullptr;
    if (!(unmodified_flags & kModifierRemappings[i].flag)) {
      continue;
    }
    switch (kModifierRemappings[i].flag) {
      case EF_COMMAND_DOWN:
        remapped_key = GetSearchRemappedKey(delegate_, last_keyboard_device_id_,
                                            GetLastKeyboardType());
        break;
      case EF_MOD3_DOWN:
        // If EF_MOD3_DOWN is used by the current input method, leave it alone;
        // it is not remappable.
        if (IsISOLevel5ShiftUsedByCurrentInputMethod()) {
          continue;
        }
        // Otherwise, Mod3Mask is set on X events when the Caps Lock key
        // is down, but, if Caps Lock is remapped, CapsLock is NOT set,
        // because pressing the key does not invoke caps lock. So, the
        // kModifierRemappings[] table uses EF_MOD3_DOWN for the Caps
        // Lock remapping.
        break;
      case EF_MOD3_DOWN | EF_ALTGR_DOWN:
        if ((original_flags & EF_ALTGR_DOWN) &&
            IsISOLevel5ShiftUsedByCurrentInputMethod()) {
          remapped_key = kModifierRemappingIsoLevel5ShiftMod3;
        }
        break;
      default:
        break;
    }
    if (!remapped_key && kModifierRemappings[i].pref_name) {
      remapped_key = GetRemappedKey(
          last_keyboard_device_id_, kModifierRemappings[i].remap_to,
          kModifierRemappings[i].pref_name, delegate_);
    }
    if (remapped_key) {
      unmodified_flags &= ~kModifierRemappings[i].flag;
      rewritten_flags |= remapped_key->flag;
    }
  }
  return rewritten_flags | unmodified_flags;
}

bool EventRewriterAsh::ShouldRemapToRightClick(
    const MouseEvent& mouse_event,
    int flags,
    int* matched_mask,
    bool* matched_alt_deprecation) const {
  *matched_mask = 0;
  *matched_alt_deprecation = false;

  // If currently only mouse left button is still pressed, while Alt or Search
  // is not, then we need to look deeper. Here we piggyback on an existing
  // instance variable `pressed_as_right_button_device_ids_` to check whether
  // the previous remapped event is a remapped mouse right button press event.
  // If yes, then even currently the Alt or Search is not pressed, we still
  // proceed to remap to a mouse right button event. Also, in this case, this
  // event has to be a release event. this change is for regressions such as:
  // https://crbug.com/1399284, https://crbug.com/1417079
  const bool release_without_modifier =
      AreFlagsSet(flags, EF_LEFT_MOUSE_BUTTON) &&
      pressed_as_right_button_device_ids_.count(
          mouse_event.source_device_id()) &&
      mouse_event.type() == ET_MOUSE_RELEASED;
  // TODO(crbug.com/1179893): When enabling the deprecate alt click flag by
  // default, decide whether kUseSearchClickForRightClick being disabled
  // should be able to override it.
  const bool use_search_key =
      base::FeatureList::IsEnabled(
          ::ash::features::kUseSearchClickForRightClick) ||
      ::features::IsDeprecateAltClickEnabled();
  if (use_search_key) {
    if (AreFlagsSet(flags, kSearchLeftButton)) {
      *matched_mask = kSearchLeftButton;
    } else if (release_without_modifier) {
      *matched_mask = kSearchLeftButton;
    } else if (AreFlagsSet(flags, kAltLeftButton) &&
               is_alt_down_remapping_enabled_) {
      // When the alt variant is deprecated, report when it would have matched.
      *matched_alt_deprecation = ((mouse_event.type() == ET_MOUSE_PRESSED) ||
                                  pressed_as_right_button_device_ids_.count(
                                      mouse_event.source_device_id())) &&
                                 IsFromTouchpadDevice(mouse_event);
    }
  } else if (is_alt_down_remapping_enabled_) {
    // If currently both Alt key and mouse left button are still pressed,
    // then this would be an easy case, let's still proceed to remap it
    // to a mouse right button press or release event.
    if (AreFlagsSet(flags, kAltLeftButton)) {
      *matched_mask = kAltLeftButton;
    } else if (release_without_modifier) {
      *matched_mask = kAltLeftButton;
    }
  }

  // If the event rewrite matched (ie. matched_mask != 0) then
  // |matched_alt_deprecation| must be false.
  DCHECK(*matched_mask == 0 || !*matched_alt_deprecation);

  return (*matched_mask != 0) &&
         ((mouse_event.type() == ET_MOUSE_PRESSED) ||
          pressed_as_right_button_device_ids_.count(
              mouse_event.source_device_id())) &&
         IsFromTouchpadDevice(mouse_event);
}

void EventRewriterAsh::RecordModifierKeyPressedAfterRemapping(
    DomCode dom_code) {
  const ModifierKeyUsageMapping* modifier_key_usage_mapping = nullptr;
  for (const auto& mapping : modifier_key_usage_mappings) {
    if (dom_code == mapping.code) {
      modifier_key_usage_mapping = &mapping;
      break;
    }
  }

  if (modifier_key_usage_mapping == nullptr) {
    return;
  }

  const auto device_type = GetLastKeyboardType();
  switch (device_type) {
    case KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case KeyboardCapability::DeviceType::kDeviceExternalUnknown:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
    case KeyboardCapability::DeviceType::kDeviceUnknown:
      break;
  }
}

void EventRewriterAsh::RecordModifierKeyPressedBeforeRemapping(
    DomCode dom_code) {
  const ModifierKeyUsageMapping* modifier_key_usage_mapping = nullptr;
  for (const auto& mapping : modifier_key_usage_mappings) {
    if (dom_code == mapping.code) {
      modifier_key_usage_mapping = &mapping;
      break;
    }
  }

  if (modifier_key_usage_mapping == nullptr) {
    return;
  }

  const auto device_type = GetLastKeyboardType();
  switch (device_type) {
    case KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
    case KeyboardCapability::DeviceType::kDeviceExternalUnknown:
      UMA_HISTOGRAM_ENUMERATION(
          "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
          modifier_key_usage_mapping->modifier_key_enum);
      break;
    case KeyboardCapability::DeviceType::kDeviceHotrodRemote:
    case KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
    case KeyboardCapability::DeviceType::kDeviceUnknown:
      break;
  }
}

EventRewriteStatus EventRewriterAsh::RewriteKeyEvent(
    const KeyEvent& key_event,
    std::unique_ptr<Event>* rewritten_event) {
  if (key_event.source_device_id() != ED_UNKNOWN_DEVICE) {
    DeviceKeyPressedOrReleased(key_event.source_device_id());
  }

  // Drop repeated keys from Hotrod remote.
  if ((key_event.flags() & EF_IS_REPEAT) &&
      (key_event.type() == ET_KEY_PRESSED) && IsHotrodRemote() &&
      key_event.key_code() != VKEY_BACK) {
    return EVENT_REWRITE_DISCARD;
  }

  // Records metric if the `key_event` is for a modifier key press event.
  const bool should_record_modifier_key_press_metrics =
      !(key_event.flags() & EF_IS_REPEAT) && key_event.type() == ET_KEY_PRESSED;
  if (should_record_modifier_key_press_metrics) {
    RecordModifierKeyPressedBeforeRemapping(key_event.code());
  }

  MutableKeyState state = {key_event.flags(), key_event.code(),
                           key_event.GetDomKey(), key_event.key_code()};

  // Do not rewrite an event sent by ui_controls::SendKeyPress(). See
  // crbug.com/136465.
  if (!(key_event.flags() & EF_FINAL)) {
    // If RewriteModifierKeys() returns true there should be no more processing
    // done to the key event. It will only return true if the key event is
    // rewritten to ALTGR. A false return is not an error.
    if (RewriteModifierKeys(key_event, &state)) {
      if (should_record_modifier_key_press_metrics) {
        RecordModifierKeyPressedAfterRemapping(state.code);
      }
      // Early exit with completed event.
      BuildRewrittenKeyEvent(key_event, state, rewritten_event);
      return EVENT_REWRITE_REWRITTEN;
    }
    RewriteNumPadKeys(key_event, &state);
  }

  if (should_record_modifier_key_press_metrics) {
    RecordModifierKeyPressedAfterRemapping(state.code);
  }

  if (delegate_ &&
      delegate_->IsExtensionCommandRegistered(state.key_code, state.flags)) {
    // If |state| and |key_event| have any different fields, a rewrite has
    // occurred. Build the rewritten key event and return early.
    if (MutableKeyState(&key_event) != state) {
      BuildRewrittenKeyEvent(key_event, state, rewritten_event);
      return EVENT_REWRITE_REWRITTEN;
    }
    // The key event was not modified, forward the event downstream.
    return EVENT_REWRITE_CONTINUE;
  }

  EventRewriteStatus status = EVENT_REWRITE_CONTINUE;
  bool is_sticky_key_extension_command = false;
  if (sticky_keys_controller_) {
    KeyEvent tmp_event = key_event;
    tmp_event.set_key_code(state.key_code);
    tmp_event.set_flags(state.flags);
    std::unique_ptr<Event> output_event;
    status = sticky_keys_controller_->RewriteEvent(tmp_event, &output_event);
    if (status == EVENT_REWRITE_REWRITTEN ||
        status == EVENT_REWRITE_DISPATCH_ANOTHER) {
      state.flags = output_event->flags();
    }
    if (status == EVENT_REWRITE_DISCARD) {
      return EVENT_REWRITE_DISCARD;
    }
    is_sticky_key_extension_command =
        delegate_ &&
        delegate_->IsExtensionCommandRegistered(state.key_code, state.flags);
  }

  // If flags have changed, this may change the interpretation of the key,
  // so reapply layout.
  if (state.flags != key_event.flags()) {
    SetMeaningForLayout(key_event.type(), &state);
  }

  // If sticky key rewrites the event, and it matches an extension command, do
  // not further rewrite the event since it won't match the extension command
  // thereafter.
  if (!is_sticky_key_extension_command && !(key_event.flags() & EF_FINAL)) {
    RewriteExtendedKeys(key_event, &state);
    RewriteFunctionKeys(key_event, &state);
  }
  if ((key_event.flags() == state.flags) &&
      (key_event.key_code() == state.key_code) &&
      (status == EVENT_REWRITE_CONTINUE)) {
    return EVENT_REWRITE_CONTINUE;
  }
  // Sticky keys may have returned a result other than |EVENT_REWRITE_CONTINUE|,
  // in which case we need to preserve that return status. Alternatively, we
  // might be here because key_event changed, in which case we need to
  // return |EVENT_REWRITE_REWRITTEN|.
  if (status == EVENT_REWRITE_CONTINUE) {
    status = EVENT_REWRITE_REWRITTEN;
  }
  BuildRewrittenKeyEvent(key_event, state, rewritten_event);
  return status;
}

// TODO(yhanada): Clean up this method once StickyKeysController migrates to the
// new API.
EventDispatchDetails EventRewriterAsh::RewriteMouseButtonEvent(
    const MouseEvent& mouse_event,
    const Continuation continuation) {
  int flags = RewriteLocatedEvent(mouse_event);
  EventRewriteStatus status = EVENT_REWRITE_CONTINUE;
  if (sticky_keys_controller_) {
    MouseEvent tmp_event = mouse_event;
    tmp_event.set_flags(flags);
    std::unique_ptr<Event> output_event;
    status = sticky_keys_controller_->RewriteEvent(tmp_event, &output_event);
    if (status == EVENT_REWRITE_REWRITTEN ||
        status == EVENT_REWRITE_DISPATCH_ANOTHER) {
      flags = output_event->flags();
    }
  }
  int changed_button = EF_NONE;
  if ((mouse_event.type() == ET_MOUSE_PRESSED) ||
      (mouse_event.type() == ET_MOUSE_RELEASED)) {
    changed_button = RewriteModifierClick(mouse_event, &flags);
  }
  if ((mouse_event.flags() == flags) && (status == EVENT_REWRITE_CONTINUE)) {
    return SendEvent(continuation, &mouse_event);
  }

  std::unique_ptr<Event> rewritten_event = mouse_event.Clone();
  rewritten_event->set_flags(flags);
  if (changed_button != EF_NONE) {
    static_cast<MouseEvent*>(rewritten_event.get())
        ->set_changed_button_flags(changed_button);
  }

  EventDispatchDetails details =
      SendEventFinally(continuation, rewritten_event.get());
  if (status == EVENT_REWRITE_DISPATCH_ANOTHER &&
      !details.dispatcher_destroyed) {
    // Here, we know that another event is a modifier key release event from
    // StickyKeysController.
    return SendStickyKeysReleaseEvents(std::move(rewritten_event),
                                       continuation);
  }
  return details;
}

EventDispatchDetails EventRewriterAsh::RewriteMouseWheelEvent(
    const MouseWheelEvent& wheel_event,
    const Continuation continuation) {
  if (!sticky_keys_controller_) {
    return SendEvent(continuation, &wheel_event);
  }

  const int flags = RewriteLocatedEvent(wheel_event);
  MouseWheelEvent tmp_event = wheel_event;
  tmp_event.set_flags(flags);
  return sticky_keys_controller_->RewriteEvent(tmp_event, continuation);
}

EventDispatchDetails EventRewriterAsh::RewriteTouchEvent(
    const TouchEvent& touch_event,
    const Continuation continuation) {
  const int flags = RewriteLocatedEvent(touch_event);
  if (touch_event.flags() == flags) {
    return SendEvent(continuation, &touch_event);
  }
  TouchEvent rewritten_touch_event(touch_event);
  rewritten_touch_event.set_flags(flags);
  return SendEventFinally(continuation, &rewritten_touch_event);
}

EventDispatchDetails EventRewriterAsh::RewriteScrollEvent(
    const ScrollEvent& scroll_event,
    const Continuation continuation) {
  if (!sticky_keys_controller_) {
    return SendEvent(continuation, &scroll_event);
  }
  return sticky_keys_controller_->RewriteEvent(scroll_event, continuation);
}

void EventRewriterAsh::RewriteNumPadKeys(const KeyEvent& key_event,
                                         MutableKeyState* state) {
  DCHECK(key_event.type() == ET_KEY_PRESSED ||
         key_event.type() == ET_KEY_RELEASED);
  static const struct NumPadRemapping {
    KeyboardCode input_key_code;
    MutableKeyState result;
  } kNumPadRemappings[] = {{VKEY_DELETE,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'.'>::Character, VKEY_DECIMAL}},
                           {VKEY_INSERT,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'0'>::Character, VKEY_NUMPAD0}},
                           {VKEY_END,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'1'>::Character, VKEY_NUMPAD1}},
                           {VKEY_DOWN,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'2'>::Character, VKEY_NUMPAD2}},
                           {VKEY_NEXT,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'3'>::Character, VKEY_NUMPAD3}},
                           {VKEY_LEFT,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'4'>::Character, VKEY_NUMPAD4}},
                           {VKEY_CLEAR,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'5'>::Character, VKEY_NUMPAD5}},
                           {VKEY_RIGHT,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'6'>::Character, VKEY_NUMPAD6}},
                           {VKEY_HOME,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'7'>::Character, VKEY_NUMPAD7}},
                           {VKEY_UP,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'8'>::Character, VKEY_NUMPAD8}},
                           {VKEY_PRIOR,
                            {EF_NONE, DomCode::NONE,
                             DomKey::Constant<'9'>::Character, VKEY_NUMPAD9}}};
  for (const auto& map : kNumPadRemappings) {
    if (state->key_code == map.input_key_code) {
      if (KeycodeConverter::DomCodeToLocation(state->code) ==
          DomKeyLocation::NUMPAD) {
        ApplyRemapping(map.result, state);
      }
      return;
    }
  }
}

void EventRewriterAsh::RewriteExtendedKeys(const KeyEvent& key_event,
                                           MutableKeyState* state) {
  DCHECK(key_event.type() == ET_KEY_PRESSED ||
         key_event.type() == ET_KEY_RELEASED);
  MutableKeyState incoming = *state;

  // TODO(crbug.com/1179893): This workaround isn't needed once Alt rewrites
  // are deprecated.
  if ((!::features::IsImprovedKeyboardShortcutsEnabled() ||
       !::features::IsDeprecateAltBasedSixPackEnabled()) &&
      ((incoming.flags & (EF_COMMAND_DOWN | EF_ALT_DOWN)) ==
       (EF_COMMAND_DOWN | EF_ALT_DOWN))) {
    // Allow Search to avoid rewriting extended keys.
    // For these, we only remove the EF_COMMAND_DOWN flag.
    static const KeyboardRemapping::Condition kAvoidRemappings[] = {
        {// Alt+Backspace
         EF_ALT_DOWN | EF_COMMAND_DOWN, VKEY_BACK},
        {// Control+Alt+Up
         EF_ALT_DOWN | EF_CONTROL_DOWN | EF_COMMAND_DOWN, VKEY_UP},
        {// Control+Alt+Down
         EF_ALT_DOWN | EF_CONTROL_DOWN | EF_COMMAND_DOWN, VKEY_DOWN}};
    for (const auto& condition : kAvoidRemappings) {
      if (MatchKeyboardRemapping(*state, condition)) {
        state->flags = incoming.flags & ~EF_COMMAND_DOWN;
        return;
      }
    }
  }

  if (incoming.flags & EF_COMMAND_DOWN) {
    bool strict = false;
    bool skip_search_key_remapping =
        delegate_ && delegate_->IsSearchKeyAcceleratorReserved();

    if (!::features::IsImprovedKeyboardShortcutsEnabled()) {
      // TODO(crbug.com/1179893): This workaround isn't needed once Alt rewrites
      // are deprecated.
      strict = ::features::IsNewShortcutMappingEnabled();
      if (strict) {
        DCHECK(!::features::IsImprovedKeyboardShortcutsEnabled());

        // These two keys are used to select to Home/End.
        static const KeyboardRemapping kNewSearchRemappings[] = {
            {// Search+Shift+Left -> select to home.
             {EF_COMMAND_DOWN | EF_SHIFT_DOWN, VKEY_LEFT},
             {EF_SHIFT_DOWN, DomCode::HOME, DomKey::HOME, VKEY_HOME}},
            {// Search+Shift+Right -> select to end.
             {EF_COMMAND_DOWN | EF_SHIFT_DOWN, VKEY_RIGHT},
             {EF_SHIFT_DOWN, DomCode::END, DomKey::END, VKEY_END}},
        };
        if (!skip_search_key_remapping &&
            RewriteWithKeyboardRemappings(kNewSearchRemappings,
                                          std::size(kNewSearchRemappings),
                                          incoming, state, /*strict=*/true)) {
          return;
        }
      }
    }

    // The new Search+Shift+Backspace rewrite is only active when
    // IsImprovedKeyboardShortcutsEnabled() is true.
    // TODO(crbug.com/1179893): Merge this entry into kSixPackRemappings
    // once the flag is removed.
    static const KeyboardRemapping kOldInsertRemapping[] = {
        {// Search+Period -> Insert
         {EF_COMMAND_DOWN, VKEY_OEM_PERIOD},
         {EF_NONE, DomCode::INSERT, DomKey::INSERT, VKEY_INSERT}},
    };

    if (::features::IsImprovedKeyboardShortcutsEnabled()) {
      static const KeyboardRemapping kNewInsertRemapping[] = {
          {// Search+Shift+BackSpace -> Insert
           {EF_COMMAND_DOWN | EF_SHIFT_DOWN, VKEY_BACK},
           {EF_NONE, DomCode::INSERT, DomKey::INSERT, VKEY_INSERT}},
      };

      if (!skip_search_key_remapping &&
          RewriteWithKeyboardRemappings(kNewInsertRemapping,
                                        std::size(kNewInsertRemapping),
                                        incoming, state, strict)) {
        RecordSixPackEventRewrites(key_event.type(), state->key_code,
                                   /*legacy_variant=*/false);
        return;
      }

      // Test for the deprecated insert rewrite in order to show a notification.
      const ui::KeyboardCode deprecated_key = MatchedDeprecatedRemapping(
          kOldInsertRemapping, std::size(kOldInsertRemapping), incoming);
      if (deprecated_key != VKEY_UNKNOWN) {
        // If the key would have matched prior to being deprecated then notify
        // the delegate to show a notification.
        delegate_->NotifyDeprecatedSixPackKeyRewrite(deprecated_key);
      }
    } else {
      if (!skip_search_key_remapping &&
          RewriteWithKeyboardRemappings(kOldInsertRemapping,
                                        std::size(kOldInsertRemapping),
                                        incoming, state, strict)) {
        RecordSixPackEventRewrites(key_event.type(), state->key_code,
                                   /*legacy_variant=*/true);
        return;
      }
    }

    static const KeyboardRemapping kSixPackRemappings[] = {
        {// Search+BackSpace -> Delete
         {EF_COMMAND_DOWN, VKEY_BACK},
         {EF_NONE, DomCode::DEL, DomKey::DEL, VKEY_DELETE}},
        {// Search+Left -> Home
         {EF_COMMAND_DOWN, VKEY_LEFT},
         {EF_NONE, DomCode::HOME, DomKey::HOME, VKEY_HOME}},
        {// Search+Up -> Prior (aka PageUp)
         {EF_COMMAND_DOWN, VKEY_UP},
         {EF_NONE, DomCode::PAGE_UP, DomKey::PAGE_UP, VKEY_PRIOR}},
        {// Search+Right -> End
         {EF_COMMAND_DOWN, VKEY_RIGHT},
         {EF_NONE, DomCode::END, DomKey::END, VKEY_END}},
        {// Search+Down -> Next (aka PageDown)
         {EF_COMMAND_DOWN, VKEY_DOWN},
         {EF_NONE, DomCode::PAGE_DOWN, DomKey::PAGE_DOWN, VKEY_NEXT}}};

    if (!skip_search_key_remapping &&
        RewriteWithKeyboardRemappings(kSixPackRemappings,
                                      std::size(kSixPackRemappings), incoming,
                                      state, strict)) {
      RecordSixPackEventRewrites(key_event.type(), state->key_code,
                                 /*legacy_variant=*/false);
      return;
    }
  }

  // TODO(crbug.com/1179893): Remove block once Alt rewrites are deprecated.
  if ((incoming.flags & EF_ALT_DOWN) && is_alt_down_remapping_enabled_) {
    static const KeyboardRemapping kLegacySixPackRemappings[] = {
        {// Alt+BackSpace -> Delete
         {EF_ALT_DOWN, VKEY_BACK},
         {EF_NONE, DomCode::DEL, DomKey::DEL, VKEY_DELETE}},
        {// Control+Alt+Up -> Home
         {EF_ALT_DOWN | EF_CONTROL_DOWN, VKEY_UP},
         {EF_NONE, DomCode::HOME, DomKey::HOME, VKEY_HOME}},
        {// Alt+Up -> Prior (aka PageUp)
         {EF_ALT_DOWN, VKEY_UP},
         {EF_NONE, DomCode::PAGE_UP, DomKey::PAGE_UP, VKEY_PRIOR}},
        {// Control+Alt+Down -> End
         {EF_ALT_DOWN | EF_CONTROL_DOWN, VKEY_DOWN},
         {EF_NONE, DomCode::END, DomKey::END, VKEY_END}},
        {// Alt+Down -> Next (aka PageDown)
         {EF_ALT_DOWN, VKEY_DOWN},
         {EF_NONE, DomCode::PAGE_DOWN, DomKey::PAGE_DOWN, VKEY_NEXT}}};
    if (!::features::IsImprovedKeyboardShortcutsEnabled() ||
        !::features::IsDeprecateAltBasedSixPackEnabled()) {
      if (RewriteWithKeyboardRemappings(kLegacySixPackRemappings,
                                        std::size(kLegacySixPackRemappings),
                                        incoming, state)) {
        RecordSixPackEventRewrites(key_event.type(), state->key_code,
                                   /*legacy_variant=*/true);
        return;
      }
    } else {
      const ui::KeyboardCode deprecated_key = MatchedDeprecatedRemapping(
          kLegacySixPackRemappings, std::size(kLegacySixPackRemappings),
          incoming);
      if (deprecated_key != VKEY_UNKNOWN) {
        // If the key would have matched prior to being deprecated then notify
        // the delegate to show a notification.
        delegate_->NotifyDeprecatedSixPackKeyRewrite(deprecated_key);
      }
    }
  }
}

void EventRewriterAsh::RewriteFunctionKeys(const KeyEvent& key_event,
                                           MutableKeyState* state) {
  CHECK(key_event.type() == ET_KEY_PRESSED ||
        key_event.type() == ET_KEY_RELEASED);

  // Some key codes have a Dom code but no VKEY value assigned. They're mapped
  // to VKEY values here.
  if (state->key_code == VKEY_UNKNOWN) {
    if (state->code == DomCode::SHOW_ALL_WINDOWS) {
      // Show all windows is through VKEY_MEDIA_LAUNCH_APP1.
      state->key_code = VKEY_MEDIA_LAUNCH_APP1;
      state->key = DomKey::F4;
    } else if (state->code == DomCode::DISPLAY_TOGGLE_INT_EXT) {
      // Display toggle is through control + VKEY_ZOOM.
      state->flags |= EF_CONTROL_DOWN;
      state->key_code = VKEY_ZOOM;
      state->key = DomKey::F12;
    }
  }

  KeyboardCapability::KeyboardTopRowLayout layout =
      keyboard_capability_->GetTopRowLayout(key_event.source_device_id());

  const bool search_is_pressed = (state->flags & EF_COMMAND_DOWN) != 0;
  const bool flip_remapping = ShouldRewriteMetaTopRowKeyComboEvents(
                                  delegate_, last_keyboard_device_id_) &&
                              search_is_pressed;
  if (layout ==
      KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom) {
    if (RewriteTopRowKeysForCustomLayout(key_event.source_device_id(),
                                         key_event, search_is_pressed, state)) {
      return;
    }
  } else if (layout == KeyboardCapability::KeyboardTopRowLayout::
                           kKbdTopRowLayoutWilco ||
             layout == KeyboardCapability::KeyboardTopRowLayout::
                           kKbdTopRowLayoutDrallion) {
    if (RewriteTopRowKeysForLayoutWilco(key_event, search_is_pressed, state,
                                        layout)) {
      return;
    }
  } else if ((state->key_code >= VKEY_F1) && (state->key_code <= VKEY_F12)) {
    //  Search? Top Row  Rewrite Meta F-Key Result
    //  ------- -------- ------------------ ------
    //  No      Fn       Yes                Unchanged
    //  No      System   Yes                Fn -> System
    //  Yes     Fn       Yes                Fn -> System
    //  Yes     System   Yes                Search+Fn -> Fn
    //  No      Fn       No                 Unchanged
    //  No      System   No                 Fn -> System
    //  Yes     Fn       No                 Unchanged
    //  Yes     System   No                 Unchanged
    if (ForceTopRowAsFunctionKeys(key_event.source_device_id()) ==
        flip_remapping) {
      // Rewrite the F1-F12 keys on a Chromebook keyboard to system keys.
      // This is the original Chrome OS layout.
      static const KeyboardRemapping kFkeysToSystemKeys1[] = {
          {{EF_NONE, VKEY_F1},
           {EF_NONE, DomCode::BROWSER_BACK, DomKey::BROWSER_BACK,
            VKEY_BROWSER_BACK}},
          {{EF_NONE, VKEY_F2},
           {EF_NONE, DomCode::BROWSER_FORWARD, DomKey::BROWSER_FORWARD,
            VKEY_BROWSER_FORWARD}},
          {{EF_NONE, VKEY_F3},
           {EF_NONE, DomCode::BROWSER_REFRESH, DomKey::BROWSER_REFRESH,
            VKEY_BROWSER_REFRESH}},
          {{EF_NONE, VKEY_F4},
           {EF_NONE, DomCode::ZOOM_TOGGLE, DomKey::ZOOM_TOGGLE, VKEY_ZOOM}},
          {{EF_NONE, VKEY_F5},
           {EF_NONE, DomCode::SELECT_TASK, DomKey::LAUNCH_MY_COMPUTER,
            VKEY_MEDIA_LAUNCH_APP1}},
          {{EF_NONE, VKEY_F6},
           {EF_NONE, DomCode::BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN,
            VKEY_BRIGHTNESS_DOWN}},
          {{EF_NONE, VKEY_F7},
           {EF_NONE, DomCode::BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP,
            VKEY_BRIGHTNESS_UP}},
          {{EF_NONE, VKEY_F8},
           {EF_NONE, DomCode::VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE,
            VKEY_VOLUME_MUTE}},
          {{EF_NONE, VKEY_F9},
           {EF_NONE, DomCode::VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN,
            VKEY_VOLUME_DOWN}},
          {{EF_NONE, VKEY_F10},
           {EF_NONE, DomCode::VOLUME_UP, DomKey::AUDIO_VOLUME_UP,
            VKEY_VOLUME_UP}},
      };
      // The new layout with forward button removed and play/pause added.
      static const KeyboardRemapping kFkeysToSystemKeys2[] = {
          {{EF_NONE, VKEY_F1},
           {EF_NONE, DomCode::BROWSER_BACK, DomKey::BROWSER_BACK,
            VKEY_BROWSER_BACK}},
          {{EF_NONE, VKEY_F2},
           {EF_NONE, DomCode::BROWSER_REFRESH, DomKey::BROWSER_REFRESH,
            VKEY_BROWSER_REFRESH}},
          {{EF_NONE, VKEY_F3},
           {EF_NONE, DomCode::ZOOM_TOGGLE, DomKey::ZOOM_TOGGLE, VKEY_ZOOM}},
          {{EF_NONE, VKEY_F4},
           {EF_NONE, DomCode::SELECT_TASK, DomKey::LAUNCH_MY_COMPUTER,
            VKEY_MEDIA_LAUNCH_APP1}},
          {{EF_NONE, VKEY_F5},
           {EF_NONE, DomCode::BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN,
            VKEY_BRIGHTNESS_DOWN}},
          {{EF_NONE, VKEY_F6},
           {EF_NONE, DomCode::BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP,
            VKEY_BRIGHTNESS_UP}},
          {{EF_NONE, VKEY_F7},
           {EF_NONE, DomCode::MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE,
            VKEY_MEDIA_PLAY_PAUSE}},
          {{EF_NONE, VKEY_F8},
           {EF_NONE, DomCode::VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE,
            VKEY_VOLUME_MUTE}},
          {{EF_NONE, VKEY_F9},
           {EF_NONE, DomCode::VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN,
            VKEY_VOLUME_DOWN}},
          {{EF_NONE, VKEY_F10},
           {EF_NONE, DomCode::VOLUME_UP, DomKey::AUDIO_VOLUME_UP,
            VKEY_VOLUME_UP}},
      };

      const KeyboardRemapping* mapping = nullptr;
      size_t mappingSize = 0u;
      switch (layout) {
        case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2:
          mapping = kFkeysToSystemKeys2;
          mappingSize = std::size(kFkeysToSystemKeys2);
          break;
        case KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1:
        default:
          mapping = kFkeysToSystemKeys1;
          mappingSize = std::size(kFkeysToSystemKeys1);
          break;
      }

      MutableKeyState incoming_without_command = *state;
      incoming_without_command.flags &= ~EF_COMMAND_DOWN;
      if (RewriteWithKeyboardRemappings(mapping, mappingSize,
                                        incoming_without_command, state)) {
        // If the remapping was not supposed to be flipped and search is
        // pressed, the search flag must be added back.
        if (!flip_remapping && search_is_pressed) {
          state->flags |= EF_COMMAND_DOWN;
        }
        return;
      }
    } else if (flip_remapping) {
      // If we were supposed to flip the remapping, that means we should remove
      // the search flag.
      state->flags &= ~EF_COMMAND_DOWN;
      return;
    }
  }

  // TODO(crbug.com/1179893): Remove this entire block when
  // IsImprovedKeyboardShortcutsEnabled is always on.
  if (state->flags & EF_COMMAND_DOWN) {
    const bool strict = ::features::IsNewShortcutMappingEnabled();
    struct SearchToFunctionMap {
      DomCode input_dom_code;
      MutableKeyState result;
    };

    // We check the DOM3 |code| here instead of the VKEY, as these keys may
    // have different |KeyboardCode|s when modifiers are pressed, such as
    // shift.
    if (strict) {
      DCHECK(!::features::IsImprovedKeyboardShortcutsEnabled());
      // Remap Search + 1/2 to F11/12.
      static const SearchToFunctionMap kNumberKeysToFkeys[] = {
          {DomCode::DIGIT1, {EF_NONE, DomCode::F11, DomKey::F12, VKEY_F11}},
          {DomCode::DIGIT2, {EF_NONE, DomCode::F12, DomKey::F12, VKEY_F12}},
      };
      for (const auto& map : kNumberKeysToFkeys) {
        if (state->code == map.input_dom_code) {
          state->flags &= ~EF_COMMAND_DOWN;
          ApplyRemapping(map.result, state);
          return;
        }
      }
    } else {
      // Remap Search + digit row to F1~F12.
      static const SearchToFunctionMap kNumberKeysToFkeys[] = {
          {DomCode::DIGIT1, {EF_NONE, DomCode::F1, DomKey::F1, VKEY_F1}},
          {DomCode::DIGIT2, {EF_NONE, DomCode::F2, DomKey::F2, VKEY_F2}},
          {DomCode::DIGIT3, {EF_NONE, DomCode::F3, DomKey::F3, VKEY_F3}},
          {DomCode::DIGIT4, {EF_NONE, DomCode::F4, DomKey::F4, VKEY_F4}},
          {DomCode::DIGIT5, {EF_NONE, DomCode::F5, DomKey::F5, VKEY_F5}},
          {DomCode::DIGIT6, {EF_NONE, DomCode::F6, DomKey::F6, VKEY_F6}},
          {DomCode::DIGIT7, {EF_NONE, DomCode::F7, DomKey::F7, VKEY_F7}},
          {DomCode::DIGIT8, {EF_NONE, DomCode::F8, DomKey::F8, VKEY_F8}},
          {DomCode::DIGIT9, {EF_NONE, DomCode::F9, DomKey::F9, VKEY_F9}},
          {DomCode::DIGIT0, {EF_NONE, DomCode::F10, DomKey::F10, VKEY_F10}},
          {DomCode::MINUS, {EF_NONE, DomCode::F11, DomKey::F11, VKEY_F11}},
          {DomCode::EQUAL, {EF_NONE, DomCode::F12, DomKey::F12, VKEY_F12}}};
      for (const auto& map : kNumberKeysToFkeys) {
        if (state->code == map.input_dom_code) {
          if (!::features::IsImprovedKeyboardShortcutsEnabled()) {
            state->flags &= ~EF_COMMAND_DOWN;
            ApplyRemapping(map.result, state);
            RecordSearchPlusDigitFKeyRewrite(key_event.type(), state->key_code);
          }
          return;
        }
      }
    }
  }
}

int EventRewriterAsh::RewriteLocatedEvent(const Event& event) {
  if (!delegate_) {
    return event.flags();
  }
  return GetRemappedModifierMasks(event, event.flags());
}

int EventRewriterAsh::RewriteModifierClick(const MouseEvent& mouse_event,
                                           int* flags) {
  // Note that this behavior is limited to mouse events coming from touchpad
  // devices. https://crbug.com/890648.

  // Remap either Alt+Button1 or Search+Button1 to Button3 based on
  // flag/setting.
  int matched_mask;
  bool matched_alt_deprecation;
  if (ShouldRemapToRightClick(mouse_event, *flags, &matched_mask,
                              &matched_alt_deprecation)) {
    // If the rewrite matched the deprecation message should also not occur.
    DCHECK(!matched_alt_deprecation);

    *flags &= ~matched_mask;
    *flags |= EF_RIGHT_MOUSE_BUTTON;
    if (mouse_event.type() == ET_MOUSE_PRESSED) {
      pressed_as_right_button_device_ids_.insert(
          mouse_event.source_device_id());
      if (matched_mask == kSearchLeftButton) {
        base::RecordAction(
            base::UserMetricsAction("SearchClickMappedToRightClick"));
      } else {
        DCHECK(matched_mask == kAltLeftButton);
        base::RecordAction(
            base::UserMetricsAction("AltClickMappedToRightClick"));
      }
    } else {
      pressed_as_right_button_device_ids_.erase(mouse_event.source_device_id());
    }
    return EF_RIGHT_MOUSE_BUTTON;
  } else if (matched_alt_deprecation) {
    delegate_->NotifyDeprecatedRightClickRewrite();
  }
  return EF_NONE;
}

EventDispatchDetails EventRewriterAsh::RewriteKeyEventInContext(
    const KeyEvent& key_event,
    std::unique_ptr<Event> rewritten_event,
    EventRewriteStatus status,
    const Continuation continuation) {
  if (status == EventRewriteStatus::EVENT_REWRITE_DISCARD) {
    return DiscardEvent(continuation);
  }

  MutableKeyState current_key_state;
  auto key_state_comparator =
      [&current_key_state](
          const std::pair<MutableKeyState, MutableKeyState>& key_state) {
        return (current_key_state.code == key_state.first.code) &&
               (current_key_state.key == key_state.first.key) &&
               (current_key_state.key_code == key_state.first.key_code);
      };

  const int mapped_flag = ModifierDomKeyToEventFlag(key_event.GetDomKey());

  if (key_event.type() == ET_KEY_PRESSED) {
    current_key_state = MutableKeyState(
        rewritten_event ? static_cast<const KeyEvent*>(rewritten_event.get())
                        : &key_event);
    MutableKeyState original_key_state(&key_event);
    auto iter =
        base::ranges::find_if(pressed_key_states_, key_state_comparator);

    // When a key is pressed, store |current_key_state| if it is not stored
    // before.
    if (iter == pressed_key_states_.end()) {
      pressed_key_states_.emplace_back(current_key_state, original_key_state);
    }

    if (status == EventRewriteStatus::EVENT_REWRITE_CONTINUE) {
      return SendEvent(continuation, &key_event);
    }

    EventDispatchDetails details =
        SendEventFinally(continuation, rewritten_event.get());
    if (status == EventRewriteStatus::EVENT_REWRITE_DISPATCH_ANOTHER &&
        !details.dispatcher_destroyed) {
      return SendStickyKeysReleaseEvents(std::move(rewritten_event),
                                         continuation);
    }
    return details;
  }

  DCHECK_EQ(key_event.type(), ET_KEY_RELEASED);

  if (mapped_flag != EF_NONE) {
    // The released key is a modifier

    DomKey::Base current_key = key_event.GetDomKey();
    auto key_state_iter = pressed_key_states_.begin();
    int event_flags =
        rewritten_event ? rewritten_event->flags() : key_event.flags();
    rewritten_event.reset();

    // Iterate the keys being pressed. Release the key events which satisfy one
    // of the following conditions:
    // (1) the key event's original key code (before key event rewriting if
    // any) is the same with the key to be released.
    // (2) the key event is rewritten and its original flags are influenced by
    // the key to be released.
    // Example: Press the Launcher button, Press the Up Arrow button, Release
    // the Launcher button. When Launcher is released: the key event whose key
    // code is Launcher should be released because it satisfies the condition 1;
    // the key event whose key code is PageUp should be released because it
    // satisfies the condition 2.
    EventDispatchDetails details;
    while (key_state_iter != pressed_key_states_.end() &&
           !details.dispatcher_destroyed) {
      const bool is_rewritten =
          (key_state_iter->first.key != key_state_iter->second.key);
      const bool flag_affected = key_state_iter->second.flags & mapped_flag;
      const bool should_release = key_state_iter->second.key == current_key ||
                                  (flag_affected && is_rewritten);

      if (should_release) {
        // If the key should be released, create a key event for it.
        auto dispatched_event = std::make_unique<KeyEvent>(
            key_event.type(), key_state_iter->first.key_code,
            key_state_iter->first.code, event_flags, key_state_iter->first.key,
            key_event.time_stamp());
        dispatched_event->set_scan_code(key_event.scan_code());
        details = SendEventFinally(continuation, dispatched_event.get());

        key_state_iter = pressed_key_states_.erase(key_state_iter);
        continue;
      }
      key_state_iter++;
    }
    return details;
  }

  // The released key is not a modifier

  current_key_state = MutableKeyState(
      rewritten_event ? static_cast<const KeyEvent*>(rewritten_event.get())
                      : &key_event);
  auto iter = base::ranges::find_if(pressed_key_states_, key_state_comparator);
  if (iter != pressed_key_states_.end()) {
    pressed_key_states_.erase(iter);

    if (status == EventRewriteStatus::EVENT_REWRITE_CONTINUE) {
      return SendEvent(continuation, &key_event);
    }

    EventDispatchDetails details =
        SendEventFinally(continuation, rewritten_event.get());
    if (status == EventRewriteStatus::EVENT_REWRITE_DISPATCH_ANOTHER &&
        !details.dispatcher_destroyed) {
      return SendStickyKeysReleaseEvents(std::move(rewritten_event),
                                         continuation);
    }
    return details;
  }

  // Event rewriting may create a meaningless key event.
  // For example: press the Up Arrow button, press the Launcher button,
  // release the Up Arrow. When the Up Arrow button is released, key event
  // rewriting happens. However, the rewritten event is not among
  // |pressed_key_states_|. So it should be blocked and the original event
  // should be propagated.
  return SendEvent(continuation, &key_event);
}

// New CrOS keyboards differ from previous Chrome OS keyboards in a few
// ways. Previous keyboards always sent F1-Fxx keys and allowed Chrome to
// decide how to interpret them. New CrOS keyboards now always send action
// keys (eg. Back, Refresh, Overview). So while the default previously was
// to always expect to remap F-Key to action key, for these devices respect
// what the keyboard sends unless the user overrides with either the Search
// key or the "Top Row is always F-Key" setting.
//
// Additionally, these keyboards provide the mapping via sysfs so each
// new keyboard does not need to be explicitly special cased in the future.
//
//  Search  Force function keys Rewrite Meta F-Key  Key code   Result
//  ------- ------------------- ------------------  --------   ------
//  No        No                Yes                 Function   Unchanged
//  Yes       No                Yes                 Function   Unchanged
//  No        Yes               Yes                 Function   Unchanged
//  Yes       Yes               Yes                 Function   Unchanged
//  No        No                Yes                 Action     Unchanged
//  Yes       No                Yes                 Action     Action -> Fn
//  No        Yes               Yes                 Action     Action -> Fn
//  Yes       Yes               Yes                 Action     Unchanged
//  No        No                No                  Function   Unchanged
//  Yes       No                No                  Function   Unchanged
//  No        Yes               No                  Function   Unchanged
//  Yes       Yes               No                  Function   Unchanged
//  No        No                No                  Action     Unchanged
//  Yes       No                No                  Action     Unchanged
//  No        Yes               No                  Action     Unchanged
//  Yes       Yes               No                  Action     Unchanged
bool EventRewriterAsh::RewriteTopRowKeysForCustomLayout(
    int device_id,
    const KeyEvent& key_event,
    bool search_is_pressed,
    EventRewriterAsh::MutableKeyState* state) {
  // Incoming function keys are never remapped.
  if (IsCustomLayoutFunctionKey(key_event.key_code())) {
    return true;
  }

  const bool flip_remapping = ShouldRewriteMetaTopRowKeyComboEvents(
                                  delegate_, last_keyboard_device_id_) &&
                              search_is_pressed;

  const auto* scan_code_vector_ptr =
      keyboard_capability_->GetTopRowScanCodes(device_id);
  if (!scan_code_vector_ptr || scan_code_vector_ptr->empty()) {
    LOG(WARNING) << "Found no top row key mapping for device " << device_id;
    return false;
  }

  const auto& scan_code_vector = *scan_code_vector_ptr;
  const auto& key_iter =
      base::ranges::find(scan_code_vector, key_event.scan_code());

  // If the scan code appears in the top row mapping it is an action key.
  const bool is_action_key = (key_iter != scan_code_vector.end());
  if (is_action_key) {
    if (flip_remapping !=
        ForceTopRowAsFunctionKeys(key_event.source_device_id())) {
      ApplyRemapping(kCustomTopRowLayoutFKeys[std::distance(
                         scan_code_vector.begin(), key_iter)],
                     state);
    }

    // Clear command/search key if pressed and we were supposed to perform a
    // remapping.
    if (flip_remapping) {
      state->flags &= ~EF_COMMAND_DOWN;
    }

    return true;
  }

  return false;
}

// The keyboard layout for Wilco has a slightly different top-row layout, emits
// both Fn and action keys from kernel and has key events with Dom codes and no
// VKey value == VKEY_UNKNOWN. Depending on the state of the search key and
// force-function-key preference, function keys have to be mapped to action keys
// or vice versa.
//
//  Search  Force function keys Rewrite Meta F-Key  Key code   Result
//  ------- ------------------- ------------------  --------   ------
//  No      No                  Yes                 Function   Unchanged
//  Yes     No                  Yes                 Function   Fn -> Action
//  No      Yes                 Yes                 Function   Unchanged
//  Yes     Yes                 Yes                 Function   Fn -> Action
//  No      No                  Yes                 Action     Unchanged
//  Yes     No                  Yes                 Action     Action -> Fn
//  No      Yes                 Yes                 Action     Action -> Fn
//  Yes     Yes                 Yes                 Action     Unchanged
//  No      No                  No                  Function   Unchanged
//  Yes     No                  No                  Function   Unchanged
//  No      Yes                 No                  Function   Unchanged
//  Yes     Yes                 No                  Function   Unchanged
//  No      No                  No                  Action     Unchanged
//  Yes     No                  No                  Action     Unchanged
//  No      Yes                 No                  Action     Action -> Fn
//  Yes     Yes                 No                  Action     Action -> Fn
bool EventRewriterAsh::RewriteTopRowKeysForLayoutWilco(
    const KeyEvent& key_event,
    bool search_is_pressed,
    MutableKeyState* state,
    KeyboardCapability::KeyboardTopRowLayout layout) {
  // When the kernel issues an function key (Fn modifier help down) and the
  // search key is pressed, the function key needs to be mapped to its
  // corresponding action key. This table defines those function-to-action
  // mappings.
  static const KeyboardRemapping kFnkeysToActionKeys[] = {
      {{EF_NONE, VKEY_F1},
       {EF_NONE, DomCode::BROWSER_BACK, DomKey::BROWSER_BACK,
        VKEY_BROWSER_BACK}},
      {{EF_NONE, VKEY_F2},
       {EF_NONE, DomCode::BROWSER_REFRESH, DomKey::BROWSER_REFRESH,
        VKEY_BROWSER_REFRESH}},
      // Map F3 to VKEY_ZOOM + EF_NONE == toggle full screen:
      {{EF_NONE, VKEY_F3},
       {EF_NONE, DomCode::ZOOM_TOGGLE, DomKey::ZOOM_TOGGLE, VKEY_ZOOM}},
      // Map F4 to VKEY_MEDIA_LAUNCH_APP1 + EF_NONE == overview:
      {{EF_NONE, VKEY_F4},
       {EF_NONE, DomCode::F4, DomKey::F4, VKEY_MEDIA_LAUNCH_APP1}},
      {{EF_NONE, VKEY_F5},
       {EF_NONE, DomCode::BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN,
        VKEY_BRIGHTNESS_DOWN}},
      {{EF_NONE, VKEY_F6},
       {EF_NONE, DomCode::BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP,
        VKEY_BRIGHTNESS_UP}},
      {{EF_NONE, VKEY_F7},
       {EF_NONE, DomCode::VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE,
        VKEY_VOLUME_MUTE}},
      {{EF_NONE, VKEY_F8},
       {EF_NONE, DomCode::VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN,
        VKEY_VOLUME_DOWN}},
      {{EF_NONE, VKEY_F9},
       {EF_NONE, DomCode::VOLUME_UP, DomKey::AUDIO_VOLUME_UP, VKEY_VOLUME_UP}},
      // Note: F10 and F11 are left as-is since no action is associated with
      // these keys.
      {{EF_NONE, VKEY_F10}, {EF_NONE, DomCode::F10, DomKey::F10, VKEY_F10}},
      {{EF_NONE, VKEY_F11}, {EF_NONE, DomCode::F11, DomKey::F11, VKEY_F11}},
      {{EF_NONE, VKEY_F12},
       // Map F12 to VKEY_ZOOM + EF_CONTROL_DOWN == toggle mirror
       // mode:
       {EF_CONTROL_DOWN, DomCode::F12, DomKey::F12, VKEY_ZOOM}},
  };

  // When the kernel issues an action key (default mode) and the search key is
  // pressed, the action key needs to be mapped back to its corresponding
  // action key. This table defines those action-to-function mappings. Note:
  // this table is essentially the dual of kFnToActionLeys above.
  static const KeyboardRemapping kActionToFnKeys[] = {
      {{EF_NONE, VKEY_BROWSER_BACK},
       {EF_NONE, DomCode::F1, DomKey::F1, VKEY_F1}},
      {{EF_NONE, VKEY_BROWSER_REFRESH},
       {EF_NONE, DomCode::F2, DomKey::F2, VKEY_F2}},
      {{EF_NONE, VKEY_MEDIA_LAUNCH_APP1},
       {EF_NONE, DomCode::F4, DomKey::F4, VKEY_F4}},
      {{EF_NONE, VKEY_BRIGHTNESS_DOWN},
       {EF_NONE, DomCode::F5, DomKey::F5, VKEY_F5}},
      {{EF_NONE, VKEY_BRIGHTNESS_UP},
       {EF_NONE, DomCode::F6, DomKey::F6, VKEY_F6}},
      {{EF_NONE, VKEY_VOLUME_MUTE},
       {EF_NONE, DomCode::F7, DomKey::F7, VKEY_F7}},
      {{EF_NONE, VKEY_VOLUME_DOWN},
       {EF_NONE, DomCode::F8, DomKey::F8, VKEY_F8}},
      {{EF_NONE, VKEY_VOLUME_UP}, {EF_NONE, DomCode::F9, DomKey::F9, VKEY_F9}},
      // Do not change the order of the next two entries. The remapping of
      // VKEY_ZOOM with Control held down must appear before
      // VKEY_ZOOM by itself to be considered.
      {{EF_CONTROL_DOWN, VKEY_ZOOM},
       {EF_NONE, DomCode::F12, DomKey::F12, VKEY_F12}},
      {{EF_NONE, VKEY_ZOOM}, {EF_NONE, DomCode::F3, DomKey::F3, VKEY_F3}},
      // VKEY_PRIVACY_SCREEN_TOGGLE shares a key with F12 on Drallion.
      {{EF_NONE, VKEY_PRIVACY_SCREEN_TOGGLE},
       {EF_NONE, DomCode::F12, DomKey::F12, VKEY_F12}},
  };
  const bool flip_remapping = ShouldRewriteMetaTopRowKeyComboEvents(
                                  delegate_, last_keyboard_device_id_) &&
                              search_is_pressed;
  MutableKeyState incoming_with_command_removed_if_neccessary = *state;
  if (flip_remapping) {
    incoming_with_command_removed_if_neccessary.flags &= ~EF_COMMAND_DOWN;
  }

  if ((state->key_code >= VKEY_F1) && (state->key_code <= VKEY_F12)) {
    // Incoming key code is a Fn key. Check if it needs to be mapped back to its
    // corresponding action key.
    if (flip_remapping) {
      // On some Drallion devices, F12 shares a key with privacy screen toggle.
      // Account for this before rewriting for Wilco 1.0 layout.
      if (layout == KeyboardCapability::KeyboardTopRowLayout::
                        kKbdTopRowLayoutDrallion &&
          state->key_code == VKEY_F12) {
        if (privacy_screen_supported_) {
          state->key_code = VKEY_PRIVACY_SCREEN_TOGGLE;
          state->code = DomCode::PRIVACY_SCREEN_TOGGLE;
        }
        // Clear command flag if the remapping should be flipped.
        state->flags = (state->flags & ~EF_COMMAND_DOWN);
        return true;
      }
      return RewriteWithKeyboardRemappings(
          kFnkeysToActionKeys, std::size(kFnkeysToActionKeys),
          incoming_with_command_removed_if_neccessary, state);
    }
    return true;
  } else if (IsKeyCodeInMappings(state->key_code, kActionToFnKeys,
                                 std::size(kActionToFnKeys))) {
    // Incoming key code is an action key. Check if it needs to be mapped back
    // to its corresponding function key.
    if (flip_remapping !=
        ForceTopRowAsFunctionKeys(key_event.source_device_id())) {
      // On Drallion, mirror mode toggle is on its own key so don't remap it.
      if (layout == KeyboardCapability::KeyboardTopRowLayout::
                        kKbdTopRowLayoutDrallion &&
          MatchKeyboardRemapping(*state, {EF_CONTROL_DOWN, VKEY_ZOOM})) {
        // Clear command flag before returning if the remapping should be
        // flipped.
        if (flip_remapping) {
          state->flags = (state->flags & ~EF_COMMAND_DOWN);
        }
        return true;
      }
      return RewriteWithKeyboardRemappings(
          kActionToFnKeys, std::size(kActionToFnKeys),
          incoming_with_command_removed_if_neccessary, state);
    }
    // Remap Privacy Screen Toggle to F12 on Drallion devices that do not have
    // privacy screens.
    if (layout == KeyboardCapability::KeyboardTopRowLayout::
                      kKbdTopRowLayoutDrallion &&
        !privacy_screen_supported_ &&
        MatchKeyboardRemapping(*state, {EF_NONE, VKEY_PRIVACY_SCREEN_TOGGLE})) {
      state->key_code = VKEY_F12;
      state->code = DomCode::F12;
      state->key = DomKey::F12;
    }
    // If the mapping should be flipped when command is down, the flag needs to
    // be cleared.
    if (flip_remapping) {
      state->flags &= ~EF_COMMAND_DOWN;
    }
    return true;
  }

  return false;
}

bool EventRewriterAsh::ForceTopRowAsFunctionKeys(int device_id) const {
  return delegate_ && delegate_->TopRowKeysAreFunctionKeys(device_id);
}

EventDispatchDetails EventRewriterAsh::SendStickyKeysReleaseEvents(
    std::unique_ptr<Event> rewritten_event,
    const Continuation continuation) {
  EventDispatchDetails details;
  std::unique_ptr<Event> last_sent_event = std::move(rewritten_event);
  while (sticky_keys_controller_ && !details.dispatcher_destroyed) {
    std::unique_ptr<Event> new_event;
    EventRewriteStatus status = sticky_keys_controller_->NextDispatchEvent(
        *last_sent_event, &new_event);
    details = SendEventFinally(continuation, new_event.get());
    last_sent_event = std::move(new_event);
    if (status != EventRewriteStatus::EVENT_REWRITE_DISPATCH_ANOTHER) {
      return details;
    }
  }
  return details;
}

}  // namespace ui
