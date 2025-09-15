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

namespace ui {

DomCode SbKeyToDomCode(SbKey sb_key) {
  switch (sb_key) {
    case kSbKeyUnknown:
      return DomCode::NONE;
    case kSbKeyCancel:
      return DomCode::ABORT;
    case kSbKeyBackspace:
      return DomCode::BACKSPACE;
    case kSbKeyTab:
      return DomCode::TAB;
    case kSbKeyBacktab:
      return DomCode::TAB;
    case kSbKeyClear:
      return DomCode::NUMPAD_CLEAR;
    case kSbKeyReturn:
      return DomCode::ENTER;
    case kSbKeyShift:
      return DomCode::SHIFT_LEFT;
    case kSbKeyControl:
      return DomCode::CONTROL_LEFT;
    case kSbKeyPause:
      return DomCode::PAUSE;
    case kSbKeyCapital:
      return DomCode::CAPS_LOCK;
    case kSbKeyKana:
      return DomCode::KANA_MODE;
    case kSbKeyHanja:
      return DomCode::LANG2;
    case kSbKeyEscape:
      return DomCode::ESCAPE;
    case kSbKeyConvert:
      return DomCode::CONVERT;
    case kSbKeyNonconvert:
      return DomCode::NON_CONVERT;
    case kSbKeySpace:
      return DomCode::SPACE;
    case kSbKeyPrior:
      return DomCode::PAGE_UP;
    case kSbKeyNext:
      return DomCode::PAGE_DOWN;
    case kSbKeyEnd:
      return DomCode::END;
    case kSbKeyHome:
      return DomCode::HOME;
    case kSbKeyLeft:
      return DomCode::ARROW_LEFT;
    case kSbKeyUp:
      return DomCode::ARROW_UP;
    case kSbKeyRight:
      return DomCode::ARROW_RIGHT;
    case kSbKeyDown:
      return DomCode::ARROW_DOWN;
    case kSbKeySelect:
      return DomCode::SELECT;
    case kSbKeyPrint:
      return DomCode::PRINT;
    case kSbKeyExecute:
      return DomCode::OPEN;
    case kSbKeySnapshot:
      return DomCode::PRINT_SCREEN;
    case kSbKeyInsert:
      return DomCode::INSERT;
    case kSbKeyDelete:
      return DomCode::DEL;
    case kSbKeyHelp:
      return DomCode::HELP;
    case kSbKey0:
      return DomCode::DIGIT0;
    case kSbKey1:
      return DomCode::DIGIT1;
    case kSbKey2:
      return DomCode::DIGIT2;
    case kSbKey3:
      return DomCode::DIGIT3;
    case kSbKey4:
      return DomCode::DIGIT4;
    case kSbKey5:
      return DomCode::DIGIT5;
    case kSbKey6:
      return DomCode::DIGIT6;
    case kSbKey7:
      return DomCode::DIGIT7;
    case kSbKey8:
      return DomCode::DIGIT8;
    case kSbKey9:
      return DomCode::DIGIT9;
    case kSbKeyA:
      return DomCode::US_A;
    case kSbKeyB:
      return DomCode::US_B;
    case kSbKeyC:
      return DomCode::US_C;
    case kSbKeyD:
      return DomCode::US_D;
    case kSbKeyE:
      return DomCode::US_E;
    case kSbKeyF:
      return DomCode::US_F;
    case kSbKeyG:
      return DomCode::US_G;
    case kSbKeyH:
      return DomCode::US_H;
    case kSbKeyI:
      return DomCode::US_I;
    case kSbKeyJ:
      return DomCode::US_J;
    case kSbKeyK:
      return DomCode::US_K;
    case kSbKeyL:
      return DomCode::US_L;
    case kSbKeyM:
      return DomCode::US_M;
    case kSbKeyN:
      return DomCode::US_N;
    case kSbKeyO:
      return DomCode::US_O;
    case kSbKeyP:
      return DomCode::US_P;
    case kSbKeyQ:
      return DomCode::US_Q;
    case kSbKeyR:
      return DomCode::US_R;
    case kSbKeyS:
      return DomCode::US_S;
    case kSbKeyT:
      return DomCode::US_T;
    case kSbKeyU:
      return DomCode::US_U;
    case kSbKeyV:
      return DomCode::US_V;
    case kSbKeyW:
      return DomCode::US_W;
    case kSbKeyX:
      return DomCode::US_X;
    case kSbKeyY:
      return DomCode::US_Y;
    case kSbKeyZ:
      return DomCode::US_Z;
    case kSbKeyLwin:
      return DomCode::META_LEFT;
    case kSbKeyRwin:
      return DomCode::META_RIGHT;
    case kSbKeyApps:
      return DomCode::CONTEXT_MENU;
    case kSbKeySleep:
      return DomCode::SLEEP;
    case kSbKeyNumpad0:
      return DomCode::NUMPAD0;
    case kSbKeyNumpad1:
      return DomCode::NUMPAD1;
    case kSbKeyNumpad2:
      return DomCode::NUMPAD2;
    case kSbKeyNumpad3:
      return DomCode::NUMPAD3;
    case kSbKeyNumpad4:
      return DomCode::NUMPAD4;
    case kSbKeyNumpad5:
      return DomCode::NUMPAD5;
    case kSbKeyNumpad6:
      return DomCode::NUMPAD6;
    case kSbKeyNumpad7:
      return DomCode::NUMPAD7;
    case kSbKeyNumpad8:
      return DomCode::NUMPAD8;
    case kSbKeyNumpad9:
      return DomCode::NUMPAD9;
    case kSbKeyMultiply:
      return DomCode::NUMPAD_MULTIPLY;
    case kSbKeyAdd:
      return DomCode::NUMPAD_ADD;
    case kSbKeySeparator:
      return DomCode::NUMPAD_COMMA;
    case kSbKeySubtract:
      return DomCode::NUMPAD_SUBTRACT;
    case kSbKeyDecimal:
      return DomCode::NUMPAD_DECIMAL;
    case kSbKeyDivide:
      return DomCode::NUMPAD_DIVIDE;
    case kSbKeyF1:
      return DomCode::F1;
    case kSbKeyF2:
      return DomCode::F2;
    case kSbKeyF3:
      return DomCode::F3;
    case kSbKeyF4:
      return DomCode::F4;
    case kSbKeyF5:
      return DomCode::F5;
    case kSbKeyF6:
      return DomCode::F6;
    case kSbKeyF7:
      return DomCode::F7;
    case kSbKeyF8:
      return DomCode::F8;
    case kSbKeyF9:
      return DomCode::F9;
    case kSbKeyF10:
      return DomCode::F10;
    case kSbKeyF11:
      return DomCode::F11;
    case kSbKeyF12:
      return DomCode::F12;
    case kSbKeyF13:
      return DomCode::F13;
    case kSbKeyF14:
      return DomCode::F14;
    case kSbKeyF15:
      return DomCode::F15;
    case kSbKeyF16:
      return DomCode::F16;
    case kSbKeyF17:
      return DomCode::F17;
    case kSbKeyF18:
      return DomCode::F18;
    case kSbKeyF19:
      return DomCode::F19;
    case kSbKeyF20:
      return DomCode::F20;
    case kSbKeyF21:
      return DomCode::F21;
    case kSbKeyF22:
      return DomCode::F22;
    case kSbKeyF23:
      return DomCode::F23;
    case kSbKeyF24:
      return DomCode::F24;
    case kSbKeyNumlock:
      return DomCode::NUM_LOCK;
    case kSbKeyScroll:
      return DomCode::SCROLL_LOCK;
    case kSbKeyPower:
      return DomCode::POWER;
    case kSbKeyLshift:
      return DomCode::SHIFT_LEFT;
    case kSbKeyRshift:
      return DomCode::SHIFT_RIGHT;
    case kSbKeyLcontrol:
      return DomCode::CONTROL_LEFT;
    case kSbKeyRcontrol:
      return DomCode::CONTROL_RIGHT;
    case kSbKeyLmenu:
      return DomCode::ALT_LEFT;
    case kSbKeyRmenu:
      return DomCode::ALT_RIGHT;
    case kSbKeyBrowserBack:
      return DomCode::BROWSER_BACK;
    case kSbKeyBrowserForward:
      return DomCode::BROWSER_FORWARD;
    case kSbKeyBrowserRefresh:
      return DomCode::BROWSER_REFRESH;
    case kSbKeyBrowserStop:
      return DomCode::BROWSER_STOP;
    case kSbKeyBrowserSearch:
      return DomCode::BROWSER_SEARCH;
    case kSbKeyBrowserFavorites:
      return DomCode::BROWSER_FAVORITES;
    case kSbKeyBrowserHome:
      return DomCode::BROWSER_HOME;
    case kSbKeyVolumeMute:
      return DomCode::VOLUME_MUTE;
    case kSbKeyVolumeDown:
      return DomCode::VOLUME_DOWN;
    case kSbKeyVolumeUp:
      return DomCode::VOLUME_UP;
    case kSbKeyMediaNextTrack:
      return DomCode::MEDIA_TRACK_NEXT;
    case kSbKeyMediaPrevTrack:
      return DomCode::MEDIA_TRACK_PREVIOUS;
    case kSbKeyMediaStop:
      return DomCode::MEDIA_STOP;
    case kSbKeyMediaPlayPause:
      return DomCode::MEDIA_PLAY_PAUSE;
    case kSbKeyMediaLaunchMail:
      return DomCode::LAUNCH_MAIL;
    case kSbKeyMediaLaunchMediaSelect:
      return DomCode::MEDIA_SELECT;
    case kSbKeyMediaLaunchApp1:
      return DomCode::LAUNCH_APP1;
    case kSbKeyMediaLaunchApp2:
      return DomCode::LAUNCH_APP2;
    case kSbKeyOem1:
      return DomCode::SEMICOLON;
    case kSbKeyOemPlus:
      return DomCode::EQUAL;
    case kSbKeyOemComma:
      return DomCode::COMMA;
    case kSbKeyOemMinus:
      return DomCode::MINUS;
    case kSbKeyOemPeriod:
      return DomCode::PERIOD;
    case kSbKeyOem2:
      return DomCode::SLASH;
    case kSbKeyOem3:
      return DomCode::BACKQUOTE;
    case kSbKeyBrightnessDown:
      return DomCode::BRIGHTNESS_DOWN;
    case kSbKeyBrightnessUp:
      return DomCode::BRIGHTNESS_UP;
    case kSbKeyKbdBrightnessDown:
      return DomCode::KBD_ILLUM_DOWN;
    case kSbKeyOem4:
      return DomCode::BRACKET_LEFT;
    case kSbKeyOem5:
      return DomCode::BACKSLASH;
    case kSbKeyOem6:
      return DomCode::BRACKET_RIGHT;
    case kSbKeyOem7:
      return DomCode::QUOTE;
    case kSbKeyOem102:
      return DomCode::INTL_BACKSLASH;
    case kSbKeyKbdBrightnessUp:
      return DomCode::KBD_ILLUM_UP;
    case kSbKeyPlay:
      return DomCode::MEDIA_PLAY;
    case kSbKeyMediaRewind:
      return DomCode::MEDIA_REWIND;
    case kSbKeyMediaFastForward:
      return DomCode::MEDIA_FAST_FORWARD;
    case kSbKeyRed:
      return DomCode::F13;
    case kSbKeyGreen:
      return DomCode::F14;
    case kSbKeyYellow:
      return DomCode::F15;
    case kSbKeyBlue:
      return DomCode::F16;
    case kSbKeyRecord:
      return DomCode::MEDIA_RECORD;
    case kSbKeyChannelUp:
      return DomCode::CHANNEL_UP;
    case kSbKeyChannelDown:
      return DomCode::CHANNEL_DOWN;
    case kSbKeyInfo:
      return DomCode::INFO;
    case kSbKeyGuide:
      return DomCode::PROGRAM_GUIDE;
    case kSbKeyLast:
      return DomCode::MEDIA_LAST;
    case kSbKeyMicrophone:
      return DomCode::MICROPHONE_MUTE_TOGGLE;

    // The following keys don't have standard DomCode values.
    case kSbKeyMenu:
    case kSbKeyJunja:
    case kSbKeyFinal:
    case kSbKeyAccept:
    case kSbKeyModechange:
    case kSbKeyWlan:
    case kSbKeyOem8:
    case kSbKeyDbeSbcschar:
    case kSbKeyDbeDbcschar:
    case kSbKeyLaunchThisApplication:
    case kSbKeySubtitle:
    case kSbKeyInstantReplay:
    case kSbKeyMediaAudioTrack:
    case kSbKeyMouse1:
    case kSbKeyMouse2:
    case kSbKeyMouse3:
    case kSbKeyMouse4:
    case kSbKeyMouse5:
    case kSbKeyGamepad1:
    case kSbKeyGamepad2:
    case kSbKeyGamepad3:
    case kSbKeyGamepad4:
    case kSbKeyGamepadLeftBumper:
    case kSbKeyGamepadRightBumper:
    case kSbKeyGamepadLeftTrigger:
    case kSbKeyGamepadRightTrigger:
    case kSbKeyGamepad5:
    case kSbKeyGamepad6:
    case kSbKeyGamepadLeftStick:
    case kSbKeyGamepadRightStick:
    case kSbKeyGamepadDPadUp:
    case kSbKeyGamepadDPadDown:
    case kSbKeyGamepadDPadLeft:
    case kSbKeyGamepadDPadRight:
    case kSbKeyGamepadSystem:
    case kSbKeyGamepadLeftStickUp:
    case kSbKeyGamepadLeftStickDown:
    case kSbKeyGamepadLeftStickLeft:
    case kSbKeyGamepadLeftStickRight:
    case kSbKeyGamepadRightStickUp:
    case kSbKeyGamepadRightStickDown:
    case kSbKeyGamepadRightStickLeft:
    case kSbKeyGamepadRightStickRight:
      return DomCode::NONE;
  }
}

DomKey SbKeyToDomKey(SbKey sb_key, bool shift) {
  switch (sb_key) {
    case kSbKeyUnknown:
      return DomKey::UNIDENTIFIED;
    case kSbKeyCancel:
      return DomKey::CANCEL;
    case kSbKeyBackspace:
      return DomKey::BACKSPACE;
    case kSbKeyTab:
      return DomKey::TAB;
    case kSbKeyBacktab:
      return DomKey::TAB;
    case kSbKeyClear:
      return DomKey::CLEAR;
    case kSbKeyReturn:
      return DomKey::ENTER;
    case kSbKeyShift:
      return DomKey::SHIFT;
    case kSbKeyControl:
      return DomKey::CONTROL;
    case kSbKeyMenu:
      return DomKey::ALT;
    case kSbKeyPause:
      return DomKey::PAUSE;
    case kSbKeyCapital:
      return DomKey::CAPS_LOCK;
    case kSbKeyKana:
      return DomKey::KANA_MODE;
    case kSbKeyJunja:
      return DomKey::JUNJA_MODE;
    case kSbKeyFinal:
      return DomKey::FINAL_MODE;
    case kSbKeyHanja:
      return DomKey::HANJA_MODE;
    case kSbKeyEscape:
      return DomKey::ESCAPE;
    case kSbKeyConvert:
      return DomKey::CONVERT;
    case kSbKeyNonconvert:
      return DomKey::NON_CONVERT;
    case kSbKeyAccept:
      return DomKey::ACCEPT;
    case kSbKeyModechange:
      return DomKey::MODE_CHANGE;
    case kSbKeySpace:
      return DomKey::FromCharacter(' ');
    case kSbKeyPrior:
      return DomKey::PAGE_UP;
    case kSbKeyNext:
      return DomKey::PAGE_DOWN;
    case kSbKeyEnd:
      return DomKey::END;
    case kSbKeyHome:
      return DomKey::HOME;
    case kSbKeyLeft:
      return DomKey::ARROW_LEFT;
    case kSbKeyUp:
      return DomKey::ARROW_UP;
    case kSbKeyRight:
      return DomKey::ARROW_RIGHT;
    case kSbKeyDown:
      return DomKey::ARROW_DOWN;
    case kSbKeySelect:
      return DomKey::SELECT;
    case kSbKeyPrint:
      return DomKey::PRINT;
    case kSbKeyExecute:
      return DomKey::EXECUTE;
    case kSbKeySnapshot:
      return DomKey::PRINT_SCREEN;
    case kSbKeyInsert:
      return DomKey::INSERT;
    case kSbKeyDelete:
      return DomKey::DEL;
    case kSbKeyHelp:
      return DomKey::HELP;
    case kSbKey0:
      if (shift) {
        return DomKey::FromCharacter(')');
      } else {
        return DomKey::FromCharacter('0');
      }
    case kSbKey1:
      if (shift) {
        return DomKey::FromCharacter('!');
      } else {
        return DomKey::FromCharacter('1');
      }
    case kSbKey2:
      if (shift) {
        return DomKey::FromCharacter('@');
      } else {
        return DomKey::FromCharacter('2');
      }
    case kSbKey3:
      if (shift) {
        return DomKey::FromCharacter('#');
      } else {
        return DomKey::FromCharacter('3');
      }
    case kSbKey4:
      if (shift) {
        return DomKey::FromCharacter('$');
      } else {
        return DomKey::FromCharacter('4');
      }
    case kSbKey5:
      if (shift) {
        return DomKey::FromCharacter('%');
      } else {
        return DomKey::FromCharacter('5');
      }
    case kSbKey6:
      if (shift) {
        return DomKey::FromCharacter('^');
      } else {
        return DomKey::FromCharacter('6');
      }
    case kSbKey7:
      if (shift) {
        return DomKey::FromCharacter('&');
      } else {
        return DomKey::FromCharacter('7');
      }
    case kSbKey8:
      if (shift) {
        return DomKey::FromCharacter('*');
      } else {
        return DomKey::FromCharacter('8');
      }
    case kSbKey9:
      if (shift) {
        return DomKey::FromCharacter('(');
      } else {
        return DomKey::FromCharacter('9');
      }
    case kSbKeyA:
      if (shift) {
        return DomKey::FromCharacter('A');
      } else {
        return DomKey::FromCharacter('a');
      }
    case kSbKeyB:
      if (shift) {
        return DomKey::FromCharacter('B');
      } else {
        return DomKey::FromCharacter('b');
      }
    case kSbKeyC:
      if (shift) {
        return DomKey::FromCharacter('C');
      } else {
        return DomKey::FromCharacter('c');
      }
    case kSbKeyD:
      if (shift) {
        return DomKey::FromCharacter('D');
      } else {
        return DomKey::FromCharacter('d');
      }
    case kSbKeyE:
      if (shift) {
        return DomKey::FromCharacter('E');
      } else {
        return DomKey::FromCharacter('e');
      }
    case kSbKeyF:
      if (shift) {
        return DomKey::FromCharacter('F');
      } else {
        return DomKey::FromCharacter('f');
      }
    case kSbKeyG:
      if (shift) {
        return DomKey::FromCharacter('G');
      } else {
        return DomKey::FromCharacter('g');
      }
    case kSbKeyH:
      if (shift) {
        return DomKey::FromCharacter('H');
      } else {
        return DomKey::FromCharacter('h');
      }
    case kSbKeyI:
      if (shift) {
        return DomKey::FromCharacter('I');
      } else {
        return DomKey::FromCharacter('i');
      }
    case kSbKeyJ:
      if (shift) {
        return DomKey::FromCharacter('J');
      } else {
        return DomKey::FromCharacter('j');
      }
    case kSbKeyK:
      if (shift) {
        return DomKey::FromCharacter('K');
      } else {
        return DomKey::FromCharacter('k');
      }
    case kSbKeyL:
      if (shift) {
        return DomKey::FromCharacter('L');
      } else {
        return DomKey::FromCharacter('l');
      }
    case kSbKeyM:
      if (shift) {
        return DomKey::FromCharacter('M');
      } else {
        return DomKey::FromCharacter('m');
      }
    case kSbKeyN:
      if (shift) {
        return DomKey::FromCharacter('N');
      } else {
        return DomKey::FromCharacter('n');
      }
    case kSbKeyO:
      if (shift) {
        return DomKey::FromCharacter('O');
      } else {
        return DomKey::FromCharacter('o');
      }
    case kSbKeyP:
      if (shift) {
        return DomKey::FromCharacter('P');
      } else {
        return DomKey::FromCharacter('p');
      }
    case kSbKeyQ:
      if (shift) {
        return DomKey::FromCharacter('Q');
      } else {
        return DomKey::FromCharacter('q');
      }
    case kSbKeyR:
      if (shift) {
        return DomKey::FromCharacter('R');
      } else {
        return DomKey::FromCharacter('r');
      }
    case kSbKeyS:
      if (shift) {
        return DomKey::FromCharacter('S');
      } else {
        return DomKey::FromCharacter('s');
      }
    case kSbKeyT:
      if (shift) {
        return DomKey::FromCharacter('T');
      } else {
        return DomKey::FromCharacter('t');
      }
    case kSbKeyU:
      if (shift) {
        return DomKey::FromCharacter('U');
      } else {
        return DomKey::FromCharacter('u');
      }
    case kSbKeyV:
      if (shift) {
        return DomKey::FromCharacter('V');
      } else {
        return DomKey::FromCharacter('v');
      }
    case kSbKeyW:
      if (shift) {
        return DomKey::FromCharacter('W');
      } else {
        return DomKey::FromCharacter('w');
      }
    case kSbKeyX:
      if (shift) {
        return DomKey::FromCharacter('X');
      } else {
        return DomKey::FromCharacter('x');
      }
    case kSbKeyY:
      if (shift) {
        return DomKey::FromCharacter('Y');
      } else {
        return DomKey::FromCharacter('y');
      }
    case kSbKeyZ:
      if (shift) {
        return DomKey::FromCharacter('Z');
      } else {
        return DomKey::FromCharacter('z');
      }
    case kSbKeyLwin:
      return DomKey::META;
    case kSbKeyRwin:
      return DomKey::META;
    case kSbKeyApps:
      return DomKey::CONTEXT_MENU;
    case kSbKeySleep:
      return DomKey::STANDBY;
    case kSbKeyNumpad0:
      return DomKey::FromCharacter('0');
    case kSbKeyNumpad1:
      return DomKey::FromCharacter('1');
    case kSbKeyNumpad2:
      return DomKey::FromCharacter('2');
    case kSbKeyNumpad3:
      return DomKey::FromCharacter('3');
    case kSbKeyNumpad4:
      return DomKey::FromCharacter('4');
    case kSbKeyNumpad5:
      return DomKey::FromCharacter('5');
    case kSbKeyNumpad6:
      return DomKey::FromCharacter('6');
    case kSbKeyNumpad7:
      return DomKey::FromCharacter('7');
    case kSbKeyNumpad8:
      return DomKey::FromCharacter('8');
    case kSbKeyNumpad9:
      return DomKey::FromCharacter('9');
    case kSbKeyMultiply:
      return DomKey::FromCharacter('*');
    case kSbKeyAdd:
      return DomKey::FromCharacter('+');
    case kSbKeySubtract:
      return DomKey::FromCharacter('-');
    case kSbKeyDecimal:
      return DomKey::FromCharacter('.');
    case kSbKeyDivide:
      return DomKey::FromCharacter('/');
    case kSbKeyF1:
      return DomKey::F1;
    case kSbKeyF2:
      return DomKey::F2;
    case kSbKeyF3:
      return DomKey::F3;
    case kSbKeyF4:
      return DomKey::F4;
    case kSbKeyF5:
      return DomKey::F5;
    case kSbKeyF6:
      return DomKey::F6;
    case kSbKeyF7:
      return DomKey::F7;
    case kSbKeyF8:
      return DomKey::F8;
    case kSbKeyF9:
      return DomKey::F9;
    case kSbKeyF10:
      return DomKey::F10;
    case kSbKeyF11:
      return DomKey::F11;
    case kSbKeyF12:
      return DomKey::F12;
    case kSbKeyF13:
      return DomKey::F13;
    case kSbKeyF14:
      return DomKey::F14;
    case kSbKeyF15:
      return DomKey::F15;
    case kSbKeyF16:
      return DomKey::F16;
    case kSbKeyF17:
      return DomKey::F17;
    case kSbKeyF18:
      return DomKey::F18;
    case kSbKeyF19:
      return DomKey::F19;
    case kSbKeyF20:
      return DomKey::F20;
    case kSbKeyF21:
      return DomKey::F21;
    case kSbKeyF22:
      return DomKey::F22;
    case kSbKeyF23:
      return DomKey::F23;
    case kSbKeyF24:
      return DomKey::F24;
    case kSbKeyNumlock:
      return DomKey::NUM_LOCK;
    case kSbKeyScroll:
      return DomKey::SCROLL_LOCK;
    case kSbKeyPower:
      return DomKey::POWER;
    case kSbKeyLshift:
      return DomKey::SHIFT;
    case kSbKeyRshift:
      return DomKey::SHIFT;
    case kSbKeyLcontrol:
      return DomKey::CONTROL;
    case kSbKeyRcontrol:
      return DomKey::CONTROL;
    case kSbKeyLmenu:
      return DomKey::ALT;
    case kSbKeyRmenu:
      return DomKey::ALT;
    case kSbKeyBrowserBack:
      return DomKey::BROWSER_BACK;
    case kSbKeyBrowserForward:
      return DomKey::BROWSER_FORWARD;
    case kSbKeyBrowserRefresh:
      return DomKey::BROWSER_REFRESH;
    case kSbKeyBrowserStop:
      return DomKey::BROWSER_STOP;
    case kSbKeyBrowserSearch:
      return DomKey::BROWSER_SEARCH;
    case kSbKeyBrowserFavorites:
      return DomKey::BROWSER_FAVORITES;
    case kSbKeyBrowserHome:
      return DomKey::BROWSER_HOME;
    case kSbKeyVolumeMute:
      return DomKey::AUDIO_VOLUME_MUTE;
    case kSbKeyVolumeDown:
      return DomKey::AUDIO_VOLUME_DOWN;
    case kSbKeyVolumeUp:
      return DomKey::AUDIO_VOLUME_UP;
    case kSbKeyMediaNextTrack:
      return DomKey::MEDIA_TRACK_NEXT;
    case kSbKeyMediaPrevTrack:
      return DomKey::MEDIA_TRACK_PREVIOUS;
    case kSbKeyMediaStop:
      return DomKey::MEDIA_STOP;
    case kSbKeyMediaPlayPause:
      return DomKey::MEDIA_PLAY_PAUSE;
    case kSbKeyMediaLaunchMail:
      return DomKey::LAUNCH_MAIL;
    case kSbKeyMediaLaunchApp1:
      return DomKey::LAUNCH_MY_COMPUTER;
    case kSbKeyMediaLaunchApp2:
      return DomKey::LAUNCH_CALCULATOR;
    case kSbKeyOem1:
      if (shift) {
        return DomKey::FromCharacter(':');
      } else {
        return DomKey::FromCharacter(';');
      }
    case kSbKeyOemPlus:
      if (shift) {
        return DomKey::FromCharacter('+');
      } else {
        return DomKey::FromCharacter('=');
      }
    case kSbKeyOemComma:
      if (shift) {
        return DomKey::FromCharacter('<');
      } else {
        return DomKey::FromCharacter(',');
      }
    case kSbKeyOemMinus:
      if (shift) {
        return DomKey::FromCharacter('_');
      } else {
        return DomKey::FromCharacter('-');
      }
    case kSbKeyOemPeriod:
      if (shift) {
        return DomKey::FromCharacter('>');
      } else {
        return DomKey::FromCharacter('.');
      }
    case kSbKeyOem2:
      if (shift) {
        return DomKey::FromCharacter('?');
      } else {
        return DomKey::FromCharacter('/');
      }
    case kSbKeyOem3:
      if (shift) {
        return DomKey::FromCharacter('~');
      } else {
        return DomKey::FromCharacter('`');
      }
    case kSbKeyBrightnessDown:
      return DomKey::BRIGHTNESS_DOWN;
    case kSbKeyBrightnessUp:
      return DomKey::BRIGHTNESS_UP;
    case kSbKeyOem4:
      if (shift) {
        return DomKey::FromCharacter('{');
      } else {
        return DomKey::FromCharacter('[');
      }
    case kSbKeyOem5:
      if (shift) {
        return DomKey::FromCharacter('|');
      } else {
        return DomKey::FromCharacter('\\');
      }
    case kSbKeyOem6:
      if (shift) {
        return DomKey::FromCharacter('}');
      } else {
        return DomKey::FromCharacter(']');
      }
    case kSbKeyOem7:
      if (shift) {
        return DomKey::FromCharacter('"');
      } else {
        return DomKey::FromCharacter('\'');
      }
    case kSbKeyOem102:
      if (shift) {
        return DomKey::FromCharacter('|');
      } else {
        return DomKey::FromCharacter('\\');
      }
    case kSbKeyPlay:
      return DomKey::PLAY;
    case kSbKeyMediaRewind:
      return DomKey::MEDIA_REWIND;
    case kSbKeyMediaFastForward:
      return DomKey::MEDIA_FAST_FORWARD;
    case kSbKeyRed:
      return DomKey::COLOR_F0_RED;
    case kSbKeyGreen:
      return DomKey::COLOR_F1_GREEN;
    case kSbKeyYellow:
      return DomKey::COLOR_F2_YELLOW;
    case kSbKeyBlue:
      return DomKey::COLOR_F3_BLUE;
    case kSbKeyRecord:
      return DomKey::MEDIA_RECORD;
    case kSbKeyChannelUp:
      return DomKey::CHANNEL_UP;
    case kSbKeyChannelDown:
      return DomKey::CHANNEL_DOWN;
    case kSbKeySubtitle:
      return DomKey::SUBTITLE;
    case kSbKeyInfo:
      return DomKey::INFO;
    case kSbKeyGuide:
      return DomKey::GUIDE;
    case kSbKeyInstantReplay:
      return DomKey::INSTANT_REPLAY;
    case kSbKeyMediaAudioTrack:
      return DomKey::MEDIA_AUDIO_TRACK;
    case kSbKeyMicrophone:
      return DomKey::MICROPHONE_TOGGLE;
    case kSbKeyGamepadDPadUp:
      return DomKey::ARROW_UP;
    case kSbKeyGamepadDPadDown:
      return DomKey::ARROW_DOWN;
    case kSbKeyGamepadDPadLeft:
      return DomKey::ARROW_LEFT;
    case kSbKeyGamepadDPadRight:
      return DomKey::ARROW_RIGHT;

    // The foolwing keys don't have  standard DomKey values.
    case kSbKeySeparator:
    case kSbKeyWlan:
    case kSbKeyMediaLaunchMediaSelect:
    case kSbKeyKbdBrightnessDown:
    case kSbKeyOem8:
    case kSbKeyKbdBrightnessUp:
    case kSbKeyDbeSbcschar:
    case kSbKeyDbeDbcschar:
    case kSbKeyLast:
    case kSbKeyLaunchThisApplication:
    case kSbKeyMouse1:
    case kSbKeyMouse2:
    case kSbKeyMouse3:
    case kSbKeyMouse4:
    case kSbKeyMouse5:
    case kSbKeyGamepad1:
    case kSbKeyGamepad2:
    case kSbKeyGamepad3:
    case kSbKeyGamepad4:
    case kSbKeyGamepadLeftBumper:
    case kSbKeyGamepadRightBumper:
    case kSbKeyGamepadLeftTrigger:
    case kSbKeyGamepadRightTrigger:
    case kSbKeyGamepad5:
    case kSbKeyGamepad6:
    case kSbKeyGamepadLeftStick:
    case kSbKeyGamepadRightStick:
    case kSbKeyGamepadSystem:
    case kSbKeyGamepadLeftStickUp:
    case kSbKeyGamepadLeftStickDown:
    case kSbKeyGamepadLeftStickLeft:
    case kSbKeyGamepadLeftStickRight:
    case kSbKeyGamepadRightStickUp:
    case kSbKeyGamepadRightStickDown:
    case kSbKeyGamepadRightStickLeft:
    case kSbKeyGamepadRightStickRight:
      return DomKey::UNIDENTIFIED;
  }
}

KeyboardCode SbKeyToKeyboardCode(SbKey sb_key) {
  switch (sb_key) {
    case kSbKeyUnknown:
      return VKEY_UNKNOWN;
    case kSbKeyCancel:
      return VKEY_CANCEL;
    case kSbKeyBackspace:
      return VKEY_BACK;
    case kSbKeyTab:
      return VKEY_TAB;
    case kSbKeyBacktab:
      return VKEY_BACKTAB;
    case kSbKeyClear:
      return VKEY_CLEAR;
    case kSbKeyReturn:
      return VKEY_RETURN;
    case kSbKeyShift:
      return VKEY_SHIFT;
    case kSbKeyControl:
      return VKEY_CONTROL;
    case kSbKeyMenu:
      return VKEY_MENU;
    case kSbKeyPause:
      return VKEY_PAUSE;
    case kSbKeyCapital:
      return VKEY_CAPITAL;
    case kSbKeyKana:
      return VKEY_KANA;
    case kSbKeyJunja:
      return VKEY_JUNJA;
    case kSbKeyFinal:
      return VKEY_FINAL;
    case kSbKeyHanja:
      return VKEY_HANJA;
    case kSbKeyEscape:
      return VKEY_ESCAPE;
    case kSbKeyConvert:
      return VKEY_CONVERT;
    case kSbKeyNonconvert:
      return VKEY_NONCONVERT;
    case kSbKeyAccept:
      return VKEY_ACCEPT;
    case kSbKeyModechange:
      return VKEY_MODECHANGE;
    case kSbKeySpace:
      return VKEY_SPACE;
    case kSbKeyPrior:
      return VKEY_PRIOR;
    case kSbKeyNext:
      return VKEY_NEXT;
    case kSbKeyEnd:
      return VKEY_END;
    case kSbKeyHome:
      return VKEY_HOME;
    case kSbKeyLeft:
      return VKEY_LEFT;
    case kSbKeyUp:
      return VKEY_UP;
    case kSbKeyRight:
      return VKEY_RIGHT;
    case kSbKeyDown:
      return VKEY_DOWN;
    case kSbKeySelect:
      return VKEY_SELECT;
    case kSbKeyPrint:
      return VKEY_PRINT;
    case kSbKeyExecute:
      return VKEY_EXECUTE;
    case kSbKeySnapshot:
      return VKEY_SNAPSHOT;
    case kSbKeyInsert:
      return VKEY_INSERT;
    case kSbKeyDelete:
      return VKEY_DELETE;
    case kSbKeyHelp:
      return VKEY_HELP;
    case kSbKey0:
      return VKEY_0;
    case kSbKey1:
      return VKEY_1;
    case kSbKey2:
      return VKEY_2;
    case kSbKey3:
      return VKEY_3;
    case kSbKey4:
      return VKEY_4;
    case kSbKey5:
      return VKEY_5;
    case kSbKey6:
      return VKEY_6;
    case kSbKey7:
      return VKEY_7;
    case kSbKey8:
      return VKEY_8;
    case kSbKey9:
      return VKEY_9;
    case kSbKeyA:
      return VKEY_A;
    case kSbKeyB:
      return VKEY_B;
    case kSbKeyC:
      return VKEY_C;
    case kSbKeyD:
      return VKEY_D;
    case kSbKeyE:
      return VKEY_E;
    case kSbKeyF:
      return VKEY_F;
    case kSbKeyG:
      return VKEY_G;
    case kSbKeyH:
      return VKEY_H;
    case kSbKeyI:
      return VKEY_I;
    case kSbKeyJ:
      return VKEY_J;
    case kSbKeyK:
      return VKEY_K;
    case kSbKeyL:
      return VKEY_L;
    case kSbKeyM:
      return VKEY_M;
    case kSbKeyN:
      return VKEY_N;
    case kSbKeyO:
      return VKEY_O;
    case kSbKeyP:
      return VKEY_P;
    case kSbKeyQ:
      return VKEY_Q;
    case kSbKeyR:
      return VKEY_R;
    case kSbKeyS:
      return VKEY_S;
    case kSbKeyT:
      return VKEY_T;
    case kSbKeyU:
      return VKEY_U;
    case kSbKeyV:
      return VKEY_V;
    case kSbKeyW:
      return VKEY_W;
    case kSbKeyX:
      return VKEY_X;
    case kSbKeyY:
      return VKEY_Y;
    case kSbKeyZ:
      return VKEY_Z;
    case kSbKeyLwin:
      return VKEY_LWIN;
    case kSbKeyRwin:
      return VKEY_RWIN;
    case kSbKeyApps:
      return VKEY_APPS;
    case kSbKeySleep:
      return VKEY_SLEEP;
    case kSbKeyNumpad0:
      return VKEY_NUMPAD0;
    case kSbKeyNumpad1:
      return VKEY_NUMPAD1;
    case kSbKeyNumpad2:
      return VKEY_NUMPAD2;
    case kSbKeyNumpad3:
      return VKEY_NUMPAD3;
    case kSbKeyNumpad4:
      return VKEY_NUMPAD4;
    case kSbKeyNumpad5:
      return VKEY_NUMPAD5;
    case kSbKeyNumpad6:
      return VKEY_NUMPAD6;
    case kSbKeyNumpad7:
      return VKEY_NUMPAD7;
    case kSbKeyNumpad8:
      return VKEY_NUMPAD8;
    case kSbKeyNumpad9:
      return VKEY_NUMPAD9;
    case kSbKeyMultiply:
      return VKEY_MULTIPLY;
    case kSbKeyAdd:
      return VKEY_ADD;
    case kSbKeySeparator:
      return VKEY_SEPARATOR;
    case kSbKeySubtract:
      return VKEY_SUBTRACT;
    case kSbKeyDecimal:
      return VKEY_DECIMAL;
    case kSbKeyDivide:
      return VKEY_DIVIDE;
    case kSbKeyF1:
      return VKEY_F1;
    case kSbKeyF2:
      return VKEY_F2;
    case kSbKeyF3:
      return VKEY_F3;
    case kSbKeyF4:
      return VKEY_F4;
    case kSbKeyF5:
      return VKEY_F5;
    case kSbKeyF6:
      return VKEY_F6;
    case kSbKeyF7:
      return VKEY_F7;
    case kSbKeyF8:
      return VKEY_F8;
    case kSbKeyF9:
      return VKEY_F9;
    case kSbKeyF10:
      return VKEY_F10;
    case kSbKeyF11:
      return VKEY_F11;
    case kSbKeyF12:
      return VKEY_F12;
    case kSbKeyF13:
      return VKEY_F13;
    case kSbKeyF14:
      return VKEY_F14;
    case kSbKeyF15:
      return VKEY_F15;
    case kSbKeyF16:
      return VKEY_F16;
    case kSbKeyF17:
      return VKEY_F17;
    case kSbKeyF18:
      return VKEY_F18;
    case kSbKeyF19:
      return VKEY_F19;
    case kSbKeyF20:
      return VKEY_F20;
    case kSbKeyF21:
      return VKEY_F21;
    case kSbKeyF22:
      return VKEY_F22;
    case kSbKeyF23:
      return VKEY_F23;
    case kSbKeyF24:
      return VKEY_F24;
    case kSbKeyNumlock:
      return VKEY_NUMLOCK;
    case kSbKeyScroll:
      return VKEY_SCROLL;
    case kSbKeyWlan:
      return VKEY_WLAN;
    case kSbKeyPower:
      return VKEY_POWER;
    case kSbKeyLshift:
      return VKEY_LSHIFT;
    case kSbKeyRshift:
      return VKEY_RSHIFT;
    case kSbKeyLcontrol:
      return VKEY_LCONTROL;
    case kSbKeyRcontrol:
      return VKEY_RCONTROL;
    case kSbKeyLmenu:
      return VKEY_LMENU;
    case kSbKeyRmenu:
      return VKEY_RMENU;
    case kSbKeyBrowserBack:
      return VKEY_BROWSER_BACK;
    case kSbKeyBrowserForward:
      return VKEY_BROWSER_FORWARD;
    case kSbKeyBrowserRefresh:
      return VKEY_BROWSER_REFRESH;
    case kSbKeyBrowserStop:
      return VKEY_BROWSER_STOP;
    case kSbKeyBrowserSearch:
      return VKEY_BROWSER_SEARCH;
    case kSbKeyBrowserFavorites:
      return VKEY_BROWSER_FAVORITES;
    case kSbKeyBrowserHome:
      return VKEY_BROWSER_HOME;
    case kSbKeyVolumeMute:
      return VKEY_VOLUME_MUTE;
    case kSbKeyVolumeDown:
      return VKEY_VOLUME_DOWN;
    case kSbKeyVolumeUp:
      return VKEY_VOLUME_UP;
    case kSbKeyMediaNextTrack:
      return VKEY_MEDIA_NEXT_TRACK;
    case kSbKeyMediaPrevTrack:
      return VKEY_MEDIA_PREV_TRACK;
    case kSbKeyMediaStop:
      return VKEY_MEDIA_STOP;
    case kSbKeyMediaPlayPause:
      return VKEY_MEDIA_PLAY_PAUSE;
    case kSbKeyMediaLaunchMail:
      return VKEY_MEDIA_LAUNCH_MAIL;
    case kSbKeyMediaLaunchMediaSelect:
      return VKEY_MEDIA_LAUNCH_MEDIA_SELECT;
    case kSbKeyMediaLaunchApp1:
      return VKEY_MEDIA_LAUNCH_APP1;
    case kSbKeyMediaLaunchApp2:
      return VKEY_MEDIA_LAUNCH_APP2;
    case kSbKeyOem1:
      return VKEY_OEM_1;
    case kSbKeyOemPlus:
      return VKEY_OEM_PLUS;
    case kSbKeyOemComma:
      return VKEY_OEM_COMMA;
    case kSbKeyOemMinus:
      return VKEY_OEM_MINUS;
    case kSbKeyOemPeriod:
      return VKEY_OEM_PERIOD;
    case kSbKeyOem2:
      return VKEY_OEM_2;
    case kSbKeyOem3:
      return VKEY_OEM_3;
    case kSbKeyBrightnessDown:
      return VKEY_BRIGHTNESS_DOWN;
    case kSbKeyBrightnessUp:
      return VKEY_BRIGHTNESS_UP;
    case kSbKeyKbdBrightnessDown:
      return VKEY_KBD_BRIGHTNESS_DOWN;
    case kSbKeyOem4:
      return VKEY_OEM_4;
    case kSbKeyOem5:
      return VKEY_OEM_5;
    case kSbKeyOem6:
      return VKEY_OEM_6;
    case kSbKeyOem7:
      return VKEY_OEM_7;
    case kSbKeyOem8:
      return VKEY_OEM_8;
    case kSbKeyOem102:
      return VKEY_OEM_102;
    case kSbKeyKbdBrightnessUp:
      return VKEY_KBD_BRIGHTNESS_UP;
    case kSbKeyDbeSbcschar:
      return VKEY_DBE_SBCSCHAR;
    case kSbKeyDbeDbcschar:
      return VKEY_DBE_DBCSCHAR;
    case kSbKeyPlay:
      return VKEY_PLAY;
    case kSbKeyMediaRewind:
      return VKEY_OEM_103;
    case kSbKeyMediaFastForward:
      return VKEY_OEM_104;
    case kSbKeyChannelUp:
      return VKEY_PRIOR;
    case kSbKeyChannelDown:
      return VKEY_NEXT;
    case kSbKeySubtitle:
      return KEY_SUBTITLES;
    case kSbKeyMicrophone:
      return VKEY_MICROPHONE_MUTE_TOGGLE;
    case kSbKeyGamepadDPadUp:
      return VKEY_UP;
    case kSbKeyGamepadDPadDown:
      return VKEY_DOWN;
    case kSbKeyGamepadDPadLeft:
      return VKEY_LEFT;
    case kSbKeyGamepadDPadRight:
      return VKEY_RIGHT;

    // The following keys don't have standard KeyboardCode values.
    case kSbKeyRed:
    case kSbKeyGreen:
    case kSbKeyYellow:
    case kSbKeyBlue:
    case kSbKeyRecord:
    case kSbKeyInfo:
    case kSbKeyGuide:
    case kSbKeyLast:
    case kSbKeyInstantReplay:
    case kSbKeyLaunchThisApplication:
    case kSbKeyMediaAudioTrack:
    case kSbKeyMouse1:
    case kSbKeyMouse2:
    case kSbKeyMouse3:
    case kSbKeyMouse4:
    case kSbKeyMouse5:
    case kSbKeyGamepad1:
    case kSbKeyGamepad2:
    case kSbKeyGamepad3:
    case kSbKeyGamepad4:
    case kSbKeyGamepadLeftBumper:
    case kSbKeyGamepadRightBumper:
    case kSbKeyGamepadLeftTrigger:
    case kSbKeyGamepadRightTrigger:
    case kSbKeyGamepad5:
    case kSbKeyGamepad6:
    case kSbKeyGamepadLeftStick:
    case kSbKeyGamepadRightStick:
    case kSbKeyGamepadSystem:
    case kSbKeyGamepadLeftStickUp:
    case kSbKeyGamepadLeftStickDown:
    case kSbKeyGamepadLeftStickLeft:
    case kSbKeyGamepadLeftStickRight:
    case kSbKeyGamepadRightStickUp:
    case kSbKeyGamepadRightStickDown:
    case kSbKeyGamepadRightStickLeft:
    case kSbKeyGamepadRightStickRight:
      return VKEY_UNKNOWN;
  }
}

}  // namespace ui
