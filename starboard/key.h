// Copyright 2015 Google Inc. All Rights Reserved.
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

// Module Overview: Starboard Key module
//
// Defines the canonical set of Starboard key codes.

#ifndef STARBOARD_KEY_H_
#define STARBOARD_KEY_H_

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

// A standard set of key codes, ordered by value, that represent each possible
// input key across all kinds of devices. Starboard uses the semi-standard
// Windows virtual key codes documented at:
//   https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx
typedef enum SbKey {
  kSbKeyUnknown = 0,
  kSbKeyCancel = 0x03,
  kSbKeyBackspace = 0x08,
  kSbKeyBack = kSbKeyBackspace,  // You probably want kSbKeyEscape for a
                                 // semantic "back".
  kSbKeyTab = 0x09,
  kSbKeyBacktab = 0x0A,
  kSbKeyClear = 0x0C,
  kSbKeyReturn = 0x0D,  // Return/Enter key
  kSbKeyShift = 0x10,
  kSbKeyControl = 0x11,
  kSbKeyMenu = 0x12,  // Alt key
  kSbKeyPause = 0x13,
  kSbKeyCapital = 0x14,
  kSbKeyKana = 0x15,
  kSbKeyHangul = 0x15,
  kSbKeyJunja = 0x17,
  kSbKeyFinal = 0x18,
  kSbKeyHanja = 0x19,
  kSbKeyKanji = 0x19,
  kSbKeyEscape = 0x1B,
  kSbKeyConvert = 0x1C,
  kSbKeyNonconvert = 0x1D,
  kSbKeyAccept = 0x1E,
  kSbKeyModechange = 0x1F,
  kSbKeySpace = 0x20,
  kSbKeyPrior = 0x21,
  kSbKeyNext = 0x22,
  kSbKeyEnd = 0x23,
  kSbKeyHome = 0x24,
  kSbKeyLeft = 0x25,
  kSbKeyUp = 0x26,
  kSbKeyRight = 0x27,
  kSbKeyDown = 0x28,
  kSbKeySelect = 0x29,
  kSbKeyPrint = 0x2A,
  kSbKeyExecute = 0x2B,
  kSbKeySnapshot = 0x2C,
  kSbKeyInsert = 0x2D,
  kSbKeyDelete = 0x2E,
  kSbKeyHelp = 0x2F,
  kSbKey0 = 0x30,
  kSbKey1 = 0x31,
  kSbKey2 = 0x32,
  kSbKey3 = 0x33,
  kSbKey4 = 0x34,
  kSbKey5 = 0x35,
  kSbKey6 = 0x36,
  kSbKey7 = 0x37,
  kSbKey8 = 0x38,
  kSbKey9 = 0x39,
  kSbKeyA = 0x41,
  kSbKeyB = 0x42,
  kSbKeyC = 0x43,
  kSbKeyD = 0x44,
  kSbKeyE = 0x45,
  kSbKeyF = 0x46,
  kSbKeyG = 0x47,
  kSbKeyH = 0x48,
  kSbKeyI = 0x49,
  kSbKeyJ = 0x4A,
  kSbKeyK = 0x4B,
  kSbKeyL = 0x4C,
  kSbKeyM = 0x4D,
  kSbKeyN = 0x4E,
  kSbKeyO = 0x4F,
  kSbKeyP = 0x50,
  kSbKeyQ = 0x51,
  kSbKeyR = 0x52,
  kSbKeyS = 0x53,
  kSbKeyT = 0x54,
  kSbKeyU = 0x55,
  kSbKeyV = 0x56,
  kSbKeyW = 0x57,
  kSbKeyX = 0x58,
  kSbKeyY = 0x59,
  kSbKeyZ = 0x5A,
  kSbKeyLwin = 0x5B,
  kSbKeyCommand = kSbKeyLwin,  // Provide the Mac name for convenience.
  kSbKeyRwin = 0x5C,
  kSbKeyApps = 0x5D,
  kSbKeySleep = 0x5F,
  kSbKeyNumpad0 = 0x60,
  kSbKeyNumpad1 = 0x61,
  kSbKeyNumpad2 = 0x62,
  kSbKeyNumpad3 = 0x63,
  kSbKeyNumpad4 = 0x64,
  kSbKeyNumpad5 = 0x65,
  kSbKeyNumpad6 = 0x66,
  kSbKeyNumpad7 = 0x67,
  kSbKeyNumpad8 = 0x68,
  kSbKeyNumpad9 = 0x69,
  kSbKeyMultiply = 0x6A,
  kSbKeyAdd = 0x6B,
  kSbKeySeparator = 0x6C,
  kSbKeySubtract = 0x6D,
  kSbKeyDecimal = 0x6E,
  kSbKeyDivide = 0x6F,
  kSbKeyF1 = 0x70,
  kSbKeyF2 = 0x71,
  kSbKeyF3 = 0x72,
  kSbKeyF4 = 0x73,
  kSbKeyF5 = 0x74,
  kSbKeyF6 = 0x75,
  kSbKeyF7 = 0x76,
  kSbKeyF8 = 0x77,
  kSbKeyF9 = 0x78,
  kSbKeyF10 = 0x79,
  kSbKeyF11 = 0x7A,
  kSbKeyF12 = 0x7B,
  kSbKeyF13 = 0x7C,
  kSbKeyF14 = 0x7D,
  kSbKeyF15 = 0x7E,
  kSbKeyF16 = 0x7F,
  kSbKeyF17 = 0x80,
  kSbKeyF18 = 0x81,
  kSbKeyF19 = 0x82,
  kSbKeyF20 = 0x83,
  kSbKeyF21 = 0x84,
  kSbKeyF22 = 0x85,
  kSbKeyF23 = 0x86,
  kSbKeyF24 = 0x87,
  kSbKeyNumlock = 0x90,
  kSbKeyScroll = 0x91,
  kSbKeyWlan = 0x97,
  kSbKeyPower = 0x98,
  kSbKeyLshift = 0xA0,
  kSbKeyRshift = 0xA1,
  kSbKeyLcontrol = 0xA2,
  kSbKeyRcontrol = 0xA3,
  kSbKeyLmenu = 0xA4,
  kSbKeyRmenu = 0xA5,
  kSbKeyBrowserBack = 0xA6,
  kSbKeyBrowserForward = 0xA7,
  kSbKeyBrowserRefresh = 0xA8,
  kSbKeyBrowserStop = 0xA9,
  kSbKeyBrowserSearch = 0xAA,
  kSbKeyBrowserFavorites = 0xAB,
  kSbKeyBrowserHome = 0xAC,
  kSbKeyVolumeMute = 0xAD,
  kSbKeyVolumeDown = 0xAE,
  kSbKeyVolumeUp = 0xAF,
  kSbKeyMediaNextTrack = 0xB0,
  kSbKeyMediaPrevTrack = 0xB1,
  kSbKeyMediaStop = 0xB2,
  kSbKeyMediaPlayPause = 0xB3,
  kSbKeyMediaLaunchMail = 0xB4,
  kSbKeyMediaLaunchMediaSelect = 0xB5,
  kSbKeyMediaLaunchApp1 = 0xB6,
  kSbKeyMediaLaunchApp2 = 0xB7,
  kSbKeyOem1 = 0xBA,
  kSbKeyOemPlus = 0xBB,
  kSbKeyOemComma = 0xBC,
  kSbKeyOemMinus = 0xBD,
  kSbKeyOemPeriod = 0xBE,
  kSbKeyOem2 = 0xBF,
  kSbKeyOem3 = 0xC0,
  kSbKeyBrightnessDown = 0xD8,
  kSbKeyBrightnessUp = 0xD9,
  kSbKeyKbdBrightnessDown = 0xDA,
  kSbKeyOem4 = 0xDB,
  kSbKeyOem5 = 0xDC,
  kSbKeyOem6 = 0xDD,
  kSbKeyOem7 = 0xDE,
  kSbKeyOem8 = 0xDF,
  kSbKeyOem102 = 0xE2,
  kSbKeyKbdBrightnessUp = 0xE8,
  kSbKeyDbeSbcschar = 0xF3,
  kSbKeyDbeDbcschar = 0xF4,
  kSbKeyPlay = 0xFA,

  // Beyond this point are non-Windows key codes that are provided as
  // extensions, as they otherwise have no analogs.

  // Other supported CEA 2014 keys.
  kSbKeyMediaRewind = 0xE3,
  kSbKeyMediaFastForward = 0xE4,

#if SB_API_VERSION >= 6
  // Key codes from the DTV Application Software Environment,
  //   http://www.atsc.org/wp-content/uploads/2015/03/a_100_4.pdf
  kSbKeyRed = 0x193,
  kSbKeyGreen = 0x194,
  kSbKeyYellow = 0x195,
  kSbKeyBlue = 0x196,

  kSbKeyChannelUp = 0x1AB,
  kSbKeyChannelDown = 0x1AC,
  kSbKeySubtitle = 0x1CC,
  kSbKeyClosedCaption = kSbKeySubtitle,

  kSbKeyInfo = 0x1C9,
  kSbKeyGuide = 0x1CA,

  // Key codes from OCAP,
  //   https://apps.cablelabs.com/specification/opencable-application-platform-ocap/
  kSbKeyLast = 0x25f,
  kSbKeyPreviousChannel = kSbKeyLast,
#endif  // SB_API_VERSION >= 6

#if SB_API_VERSION >= 9
  // Also from OCAP
  kSbKeyInstantReplay = 0x273,
#endif  // SB_API_VERSION >= 9

#if SB_API_VERSION >= 6
  // Custom Starboard-defined keycodes.

  // A button that will directly launch the current application.
  kSbKeyLaunchThisApplication = 0x3000,

  // A button that will switch between different available audio tracks.
  kSbKeyMediaAudioTrack = 0x3001,
#endif  // SB_API_VERSION >= 6

#if SB_API_VERSION >= SB_MICROPHONE_KEY_CODE_API_VERSION
  // A button that will trigger voice input.
  kSbKeyMicrophone = 0x3002,
#endif  // SB_API_VERSION >= SB_MICROPHONE_KEY_CODE_API_VERSION

  // Mouse buttons, starting with the left mouse button.
  kSbKeyMouse1 = 0x7000,
  kSbKeyMouse2 = 0x7001,
  kSbKeyMouse3 = 0x7002,
  kSbKeyMouse4 = 0x7003,
  kSbKeyMouse5 = 0x7004,

  // Gamepad extensions to windows virtual key codes.
  // http://www.w3.org/TR/gamepad/

  // Xbox A, PS O or X (depending on region), WiiU B
  kSbKeyGamepad1 = 0x8000,

  // Xbox B, PS X or O (depending on region), WiiU A key
  kSbKeyGamepad2 = 0x8001,

  // Xbox X, PS square, WiiU Y
  kSbKeyGamepad3 = 0x8002,

  // Xbox Y, PS triangle, WiiU X
  kSbKeyGamepad4 = 0x8003,

  // Pretty much every gamepad has bumpers at the top front of the controller,
  // and triggers at the bottom front of the controller.
  kSbKeyGamepadLeftBumper = 0x8004,
  kSbKeyGamepadRightBumper = 0x8005,
  kSbKeyGamepadLeftTrigger = 0x8006,
  kSbKeyGamepadRightTrigger = 0x8007,

  // Xbox 360 Back, XB1 minimize, PS and WiiU Select
  kSbKeyGamepad5 = 0x8008,

  // Xbox 360 Play, XB1 Menu, PS and WiiU Start
  kSbKeyGamepad6 = 0x8009,

  // This refers to pressing the left stick like a button.
  kSbKeyGamepadLeftStick = 0x800A,

  // This refers to pressing the right stick like a button.
  kSbKeyGamepadRightStick = 0x800B,

  kSbKeyGamepadDPadUp = 0x800C,
  kSbKeyGamepadDPadDown = 0x800D,
  kSbKeyGamepadDPadLeft = 0x800E,
  kSbKeyGamepadDPadRight = 0x800F,

  // The system key in the middle of the gamepad, if it exists.
  kSbKeyGamepadSystem = 0x8010,

  // Codes for thumbstick to virtual dpad conversions.
  kSbKeyGamepadLeftStickUp = 0x8011,
  kSbKeyGamepadLeftStickDown = 0x8012,
  kSbKeyGamepadLeftStickLeft = 0x8013,
  kSbKeyGamepadLeftStickRight = 0x8014,
  kSbKeyGamepadRightStickUp = 0x8015,
  kSbKeyGamepadRightStickDown = 0x8016,
  kSbKeyGamepadRightStickLeft = 0x8017,
  kSbKeyGamepadRightStickRight = 0x8018,
} SbKey;

typedef enum SbKeyLocation {
  kSbKeyLocationUnspecified = 0,
  kSbKeyLocationLeft,
  kSbKeyLocationRight,
} SbKeyLocation;

// Bit-mask of key modifiers.
typedef enum SbKeyModifiers {
  kSbKeyModifiersNone = 0,
  kSbKeyModifiersAlt = 1 << 0,
  kSbKeyModifiersCtrl = 1 << 1,
  kSbKeyModifiersMeta = 1 << 2,
  kSbKeyModifiersShift = 1 << 3,
#if SB_API_VERSION >= 6
  kSbKeyModifiersPointerButtonLeft = 1 << 4,
  kSbKeyModifiersPointerButtonRight = 1 << 5,
  kSbKeyModifiersPointerButtonMiddle = 1 << 6,
  kSbKeyModifiersPointerButtonBack = 1 << 7,
  kSbKeyModifiersPointerButtonForward = 1 << 8,
#endif
} SbKeyModifiers;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_KEY_H_
