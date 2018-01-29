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

#include "starboard/shared/x11/application_x11.h"

#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#define XK_3270  // for XK_3270_BackTab
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <iomanip>

#include "starboard/common/scoped_ptr.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/player.h"
#include "starboard/shared/posix/time_internal.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"
#include "starboard/shared/x11/window_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace x11 {

using ::starboard::shared::dev_input::DevInput;

namespace {

enum {
  kNoneDeviceId,
  kKeyboardDeviceId,
  kMouseDeviceId,
};

// Key translation taken from cobalt/system_window/linux/keycode_conversion.cc
// Eventually, that code should be removed in favor of this code.

uint32_t HardwareKeycodeToDefaultXKeysym(uint32_t hardware_code) {
  static const uint32_t kHardwareKeycodeMap[] = {
      0,                // 0x00:
      0,                // 0x01:
      0,                // 0x02:
      0,                // 0x03:
      0,                // 0x04:
      0,                // 0x05:
      0,                // 0x06:
      0,                // 0x07:
      0,                // 0x08:
      XK_Escape,        // 0x09: XK_Escape
      XK_1,             // 0x0A: XK_1
      XK_2,             // 0x0B: XK_2
      XK_3,             // 0x0C: XK_3
      XK_4,             // 0x0D: XK_4
      XK_5,             // 0x0E: XK_5
      XK_6,             // 0x0F: XK_6
      XK_7,             // 0x10: XK_7
      XK_8,             // 0x11: XK_8
      XK_9,             // 0x12: XK_9
      XK_0,             // 0x13: XK_0
      XK_minus,         // 0x14: XK_minus
      XK_equal,         // 0x15: XK_equal
      XK_BackSpace,     // 0x16: XK_BackSpace
      XK_Tab,           // 0x17: XK_Tab
      XK_q,             // 0x18: XK_q
      XK_w,             // 0x19: XK_w
      XK_e,             // 0x1A: XK_e
      XK_r,             // 0x1B: XK_r
      XK_t,             // 0x1C: XK_t
      XK_y,             // 0x1D: XK_y
      XK_u,             // 0x1E: XK_u
      XK_i,             // 0x1F: XK_i
      XK_o,             // 0x20: XK_o
      XK_p,             // 0x21: XK_p
      XK_bracketleft,   // 0x22: XK_bracketleft
      XK_bracketright,  // 0x23: XK_bracketright
      XK_Return,        // 0x24: XK_Return
      XK_Control_L,     // 0x25: XK_Control_L
      XK_a,             // 0x26: XK_a
      XK_s,             // 0x27: XK_s
      XK_d,             // 0x28: XK_d
      XK_f,             // 0x29: XK_f
      XK_g,             // 0x2A: XK_g
      XK_h,             // 0x2B: XK_h
      XK_j,             // 0x2C: XK_j
      XK_k,             // 0x2D: XK_k
      XK_l,             // 0x2E: XK_l
      XK_semicolon,     // 0x2F: XK_semicolon
      XK_apostrophe,    // 0x30: XK_apostrophe
      XK_grave,         // 0x31: XK_grave
      XK_Shift_L,       // 0x32: XK_Shift_L
      XK_backslash,     // 0x33: XK_backslash
      XK_z,             // 0x34: XK_z
      XK_x,             // 0x35: XK_x
      XK_c,             // 0x36: XK_c
      XK_v,             // 0x37: XK_v
      XK_b,             // 0x38: XK_b
      XK_n,             // 0x39: XK_n
      XK_m,             // 0x3A: XK_m
      XK_comma,         // 0x3B: XK_comma
      XK_period,        // 0x3C: XK_period
      XK_slash,         // 0x3D: XK_slash
      XK_Shift_R,       // 0x3E: XK_Shift_R
      0,                // 0x3F: XK_KP_Multiply
      XK_Alt_L,         // 0x40: XK_Alt_L
      XK_space,         // 0x41: XK_space
      XK_Caps_Lock,     // 0x42: XK_Caps_Lock
      XK_F1,            // 0x43: XK_F1
      XK_F2,            // 0x44: XK_F2
      XK_F3,            // 0x45: XK_F3
      XK_F4,            // 0x46: XK_F4
      XK_F5,            // 0x47: XK_F5
      XK_F6,            // 0x48: XK_F6
      XK_F7,            // 0x49: XK_F7
      XK_F8,            // 0x4A: XK_F8
      XK_F9,            // 0x4B: XK_F9
      XK_F10,           // 0x4C: XK_F10
      XK_Num_Lock,      // 0x4D: XK_Num_Lock
      XK_Scroll_Lock,   // 0x4E: XK_Scroll_Lock
  };

  return hardware_code < SB_ARRAY_SIZE(kHardwareKeycodeMap)
             ? kHardwareKeycodeMap[hardware_code]
             : 0;
}

SbKey KeysymToSbKey(KeySym keysym) {
  switch (keysym) {
    case XK_BackSpace:
      return kSbKeyBack;
    case XK_Delete:
    case XK_KP_Delete:
      return kSbKeyDelete;
    case XK_Tab:
    case XK_KP_Tab:
    case XK_ISO_Left_Tab:
    case XK_3270_BackTab:
      return kSbKeyTab;
    case XK_Linefeed:
    case XK_Return:
    case XK_KP_Enter:
    case XK_ISO_Enter:
      return kSbKeyReturn;
    case XK_Clear:
    case XK_KP_Begin:  // NumPad 5 without Num Lock, for crosbug.com/29169.
      return kSbKeyClear;
    case XK_KP_Space:
    case XK_space:
      return kSbKeySpace;
    case XK_Home:
    case XK_KP_Home:
      return kSbKeyHome;
    case XK_End:
    case XK_KP_End:
      return kSbKeyEnd;
    case XK_Page_Up:
    case XK_KP_Page_Up:  // aka XK_KP_Prior
      return kSbKeyPrior;
    case XK_Page_Down:
    case XK_KP_Page_Down:  // aka XK_KP_Next
      return kSbKeyNext;
    case XK_Left:
    case XK_KP_Left:
      return kSbKeyLeft;
    case XK_Right:
    case XK_KP_Right:
      return kSbKeyRight;
    case XK_Down:
    case XK_KP_Down:
      return kSbKeyDown;
    case XK_Up:
    case XK_KP_Up:
      return kSbKeyUp;
    case XK_Escape:
      return kSbKeyEscape;
    case XK_Kana_Lock:
    case XK_Kana_Shift:
      return kSbKeyKana;
    case XK_Hangul:
      return kSbKeyHangul;
    case XK_Hangul_Hanja:
      return kSbKeyHanja;
    case XK_Kanji:
      return kSbKeyKanji;
    case XK_Henkan:
      return kSbKeyConvert;
    case XK_Muhenkan:
      return kSbKeyNonconvert;
    case XK_Zenkaku_Hankaku:
      return kSbKeyDbeDbcschar;
    case XK_A:
    case XK_a:
      return kSbKeyA;
    case XK_B:
    case XK_b:
      return kSbKeyB;
    case XK_C:
    case XK_c:
      return kSbKeyC;
    case XK_D:
    case XK_d:
      return kSbKeyD;
    case XK_E:
    case XK_e:
      return kSbKeyE;
    case XK_F:
    case XK_f:
      return kSbKeyF;
    case XK_G:
    case XK_g:
      return kSbKeyG;
    case XK_H:
    case XK_h:
      return kSbKeyH;
    case XK_I:
    case XK_i:
      return kSbKeyI;
    case XK_J:
    case XK_j:
      return kSbKeyJ;
    case XK_K:
    case XK_k:
      return kSbKeyK;
    case XK_L:
    case XK_l:
      return kSbKeyL;
    case XK_M:
    case XK_m:
      return kSbKeyM;
    case XK_N:
    case XK_n:
      return kSbKeyN;
    case XK_O:
    case XK_o:
      return kSbKeyO;
    case XK_P:
    case XK_p:
      return kSbKeyP;
    case XK_Q:
    case XK_q:
      return kSbKeyQ;
    case XK_R:
    case XK_r:
      return kSbKeyR;
    case XK_S:
    case XK_s:
      return kSbKeyS;
    case XK_T:
    case XK_t:
      return kSbKeyT;
    case XK_U:
    case XK_u:
      return kSbKeyU;
    case XK_V:
    case XK_v:
      return kSbKeyV;
    case XK_W:
    case XK_w:
      return kSbKeyW;
    case XK_X:
    case XK_x:
      return kSbKeyX;
    case XK_Y:
    case XK_y:
      return kSbKeyY;
    case XK_Z:
    case XK_z:
      return kSbKeyZ;

    case XK_0:
    case XK_1:
    case XK_2:
    case XK_3:
    case XK_4:
    case XK_5:
    case XK_6:
    case XK_7:
    case XK_8:
    case XK_9:
      return static_cast<SbKey>(kSbKey0 + (keysym - XK_0));

    case XK_parenright:
      return kSbKey0;
    case XK_exclam:
      return kSbKey1;
    case XK_at:
      return kSbKey2;
    case XK_numbersign:
      return kSbKey3;
    case XK_dollar:
      return kSbKey4;
    case XK_percent:
      return kSbKey5;
    case XK_asciicircum:
      return kSbKey6;
    case XK_ampersand:
      return kSbKey7;
    case XK_asterisk:
      return kSbKey8;
    case XK_parenleft:
      return kSbKey9;

    case XK_KP_0:
    case XK_KP_1:
    case XK_KP_2:
    case XK_KP_3:
    case XK_KP_4:
    case XK_KP_5:
    case XK_KP_6:
    case XK_KP_7:
    case XK_KP_8:
    case XK_KP_9:
      return static_cast<SbKey>(kSbKeyNumpad0 + (keysym - XK_KP_0));

    case XK_multiply:
    case XK_KP_Multiply:
      return kSbKeyMultiply;
    case XK_KP_Add:
      return kSbKeyAdd;
    case XK_KP_Separator:
      return kSbKeySeparator;
    case XK_KP_Subtract:
      return kSbKeySubtract;
    case XK_KP_Decimal:
      return kSbKeyDecimal;
    case XK_KP_Divide:
      return kSbKeyDivide;
    case XK_KP_Equal:
    case XK_equal:
    case XK_plus:
      return kSbKeyOemPlus;
    case XK_comma:
    case XK_less:
      return kSbKeyOemComma;
    case XK_minus:
    case XK_underscore:
      return kSbKeyOemMinus;
    case XK_greater:
    case XK_period:
      return kSbKeyOemPeriod;
    case XK_colon:
    case XK_semicolon:
      return kSbKeyOem1;
    case XK_question:
    case XK_slash:
      return kSbKeyOem2;
    case XK_asciitilde:
    case XK_quoteleft:
      return kSbKeyOem3;
    case XK_bracketleft:
    case XK_braceleft:
      return kSbKeyOem4;
    case XK_backslash:
    case XK_bar:
      return kSbKeyOem5;
    case XK_bracketright:
    case XK_braceright:
      return kSbKeyOem6;
    case XK_quoteright:
    case XK_quotedbl:
      return kSbKeyOem7;
    case XK_Shift_L:
    case XK_Shift_R:
      return kSbKeyShift;
    case XK_Control_L:
    case XK_Control_R:
      return kSbKeyControl;
    case XK_Meta_L:
    case XK_Meta_R:
    case XK_Alt_L:
    case XK_Alt_R:
      return kSbKeyMenu;
    case XK_Pause:
      return kSbKeyPause;
    case XK_Caps_Lock:
      return kSbKeyCapital;
    case XK_Num_Lock:
      return kSbKeyNumlock;
    case XK_Scroll_Lock:
      return kSbKeyScroll;
    case XK_Select:
      return kSbKeySelect;
    case XK_Print:
      return kSbKeyPrint;
    case XK_Execute:
      return kSbKeyExecute;
    case XK_Insert:
    case XK_KP_Insert:
      return kSbKeyInsert;
    case XK_Help:
      return kSbKeyHelp;
    case XK_Super_L:
      return kSbKeyLwin;
    case XK_Super_R:
      return kSbKeyRwin;
    case XK_Menu:
      return kSbKeyApps;
    case XK_F1:
    case XK_F2:
    case XK_F3:
    case XK_F4:
    case XK_F5:
    case XK_F6:
    case XK_F7:
    case XK_F8:
    case XK_F9:
    case XK_F10:
    case XK_F11:
    case XK_F12:
    case XK_F13:
    case XK_F14:
    case XK_F15:
    case XK_F16:
    case XK_F17:
    case XK_F18:
    case XK_F19:
    case XK_F20:
    case XK_F21:
    case XK_F22:
    case XK_F23:
    case XK_F24:
      return static_cast<SbKey>(kSbKeyF1 + (keysym - XK_F1));
    case XK_KP_F1:
    case XK_KP_F2:
    case XK_KP_F3:
    case XK_KP_F4:
      return static_cast<SbKey>(kSbKeyF1 + (keysym - XK_KP_F1));

    // When evdev is in use, /usr/share/X11/xkb/symbols/inet maps F13-18 keys
    // to the special XF86XK symbols to support Microsoft Ergonomic keyboards:
    // https://bugs.freedesktop.org/show_bug.cgi?id=5783
    // In Chrome, we map these X key symbols back to F13-18 since we don't have
    // VKEYs for these XF86XK symbols.
    case XF86XK_Tools:
      return kSbKeyF13;
    case XF86XK_Launch5:
      return kSbKeyF14;
    case XF86XK_Launch6:
      return kSbKeyF15;
    case XF86XK_Launch7:
      return kSbKeyF16;
    case XF86XK_Launch8:
      return kSbKeyF17;
    case XF86XK_Launch9:
      return kSbKeyF18;

    // For supporting multimedia buttons on a USB keyboard.
    case XF86XK_Back:
      return kSbKeyBrowserBack;
    case XF86XK_Forward:
      return kSbKeyBrowserForward;
    case XF86XK_Reload:
      return kSbKeyBrowserRefresh;
    case XF86XK_Stop:
      return kSbKeyBrowserStop;
    case XF86XK_Search:
      return kSbKeyBrowserSearch;
    case XF86XK_Favorites:
      return kSbKeyBrowserFavorites;
    case XF86XK_HomePage:
      return kSbKeyBrowserHome;
    case XF86XK_AudioMute:
      return kSbKeyVolumeMute;
    case XF86XK_AudioLowerVolume:
      return kSbKeyVolumeDown;
    case XF86XK_AudioRaiseVolume:
      return kSbKeyVolumeUp;
    case XF86XK_AudioNext:
      return kSbKeyMediaNextTrack;
    case XF86XK_AudioPrev:
      return kSbKeyMediaPrevTrack;
    case XF86XK_AudioStop:
      return kSbKeyMediaStop;
    case XF86XK_AudioPlay:
      return kSbKeyMediaPlayPause;
    case XF86XK_Mail:
      return kSbKeyMediaLaunchMail;
    case XF86XK_LaunchA:  // F3 on an Apple keyboard.
      return kSbKeyMediaLaunchApp1;
    case XF86XK_LaunchB:  // F4 on an Apple keyboard.
    case XF86XK_Calculator:
      return kSbKeyMediaLaunchApp2;
    case XF86XK_WLAN:
      return kSbKeyWlan;
    case XF86XK_PowerOff:
      return kSbKeyPower;
    case XF86XK_MonBrightnessDown:
      return kSbKeyBrightnessDown;
    case XF86XK_MonBrightnessUp:
      return kSbKeyBrightnessUp;
    case XF86XK_KbdBrightnessDown:
      return kSbKeyKbdBrightnessDown;
    case XF86XK_KbdBrightnessUp:
      return kSbKeyKbdBrightnessUp;
  }
  SB_DLOG(WARNING) << "Unknown keysym: 0x" << std::hex << keysym;
  return kSbKeyUnknown;
}  // NOLINT(readability/fn_size)

// Get a SbKey from an XKeyEvent.
SbKey XKeyEventToSbKey(XKeyEvent* event) {
  // XLookupKeysym does not take into consideration the state of the lock/shift
  // etc. keys. So it is necessary to use XLookupString instead.
  KeySym keysym = XK_VoidSymbol;
  XLookupString(event, NULL, 0, &keysym, NULL);
  SbKey key = KeysymToSbKey(keysym);
  if (key == kSbKeyUnknown) {
    keysym = HardwareKeycodeToDefaultXKeysym(event->keycode);
    key = KeysymToSbKey(keysym);
  }

  return key;
}

bool XButtonEventIsWheelEvent(XButtonEvent* event) {
  // Buttons 4, 5, 6, and 7 are wheel events.
  return event->button >= 4 && event->button <= 7;
}

enum {
  kWheelUpButton = 4,
  kWheelDownButton = 5,
  kWheelLeftButton = 6,
  kWheelRightButton = 7,
  kPointerBackButton = 8,
  kPointerForwardButton = 9,
};

SbKey XButtonEventToSbKey(XButtonEvent* event) {
  SbKey key;
  switch (event->button) {
    case Button1:
      return kSbKeyMouse1;
    case Button2:
      return kSbKeyMouse2;
    case Button3:
      return kSbKeyMouse3;
    case kWheelUpButton:
      return kSbKeyUp;
    case kWheelDownButton:
      return kSbKeyDown;
    case kWheelLeftButton:
      return kSbKeyLeft;
    case kWheelRightButton:
      return kSbKeyRight;
    case kPointerBackButton:
      return kSbKeyBrowserBack;
    case kPointerForwardButton:
      return kSbKeyBrowserForward;
    default:
      return kSbKeyUnknown;
  }
  return key;
}

// Get a SbKeyLocation from an XKeyEvent.
SbKeyLocation XKeyEventToSbKeyLocation(XKeyEvent* event) {
  KeySym keysym = XK_VoidSymbol;
  XLookupString(event, NULL, 0, &keysym, NULL);
  switch (keysym) {
    case XK_Shift_L:
    case XK_Control_L:
    case XK_Meta_L:
    case XK_Alt_L:
      return kSbKeyLocationLeft;
    case XK_Shift_R:
    case XK_Control_R:
    case XK_Meta_R:
    case XK_Alt_R:
      return kSbKeyLocationRight;
  }

  return kSbKeyLocationUnspecified;
}

// Get an SbKeyModifiers from an XKeyEvent.
unsigned int XEventStateToSbKeyModifiers(unsigned int state) {
  unsigned int key_modifiers = kSbKeyModifiersNone;
  if (state & Mod1Mask) {
    key_modifiers |= kSbKeyModifiersAlt;
  }
  if (state & ControlMask) {
    key_modifiers |= kSbKeyModifiersCtrl;
  }
  if (state & Mod4Mask) {
    key_modifiers |= kSbKeyModifiersMeta;
  }
  if (state & ShiftMask) {
    key_modifiers |= kSbKeyModifiersShift;
  }
#if SB_API_VERSION >= 6
  if (state & Button1Mask) {
    key_modifiers |= kSbKeyModifiersPointerButtonLeft;
  }
  if (state & Button2Mask) {
    key_modifiers |= kSbKeyModifiersPointerButtonMiddle;
  }
  if (state & Button3Mask) {
    key_modifiers |= kSbKeyModifiersPointerButtonRight;
  }
#endif
  // Note: Button 4 and button 5 represent vertical wheel motion. As a result,
  // Button4Mask and Button5Mask do not represent a useful mouse button state
  // since the wheel up and wheel down do not have 'buttons' that can be held
  // down. The state of the Back and Forward mouse buttons is not reported to
  // X11 clients. This is not a hardware limitation, but a result of historical
  // Xorg X11 mouse driver button mapping choices.
  return key_modifiers;
}

#if SB_API_VERSION >= 6
SbInputVector XButtonEventToSbInputVectorDelta(XButtonEvent* event) {
  SbInputVector delta = {0, 0};
  switch (event->button) {
    case kWheelUpButton:
      delta.y = -1;
      break;
    case kWheelDownButton:
      delta.y = 1;
      break;
    case kWheelLeftButton:
      delta.x = -1;
      break;
    case kWheelRightButton:
      delta.x = 1;
      break;
  }
  return delta;
}
#endif

void XSendAtom(Window window, Atom atom) {
  // XLib is not thread-safe. Since we may be coming from another thread, we
  // have to open another connection to the display to inject the wake-up event.
  Display* display = XOpenDisplay(NULL);
  SB_DCHECK(display);
  XClientMessageEvent event = {0};
  event.type = ClientMessage;
  event.message_type = atom;
  event.window = window;
  event.format = 32;
  XSendEvent(display, event.window, 0, 0, reinterpret_cast<XEvent*>(&event));
  XFlush(display);
  XCloseDisplay(display);
}

// X IO error handler. Called if we lose our connection to the X server.
int IOErrorHandler(Display* display) {
  // Not much we can do here except immediately exit.
  SB_DSTACK(ERROR);
  quick_exit(0);
  return 0;
}

int ErrorHandler(Display* display, XErrorEvent* event) {
  char error_text[256] = {0};
  XGetErrorText(event->display, event->error_code, error_text,
                SB_ARRAY_SIZE_INT(error_text));
  SB_DLOG(ERROR) << "X11 Error: " << error_text;
  SB_DLOG(ERROR) << "display=" << XDisplayString(event->display);
  SB_DLOG(ERROR) << "serial=" << event->serial;
  SB_DLOG(ERROR) << "request=" << static_cast<int>(event->request_code);
  SB_DLOG(ERROR) << "minor=" << static_cast<int>(event->minor_code);
  SB_DLOG(ERROR) << "resourceid=" << event->resourceid;
  SbSystemBreakIntoDebugger();
  return 0;
}

}  // namespace

using shared::starboard::player::filter::CpuVideoFrame;

ApplicationX11::ApplicationX11()
    : wake_up_atom_(None),
      wm_delete_atom_(None),
      composite_event_id_(kSbEventIdInvalid),
      frame_read_index_(0),
      frames_updated_(false),
      display_(NULL),
      paste_buffer_key_release_pending_(false) {
  SbAudioSinkPrivate::Initialize();
}

ApplicationX11::~ApplicationX11() {
  SbAudioSinkPrivate::TearDown();
}

SbWindow ApplicationX11::CreateWindow(const SbWindowOptions* options) {
  if (!EnsureX()) {
    return kSbWindowInvalid;
  }

  SbWindow window = new SbWindowPrivate(display_, options);
  windows_.push_back(window);
  if (!dev_input_) {
    // evdev input will be sent to the first created window only.
    dev_input_.reset(DevInput::Create(window, ConnectionNumber(display_)));
  }
  return window;
}

bool ApplicationX11::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }

  SB_DCHECK(display_);
  SbWindowVector::iterator iterator =
      std::find(windows_.begin(), windows_.end(), window);
  SB_DCHECK(iterator != windows_.end());
  windows_.erase(iterator);

  if (windows_.empty()) {
    SB_DCHECK(dev_input_);
    dev_input_.reset();
  }

  delete window;
  if (windows_.empty()) {
    StopX();
  }
  return true;
}

namespace {
void CompositeCallback(void* context) {
  ApplicationX11* application = reinterpret_cast<ApplicationX11*>(context);
  application->Composite();
}
}  // namespace

void ApplicationX11::Composite() {
  if (!windows_.empty()) {
    SbWindow window = windows_[0];
    if (SbWindowIsValid(window)) {
      std::map<int, FrameInfo> frame_infos;
      {
        ScopedLock lock(frame_mutex_);
        if (frames_updated_) {
          // Increment the index to the next frame, which has been written.
          frame_read_index_ = (frame_read_index_ + 1) % kNumFrames;
          frame_infos.swap(frame_infos_[frame_read_index_]);

          // Clear the frame written flag, so we will not advance frames until
          // the next frame is written.
          frames_updated_ = false;
        }
      }
      window->BeginComposite();
      for (auto& iter : frame_infos) {
        FrameInfo& frame_info = iter.second;

        if (frame_info.frame->is_end_of_stream()) {
          continue;
        }

        scoped_refptr<CpuVideoFrame> cpu_video_frame =
            static_cast<CpuVideoFrame*>(frame_info.frame.get());

        if (cpu_video_frame &&
            cpu_video_frame->format() != CpuVideoFrame::kBGRA32) {
          cpu_video_frame = cpu_video_frame->ConvertTo(CpuVideoFrame::kBGRA32);
          frame_info.frame = cpu_video_frame;
        }
        window->CompositeVideoFrame(frame_info.x, frame_info.y,
                                    frame_info.width, frame_info.height,
                                    cpu_video_frame);
      }
      window->EndComposite();
    }
  }
  composite_event_id_ =
      SbEventSchedule(&CompositeCallback, this, kSbTimeSecond / 60);
}

void ApplicationX11::AcceptFrame(SbPlayer player,
                                 const scoped_refptr<VideoFrame>& frame,
                                 int z_index,
                                 int x,
                                 int y,
                                 int width,
                                 int height) {
  ScopedLock lock(frame_mutex_);
  // Always write ahead 1 frame of the current read frame.
  int write_index = (frame_read_index_ + 1) % kNumFrames;

  for (auto iter = frame_infos_[write_index].begin();
       iter != frame_infos_[write_index].end(); ++iter) {
    if (iter->second.player == player) {
      frame_infos_[write_index].erase(iter);
      break;
    }
  }

  // Copy the frame.
  FrameInfo& frame_info = frame_infos_[write_index][z_index];
  frame_info.player = player;
  frame_info.frame = frame;
  frame_info.z_index = z_index;
  frame_info.x = x;
  frame_info.y = y;
  frame_info.width = width;
  frame_info.height = height;

  frames_updated_ = true;
}

void ApplicationX11::Initialize() {
  // Mesa is installed on Ubuntu machines and will be selected as the default
  // EGL implementation.  This Mesa environment variable ensures that Mesa
  // internally uses its Gallium drivers for its EGL implementation.
  if (getenv("EGL_DRIVER") == NULL) {
    // putenv takes a non-const char *, and holds onto it indefinitely, so we
    // first create global writable memory and then copy the literal into it.
    static char to_put[] = "EGL_DRIVER=egl_gallium";
    SB_CHECK(!putenv(to_put));
  }
}

void ApplicationX11::Teardown() {
  StopX();
}

bool ApplicationX11::MayHaveSystemEvents() {
  return display_;
}

shared::starboard::Application::Event*
ApplicationX11::WaitForSystemEventWithTimeout(SbTime time) {
  SB_DCHECK(display_);

  shared::starboard::Application::Event* pending_event = GetPendingEvent();
  if (pending_event) {
    return pending_event;
  }

  SB_DCHECK(dev_input_);
  shared::starboard::Application::Event* evdev_event =
      dev_input_->WaitForSystemEventWithTimeout(time);

  if (!evdev_event && XPending(display_) != 0) {
    XEvent x_event;
    XNextEvent(display_, &x_event);
    return XEventToEvent(&x_event);
  }

  return evdev_event;
}

void ApplicationX11::WakeSystemEventWait() {
  SB_DCHECK(!windows_.empty());
  XSendAtom((*windows_.begin())->window, wake_up_atom_);
  SB_DCHECK(dev_input_);
  dev_input_->WakeSystemEventWait();
}

bool ApplicationX11::EnsureX() {
  // TODO: Consider thread-safety.
  if (display_) {
    return true;
  }

  XInitThreads();
  XSetIOErrorHandler(IOErrorHandler);
  XSetErrorHandler(ErrorHandler);
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    const char* display_environment = getenv("DISPLAY");
    if (display_environment == NULL) {
      SB_LOG(ERROR) << "Unable to open display, DISPLAY not set.";
    } else {
      SB_LOG(ERROR) << "Unable to open display " << display_environment << ".";
    }
    return false;
  }

  // Disable keyup events on auto-repeat to match Windows.
  // Otherwise when holding down a key, we get a keyup event before
  // each keydown event.
  int supported = false;
  XkbSetDetectableAutoRepeat(display_, True, &supported);
  SB_DCHECK(supported);

  wake_up_atom_ = XInternAtom(display_, "WakeUpAtom", 0);
  wm_delete_atom_ = XInternAtom(display_, "WM_DELETE_WINDOW", True);

  Composite();

  return true;
}

void ApplicationX11::StopX() {
  if (!display_) {
    return;
  }

  SbEventCancel(composite_event_id_);
  composite_event_id_ = kSbEventIdInvalid;

  XCloseDisplay(display_);
  display_ = NULL;
  wake_up_atom_ = None;
  wm_delete_atom_ = None;
}

shared::starboard::Application::Event* ApplicationX11::GetPendingEvent() {
  typedef struct {
    SbKey key;
    unsigned int modifiers;
  } KeyModifierData;

  static const KeyModifierData ASCIIKeyModifierMap[] = {
      // 0x00 ... 0x0F
      /* .0 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .1 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .2 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .3 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .4 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .5 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .6 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .7 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .8 */ {kSbKeyBackspace, kSbKeyModifiersNone},
      /* .9 */ {kSbKeyTab, kSbKeyModifiersNone},
      /* .A */ {kSbKeyBacktab, kSbKeyModifiersNone},
      /* .B */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .C */ {kSbKeyClear, kSbKeyModifiersNone},
      /* .D */ {kSbKeyReturn, kSbKeyModifiersNone},
      /* .E */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .F */ {kSbKeyUnknown, kSbKeyModifiersNone},

      // 0x10 ... 0x1F
      /* .0 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .1 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .2 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .3 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .4 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .5 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .6 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .7 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .8 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .9 */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .A */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .B */ {kSbKeyEscape, kSbKeyModifiersNone},
      /* .C */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .D */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .E */ {kSbKeyUnknown, kSbKeyModifiersNone},
      /* .F */ {kSbKeyUnknown, kSbKeyModifiersNone},

      // 0x20 ... 0x2F
      /* .0 */ {kSbKeySpace, kSbKeyModifiersNone},
      /* .1 */ {kSbKey1, kSbKeyModifiersShift},
      /* .2 */ {kSbKeyOem7, kSbKeyModifiersShift},
      /* .3 */ {kSbKey3, kSbKeyModifiersShift},
      /* .4 */ {kSbKey4, kSbKeyModifiersShift},
      /* .5 */ {kSbKey5, kSbKeyModifiersShift},
      /* .6 */ {kSbKey7, kSbKeyModifiersShift},
      /* .7 */ {kSbKeyOem7, kSbKeyModifiersNone},
      /* .8 */ {kSbKey9, kSbKeyModifiersShift},
      /* .9 */ {kSbKey0, kSbKeyModifiersShift},
      /* .A */ {kSbKey8, kSbKeyModifiersShift},
      /* .B */ {kSbKeyOemPlus, kSbKeyModifiersShift},
      /* .C */ {kSbKeyOemComma, kSbKeyModifiersNone},
      /* .D */ {kSbKeyOemMinus, kSbKeyModifiersNone},
      /* .E */ {kSbKeyOemPeriod, kSbKeyModifiersNone},
      /* .F */ {kSbKeyOem2, kSbKeyModifiersNone},

      // 0x30 ... 0x3F
      /* .0 */ {kSbKey0, kSbKeyModifiersNone},
      /* .1 */ {kSbKey1, kSbKeyModifiersNone},
      /* .2 */ {kSbKey2, kSbKeyModifiersNone},
      /* .3 */ {kSbKey3, kSbKeyModifiersNone},
      /* .4 */ {kSbKey4, kSbKeyModifiersNone},
      /* .5 */ {kSbKey5, kSbKeyModifiersNone},
      /* .6 */ {kSbKey6, kSbKeyModifiersNone},
      /* .7 */ {kSbKey7, kSbKeyModifiersNone},
      /* .8 */ {kSbKey8, kSbKeyModifiersNone},
      /* .9 */ {kSbKey9, kSbKeyModifiersNone},
      /* .A */ {kSbKeyOem1, kSbKeyModifiersShift},
      /* .B */ {kSbKeyOem1, kSbKeyModifiersNone},
      /* .C */ {kSbKeyOemComma, kSbKeyModifiersShift},
      /* .D */ {kSbKeyOemPlus, kSbKeyModifiersNone},
      /* .E */ {kSbKeyOemPeriod, kSbKeyModifiersShift},
      /* .F */ {kSbKeyOem2, kSbKeyModifiersShift},

      // 0x40 ... 0x4F
      /* .0 */ {kSbKey2, kSbKeyModifiersShift},
      /* .1 */ {kSbKeyA, kSbKeyModifiersShift},
      /* .2 */ {kSbKeyB, kSbKeyModifiersShift},
      /* .3 */ {kSbKeyC, kSbKeyModifiersShift},
      /* .4 */ {kSbKeyD, kSbKeyModifiersShift},
      /* .5 */ {kSbKeyE, kSbKeyModifiersShift},
      /* .6 */ {kSbKeyF, kSbKeyModifiersShift},
      /* .7 */ {kSbKeyG, kSbKeyModifiersShift},
      /* .8 */ {kSbKeyH, kSbKeyModifiersShift},
      /* .9 */ {kSbKeyI, kSbKeyModifiersShift},
      /* .A */ {kSbKeyJ, kSbKeyModifiersShift},
      /* .B */ {kSbKeyK, kSbKeyModifiersShift},
      /* .C */ {kSbKeyL, kSbKeyModifiersShift},
      /* .D */ {kSbKeyM, kSbKeyModifiersShift},
      /* .E */ {kSbKeyN, kSbKeyModifiersShift},
      /* .F */ {kSbKeyO, kSbKeyModifiersShift},

      // 0x50 ... 0x5F
      /* .0 */ {kSbKeyP, kSbKeyModifiersShift},
      /* .1 */ {kSbKeyQ, kSbKeyModifiersShift},
      /* .2 */ {kSbKeyR, kSbKeyModifiersShift},
      /* .3 */ {kSbKeyS, kSbKeyModifiersShift},
      /* .4 */ {kSbKeyT, kSbKeyModifiersShift},
      /* .5 */ {kSbKeyU, kSbKeyModifiersShift},
      /* .6 */ {kSbKeyV, kSbKeyModifiersShift},
      /* .7 */ {kSbKeyW, kSbKeyModifiersShift},
      /* .8 */ {kSbKeyX, kSbKeyModifiersShift},
      /* .9 */ {kSbKeyY, kSbKeyModifiersShift},
      /* .A */ {kSbKeyZ, kSbKeyModifiersShift},
      /* .B */ {kSbKeyOem4, kSbKeyModifiersNone},
      /* .C */ {kSbKeyOem5, kSbKeyModifiersNone},
      /* .D */ {kSbKeyOem6, kSbKeyModifiersNone},
      /* .E */ {kSbKey6, kSbKeyModifiersShift},
      /* .F */ {kSbKeyOemMinus, kSbKeyModifiersShift},

      // 0x60 ... 0x6F
      /* .0 */ {kSbKeyOem3, kSbKeyModifiersNone},
      /* .1 */ {kSbKeyA, kSbKeyModifiersNone},
      /* .2 */ {kSbKeyB, kSbKeyModifiersNone},
      /* .3 */ {kSbKeyC, kSbKeyModifiersNone},
      /* .4 */ {kSbKeyD, kSbKeyModifiersNone},
      /* .5 */ {kSbKeyE, kSbKeyModifiersNone},
      /* .6 */ {kSbKeyF, kSbKeyModifiersNone},
      /* .7 */ {kSbKeyG, kSbKeyModifiersNone},
      /* .8 */ {kSbKeyH, kSbKeyModifiersNone},
      /* .9 */ {kSbKeyI, kSbKeyModifiersNone},
      /* .A */ {kSbKeyJ, kSbKeyModifiersNone},
      /* .B */ {kSbKeyK, kSbKeyModifiersNone},
      /* .C */ {kSbKeyL, kSbKeyModifiersNone},
      /* .D */ {kSbKeyM, kSbKeyModifiersNone},
      /* .E */ {kSbKeyN, kSbKeyModifiersNone},
      /* .F */ {kSbKeyO, kSbKeyModifiersNone},

      // 0x70 ... 0x7F
      /* .0 */ {kSbKeyP, kSbKeyModifiersNone},
      /* .1 */ {kSbKeyQ, kSbKeyModifiersNone},
      /* .2 */ {kSbKeyR, kSbKeyModifiersNone},
      /* .3 */ {kSbKeyS, kSbKeyModifiersNone},
      /* .4 */ {kSbKeyT, kSbKeyModifiersNone},
      /* .5 */ {kSbKeyU, kSbKeyModifiersNone},
      /* .6 */ {kSbKeyV, kSbKeyModifiersNone},
      /* .7 */ {kSbKeyW, kSbKeyModifiersNone},
      /* .8 */ {kSbKeyX, kSbKeyModifiersNone},
      /* .9 */ {kSbKeyY, kSbKeyModifiersNone},
      /* .A */ {kSbKeyZ, kSbKeyModifiersNone},
      /* .B */ {kSbKeyOem4, kSbKeyModifiersShift},
      /* .C */ {kSbKeyOem5, kSbKeyModifiersShift},
      /* .D */ {kSbKeyOem6, kSbKeyModifiersShift},
      /* .E */ {kSbKeyOem3, kSbKeyModifiersShift},
      /* .F */ {kSbKeyUnknown, kSbKeyModifiersNone},
  };

  KeyModifierData key_modifiers;
  unsigned char character;
  while (!paste_buffer_pending_characters_.empty()) {
    character = paste_buffer_pending_characters_.front();
    if (character < SB_ARRAY_SIZE(ASCIIKeyModifierMap)) {
      key_modifiers = ASCIIKeyModifierMap[character];
      if (key_modifiers.key != kSbKeyUnknown) {
        break;
      }
    }
    paste_buffer_pending_characters_.pop();
  }

  if (paste_buffer_pending_characters_.empty()) {
    return NULL;
  }

  if (paste_buffer_key_release_pending_) {
    paste_buffer_pending_characters_.pop();
  }

  scoped_ptr<SbInputData> data(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));
  data->window = windows_[0];
  SB_DCHECK(SbWindowIsValid(data->window));
  data->type = paste_buffer_key_release_pending_ ? kSbInputEventTypeUnpress
                                                 : kSbInputEventTypePress;
  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = kKeyboardDeviceId;
  data->key = key_modifiers.key;
  data->key_location = kSbKeyLocationUnspecified;
  data->key_modifiers = key_modifiers.modifiers;
  data->position.x = 0;
  data->position.y = 0;

  paste_buffer_key_release_pending_ = !paste_buffer_key_release_pending_;
  return new Event(kSbEventTypeInput, data.release(),
                   &DeleteDestructor<SbInputData>);
}

shared::starboard::Application::Event* ApplicationX11::XEventToEvent(
    XEvent* x_event) {
  switch (x_event->type) {
    case ClientMessage: {
      const XClientMessageEvent* client_message =
          reinterpret_cast<const XClientMessageEvent*>(x_event);
      if (client_message->message_type == wake_up_atom_) {
        // We've woken up, so our job is done.
        return NULL;
      }

      if (x_event->xclient.data.l[0] == wm_delete_atom_) {
        SB_DLOG(INFO) << "Received WM_DELETE_WINDOW message.";
        // TODO: Expose this as an event to clients.
        Stop(0);
        return NULL;
      }
      // Unknown event, ignore.
      return NULL;
    }
    case KeyPress:
    case KeyRelease: {
      // User pressed key.
      XKeyEvent* x_key_event = reinterpret_cast<XKeyEvent*>(x_event);

      SbKey key = XKeyEventToSbKey(x_key_event);
      unsigned int key_modifiers =
          XEventStateToSbKeyModifiers(x_key_event->state);

      bool is_press_event = KeyPress == x_event->type;
      bool is_paste_keypress = is_press_event &&
                               (key_modifiers & kSbKeyModifiersCtrl) &&
                               key == kSbKeyV;
      is_paste_keypress |= is_press_event &&
                           (key_modifiers & kSbKeyModifiersShift) &&
                           key == kSbKeyInsert;
      if (is_paste_keypress) {
        // Handle Ctrl-V or Shift-Insert as paste.
        const Atom xtarget = XInternAtom(x_key_event->display, "TEXT", 0);
        // Request the paste buffer, which will be sent as a separate
        // SelectionNotify XEvent.
        XConvertSelection(x_key_event->display, XA_PRIMARY, xtarget, XA_PRIMARY,
                          x_key_event->window, CurrentTime);
        return NULL;
      }

      scoped_ptr<SbInputData> data(new SbInputData());
      SbMemorySet(data.get(), 0, sizeof(*data));
      data->window = FindWindow(x_key_event->window);
      SB_DCHECK(SbWindowIsValid(data->window));
      data->type = x_event->type == KeyPress ? kSbInputEventTypePress
                                             : kSbInputEventTypeUnpress;
      data->device_type = kSbInputDeviceTypeKeyboard;
      data->device_id = kKeyboardDeviceId;
      data->key = key;
      data->key_modifiers = key_modifiers;
      data->key_location = XKeyEventToSbKeyLocation(x_key_event);
      data->position.x = x_key_event->x;
      data->position.y = x_key_event->y;
      return new Event(kSbEventTypeInput, data.release(),
                       &DeleteDestructor<SbInputData>);
    }
    case ButtonPress:
    case ButtonRelease: {
      XButtonEvent* x_button_event = reinterpret_cast<XButtonEvent*>(x_event);
      bool is_press_event = ButtonPress == x_event->type;
      bool is_wheel_event = XButtonEventIsWheelEvent(x_button_event);
#if SB_API_VERSION >= 6
      if (is_wheel_event && !is_press_event) {
        // unpress events from the wheel are discarded.
        return NULL;
      }
#endif
      scoped_ptr<SbInputData> data(new SbInputData());
      SbMemorySet(data.get(), 0, sizeof(*data));
      data->window = FindWindow(x_button_event->window);
      SB_DCHECK(SbWindowIsValid(data->window));
      data->key = XButtonEventToSbKey(x_button_event);
      data->type =
          is_press_event ? kSbInputEventTypePress : kSbInputEventTypeUnpress;
      data->device_type = kSbInputDeviceTypeMouse;
      if (is_wheel_event) {
#if SB_API_VERSION >= 6
        data->pressure = NAN;
        data->size = {NAN, NAN};
        data->tilt = {NAN, NAN};
        data->type = kSbInputEventTypeWheel;
        data->delta = XButtonEventToSbInputVectorDelta(x_button_event);
#else
        // This version of Starboard does not support wheel event types, send
        // keyboard event types instead.
        data->device_type = kSbInputDeviceTypeKeyboard;
#endif
      }
      data->device_id = kMouseDeviceId;
      data->key_modifiers = XEventStateToSbKeyModifiers(x_button_event->state);
      data->position.x = x_button_event->x;
      data->position.y = x_button_event->y;
      return new Event(kSbEventTypeInput, data.release(),
                       &DeleteDestructor<SbInputData>);
    }
    case MotionNotify: {
      XMotionEvent* x_motion_event = reinterpret_cast<XMotionEvent*>(x_event);
      scoped_ptr<SbInputData> data(new SbInputData());
      SbMemorySet(data.get(), 0, sizeof(*data));
      data->window = FindWindow(x_motion_event->window);
      SB_DCHECK(SbWindowIsValid(data->window));
#if SB_API_VERSION >= 6
      data->pressure = NAN;
      data->size = {NAN, NAN};
      data->tilt = {NAN, NAN};
#endif
      data->type = kSbInputEventTypeMove;
      data->device_type = kSbInputDeviceTypeMouse;
      data->device_id = kMouseDeviceId;
      data->key_modifiers = XEventStateToSbKeyModifiers(x_motion_event->state);
      data->position.x = x_motion_event->x;
      data->position.y = x_motion_event->y;
      return new Event(kSbEventTypeInput, data.release(),
                       &DeleteDestructor<SbInputData>);
    }
    case FocusIn: {
      Unpause(NULL, NULL);
      return NULL;
    }
    case FocusOut: {
      Pause(NULL, NULL);
      return NULL;
    }
    case ConfigureNotify: {
#if SB_API_VERSION >= 8
      XConfigureEvent* x_configure_event =
          reinterpret_cast<XConfigureEvent*>(x_event);
      scoped_ptr<SbEventWindowSizeChangedData> data(
          new SbEventWindowSizeChangedData());
      data->window = FindWindow(x_configure_event->window);
      bool unhandled_resize = data->window->unhandled_resize;
      data->window->BeginComposite();
      unhandled_resize |= data->window->unhandled_resize;
      if (!unhandled_resize) {
        // Ignore move events.
        return NULL;
      }
      // Get the current window size.
      SbWindowSize window_size;
      SbWindowGetSize(data->window, &window_size);
      data->size = window_size;
      data->window->unhandled_resize = false;
      return new Event(kSbEventTypeWindowSizeChanged, data.release(),
                       &DeleteDestructor<SbInputData>);
#else   // SB_API_VERSION >= 8
      return NULL;
#endif  // SB_API_VERSION >= 8
    }
    case SelectionNotify: {
      XSelectionEvent* x_selection_event =
          reinterpret_cast<XSelectionEvent*>(x_event);

      unsigned long nitems = 0;       // NOLINT(runtime/int)
      unsigned long bytes_after = 0;  // NOLINT(runtime/int)
      int format = 0;
      unsigned char* property = NULL;
      Atom type = XA_PRIMARY;

      if (XGetWindowProperty(x_selection_event->display,
                             x_selection_event->requestor, XA_PRIMARY, 0, 4096,
                             False, AnyPropertyType, &type, &format, &nitems,
                             &bytes_after, &property)) {
        return NULL;
      }

      if (property && nitems) {
        for (unsigned char* ptr = property; *ptr; ++ptr) {
          paste_buffer_pending_characters_.push(*ptr);
        }
      }
      XFree(property);
      break;
    }
    default: {
      SB_DLOG(INFO) << "Unrecognized event type = " << x_event->type;
      break;
    }
  }

  return NULL;
}

SbWindow ApplicationX11::FindWindow(Window window) {
  for (SbWindowVector::iterator i = windows_.begin(); i != windows_.end();
       ++i) {
    if ((*i)->window == window) {
      return *i;
    }
  }
  return kSbWindowInvalid;
}

}  // namespace x11
}  // namespace shared
}  // namespace starboard
