// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ui/events/keycodes/keyboard_code_conversion_starboard.h"

#include <vector>

#include "starboard/key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

namespace {

// A comprehensive list of all SbKey values from starboard/key.h.
const std::vector<SbKey> kAllSbKeys = {
    kSbKeyUnknown,
    kSbKeyCancel,
    kSbKeyBackspace,
    kSbKeyTab,
    kSbKeyBacktab,
    kSbKeyClear,
    kSbKeyReturn,
    kSbKeyShift,
    kSbKeyControl,
    kSbKeyMenu,
    kSbKeyPause,
    kSbKeyCapital,
    kSbKeyKana,
    kSbKeyHangul,
    kSbKeyJunja,
    kSbKeyFinal,
    kSbKeyHanja,
    kSbKeyKanji,
    kSbKeyEscape,
    kSbKeyConvert,
    kSbKeyNonconvert,
    kSbKeyAccept,
    kSbKeyModechange,
    kSbKeySpace,
    kSbKeyPrior,
    kSbKeyNext,
    kSbKeyEnd,
    kSbKeyHome,
    kSbKeyLeft,
    kSbKeyUp,
    kSbKeyRight,
    kSbKeyDown,
    kSbKeySelect,
    kSbKeyPrint,
    kSbKeyExecute,
    kSbKeySnapshot,
    kSbKeyInsert,
    kSbKeyDelete,
    kSbKeyHelp,
    kSbKey0,
    kSbKey1,
    kSbKey2,
    kSbKey3,
    kSbKey4,
    kSbKey5,
    kSbKey6,
    kSbKey7,
    kSbKey8,
    kSbKey9,
    kSbKeyA,
    kSbKeyB,
    kSbKeyC,
    kSbKeyD,
    kSbKeyE,
    kSbKeyF,
    kSbKeyG,
    kSbKeyH,
    kSbKeyI,
    kSbKeyJ,
    kSbKeyK,
    kSbKeyL,
    kSbKeyM,
    kSbKeyN,
    kSbKeyO,
    kSbKeyP,
    kSbKeyQ,
    kSbKeyR,
    kSbKeyS,
    kSbKeyT,
    kSbKeyU,
    kSbKeyV,
    kSbKeyW,
    kSbKeyX,
    kSbKeyY,
    kSbKeyZ,
    kSbKeyLwin,
    kSbKeyRwin,
    kSbKeyApps,
    kSbKeySleep,
    kSbKeyNumpad0,
    kSbKeyNumpad1,
    kSbKeyNumpad2,
    kSbKeyNumpad3,
    kSbKeyNumpad4,
    kSbKeyNumpad5,
    kSbKeyNumpad6,
    kSbKeyNumpad7,
    kSbKeyNumpad8,
    kSbKeyNumpad9,
    kSbKeyMultiply,
    kSbKeyAdd,
    kSbKeySeparator,
    kSbKeySubtract,
    kSbKeyDecimal,
    kSbKeyDivide,
    kSbKeyF1,
    kSbKeyF2,
    kSbKeyF3,
    kSbKeyF4,
    kSbKeyF5,
    kSbKeyF6,
    kSbKeyF7,
    kSbKeyF8,
    kSbKeyF9,
    kSbKeyF10,
    kSbKeyF11,
    kSbKeyF12,
    kSbKeyF13,
    kSbKeyF14,
    kSbKeyF15,
    kSbKeyF16,
    kSbKeyF17,
    kSbKeyF18,
    kSbKeyF19,
    kSbKeyF20,
    kSbKeyF21,
    kSbKeyF22,
    kSbKeyF23,
    kSbKeyF24,
    kSbKeyNumlock,
    kSbKeyScroll,
    kSbKeyWlan,
    kSbKeyPower,
    kSbKeyLshift,
    kSbKeyRshift,
    kSbKeyLcontrol,
    kSbKeyRcontrol,
    kSbKeyLmenu,
    kSbKeyRmenu,
    kSbKeyBrowserBack,
    kSbKeyBrowserForward,
    kSbKeyBrowserRefresh,
    kSbKeyBrowserStop,
    kSbKeyBrowserSearch,
    kSbKeyBrowserFavorites,
    kSbKeyBrowserHome,
    kSbKeyVolumeMute,
    kSbKeyVolumeDown,
    kSbKeyVolumeUp,
    kSbKeyMediaNextTrack,
    kSbKeyMediaPrevTrack,
    kSbKeyMediaStop,
    kSbKeyMediaPlayPause,
    kSbKeyMediaLaunchMail,
    kSbKeyMediaLaunchMediaSelect,
    kSbKeyMediaLaunchApp1,
    kSbKeyMediaLaunchApp2,
    kSbKeyOem1,
    kSbKeyOemPlus,
    kSbKeyOemComma,
    kSbKeyOemMinus,
    kSbKeyOemPeriod,
    kSbKeyOem2,
    kSbKeyOem3,
    kSbKeyBrightnessDown,
    kSbKeyBrightnessUp,
    kSbKeyKbdBrightnessDown,
    kSbKeyOem4,
    kSbKeyOem5,
    kSbKeyOem6,
    kSbKeyOem7,
    kSbKeyOem8,
    kSbKeyOem102,
    kSbKeyKbdBrightnessUp,
    kSbKeyDbeSbcschar,
    kSbKeyDbeDbcschar,
    kSbKeyPlay,
    kSbKeyMediaRewind,
    kSbKeyMediaFastForward,
    kSbKeyRed,
    kSbKeyGreen,
    kSbKeyYellow,
    kSbKeyBlue,
    kSbKeyRecord,
    kSbKeyChannelUp,
    kSbKeyChannelDown,
    kSbKeySubtitle,
    kSbKeyInfo,
    kSbKeyGuide,
    kSbKeyLast,
    kSbKeyInstantReplay,
    kSbKeyLaunchThisApplication,
    kSbKeyMediaAudioTrack,
    kSbKeyMicrophone,
    kSbKeyMouse1,
    kSbKeyMouse2,
    kSbKeyMouse3,
    kSbKeyMouse4,
    kSbKeyMouse5,
    kSbKeyGamepad1,
    kSbKeyGamepad2,
    kSbKeyGamepad3,
    kSbKeyGamepad4,
    kSbKeyGamepadLeftBumper,
    kSbKeyGamepadRightBumper,
    kSbKeyGamepadLeftTrigger,
    kSbKeyGamepadRightTrigger,
    kSbKeyGamepad5,
    kSbKeyGamepad6,
    kSbKeyGamepadLeftStick,
    kSbKeyGamepadRightStick,
    kSbKeyGamepadDPadUp,
    kSbKeyGamepadDPadDown,
    kSbKeyGamepadDPadLeft,
    kSbKeyGamepadDPadRight,
    kSbKeyGamepadSystem,
    kSbKeyGamepadLeftStickUp,
    kSbKeyGamepadLeftStickDown,
    kSbKeyGamepadLeftStickLeft,
    kSbKeyGamepadLeftStickRight,
    kSbKeyGamepadRightStickUp,
    kSbKeyGamepadRightStickDown,
    kSbKeyGamepadRightStickLeft,
    kSbKeyGamepadRightStickRight};

}  // namespace

TEST(KeyboardCodeConversionStarboardTest, AllSbKeysAreMappedToKeyboardCode) {
  // Add SbKey values here that are intentionally not mapped to a valid
  // KeyboardCode.
  const std::vector<SbKey> unmapped_keyboard_codes = {
      kSbKeyUnknown,
      kSbKeyRed,
      kSbKeyGreen,
      kSbKeyYellow,
      kSbKeyBlue,
      kSbKeyRecord,
      kSbKeyInfo,
      kSbKeyGuide,
      kSbKeyLast,
      kSbKeyInstantReplay,
      kSbKeyLaunchThisApplication,
      kSbKeyMediaAudioTrack,
      // Gamepad and mouse keys do not map to keyboard codes.
      kSbKeyMouse1,
      kSbKeyMouse2,
      kSbKeyMouse3,
      kSbKeyMouse4,
      kSbKeyMouse5,
      kSbKeyGamepad1,
      kSbKeyGamepad2,
      kSbKeyGamepad3,
      kSbKeyGamepad4,
      kSbKeyGamepadLeftBumper,
      kSbKeyGamepadRightBumper,
      kSbKeyGamepadLeftTrigger,
      kSbKeyGamepadRightTrigger,
      kSbKeyGamepad5,
      kSbKeyGamepad6,
      kSbKeyGamepadLeftStick,
      kSbKeyGamepadRightStick,
      kSbKeyGamepadSystem,
      kSbKeyGamepadLeftStickUp,
      kSbKeyGamepadLeftStickDown,
      kSbKeyGamepadLeftStickLeft,
      kSbKeyGamepadLeftStickRight,
      kSbKeyGamepadRightStickUp,
      kSbKeyGamepadRightStickDown,
      kSbKeyGamepadRightStickLeft,
      kSbKeyGamepadRightStickRight};

  for (SbKey key : kAllSbKeys) {
    bool is_unmapped = false;
    for (SbKey unmapped_key : unmapped_keyboard_codes) {
      if (key == unmapped_key) {
        is_unmapped = true;
        break;
      }
    }
    if (is_unmapped) {
      EXPECT_EQ(VKEY_UNKNOWN, SbKeyToKeyboardCode(key))
          << "SbKey " << key << " should not be mapped to a KeyboardCode.";
      continue;
    }
    EXPECT_NE(VKEY_UNKNOWN, SbKeyToKeyboardCode(key))
        << "SbKey " << key << " is not mapped to a KeyboardCode.";
  }
}

TEST(KeyboardCodeConversionStarboardTest, AllSbKeysAreMappedToDomKey) {
  // Add SbKey values here that are intentionally not mapped to a valid
  // DomKey.
  const std::vector<SbKey> unmapped_dom_keys = {
      kSbKeyUnknown,
      kSbKeySeparator,
      kSbKeyOem8,
      kSbKeyWlan,
      kSbKeyMediaLaunchMediaSelect,
      kSbKeyKbdBrightnessDown,
      kSbKeyKbdBrightnessUp,
      kSbKeyDbeSbcschar,
      kSbKeyDbeDbcschar,
      kSbKeyLast,
      kSbKeyLaunchThisApplication,
      kSbKeyMouse1,
      kSbKeyMouse2,
      kSbKeyMouse3,
      kSbKeyMouse4,
      kSbKeyMouse5,
      kSbKeyGamepad1,
      kSbKeyGamepad2,
      kSbKeyGamepad3,
      kSbKeyGamepad4,
      kSbKeyGamepadLeftBumper,
      kSbKeyGamepadRightBumper,
      kSbKeyGamepadLeftTrigger,
      kSbKeyGamepadRightTrigger,
      kSbKeyGamepad5,
      kSbKeyGamepad6,
      kSbKeyGamepadLeftStick,
      kSbKeyGamepadRightStick,
      kSbKeyGamepadSystem,
      kSbKeyGamepadLeftStickUp,
      kSbKeyGamepadLeftStickDown,
      kSbKeyGamepadLeftStickLeft,
      kSbKeyGamepadLeftStickRight,
      kSbKeyGamepadRightStickUp,
      kSbKeyGamepadRightStickDown,
      kSbKeyGamepadRightStickLeft,
      kSbKeyGamepadRightStickRight};

  for (SbKey key : kAllSbKeys) {
    bool is_unmapped = false;
    for (SbKey unmapped_key : unmapped_dom_keys) {
      if (key == unmapped_key) {
        is_unmapped = true;
        break;
      }
    }
    if (is_unmapped) {
      EXPECT_EQ(DomKey::UNIDENTIFIED, SbKeyToDomKey(key, false))
          << "SbKey " << key << " should not be mapped to a DomKey.";
      continue;
    }
    EXPECT_NE(DomKey::UNIDENTIFIED, SbKeyToDomKey(key, false))
        << "SbKey " << key << " is not mapped to a DomKey.";
  }
}

TEST(KeyboardCodeConversionStarboardTest, AllSbKeysAreMappedToDomCode) {
  // Add SbKey values here that are intentionally not mapped to a valid
  // DomCode.
  const std::vector<SbKey> unmapped_dom_codes = {
      kSbKeyUnknown,
      kSbKeyMenu,
      kSbKeyJunja,
      kSbKeyFinal,
      kSbKeyAccept,
      kSbKeyModechange,
      kSbKeyWlan,
      kSbKeyOem8,
      kSbKeyDbeSbcschar,
      kSbKeyDbeDbcschar,
      kSbKeySubtitle,
      kSbKeyInstantReplay,
      kSbKeyLaunchThisApplication,
      kSbKeyMediaAudioTrack,
      kSbKeyMouse1,
      kSbKeyMouse2,
      kSbKeyMouse3,
      kSbKeyMouse4,
      kSbKeyMouse5,
      kSbKeyGamepad1,
      kSbKeyGamepad2,
      kSbKeyGamepad3,
      kSbKeyGamepad4,
      kSbKeyGamepadLeftBumper,
      kSbKeyGamepadRightBumper,
      kSbKeyGamepadLeftTrigger,
      kSbKeyGamepadRightTrigger,
      kSbKeyGamepad5,
      kSbKeyGamepad6,
      kSbKeyGamepadLeftStick,
      kSbKeyGamepadRightStick,
      kSbKeyGamepadDPadUp,
      kSbKeyGamepadDPadDown,
      kSbKeyGamepadDPadLeft,
      kSbKeyGamepadDPadRight,
      kSbKeyGamepadSystem,
      kSbKeyGamepadLeftStickUp,
      kSbKeyGamepadLeftStickDown,
      kSbKeyGamepadLeftStickLeft,
      kSbKeyGamepadLeftStickRight,
      kSbKeyGamepadRightStickUp,
      kSbKeyGamepadRightStickDown,
      kSbKeyGamepadRightStickLeft,
      kSbKeyGamepadRightStickRight};

  for (SbKey key : kAllSbKeys) {
    bool is_unmapped = false;
    for (SbKey unmapped_key : unmapped_dom_codes) {
      if (key == unmapped_key) {
        is_unmapped = true;
        break;
      }
    }
    if (is_unmapped) {
      EXPECT_EQ(DomCode::NONE, SbKeyToDomCode(key))
          << "SbKey " << key << " should not be mapped to a DomCode.";
      continue;
    }
    EXPECT_NE(DomCode::NONE, SbKeyToDomCode(key))
        << "SbKey " << key << " is not mapped to a DomCode.";
  }
}

}  // namespace ui
