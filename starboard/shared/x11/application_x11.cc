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

#include <stdlib.h>
#include <unistd.h>
#define XK_3270  // for XK_3270_BackTab
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <iomanip>

#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/posix/time_internal.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/x11/window_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace x11 {
namespace {
const int kKeyboardDeviceId = 1;

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
unsigned int XKeyEventToSbKeyModifiers(XKeyEvent* event) {
  unsigned int key_modifiers = kSbKeyModifiersNone;
  if (event->state & Mod1Mask) {
    key_modifiers |= kSbKeyModifiersAlt;
  }
  if (event->state & ControlMask) {
    key_modifiers |= kSbKeyModifiersCtrl;
  }
  if (event->state & ShiftMask) {
    key_modifiers |= kSbKeyModifiersShift;
  }
  return key_modifiers;
}

bool XNextEventTimed(Display* display, XEvent* out_event, SbTime duration) {
  if (XPending(display) == 0) {
    if (duration <= SbTime()) {
      return false;
    }

    int fd = ConnectionNumber(display);
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    struct timeval tv;
    SbTime clamped_duration = std::max(duration, SbTime());
    ToTimevalDuration(clamped_duration, &tv);
    if (select(fd + 1, &read_set, NULL, NULL, &tv) == 0) {
      return false;
    }
  }

  XNextEvent(display, out_event);
  return true;
}

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

using shared::starboard::player::VideoFrame;

ApplicationX11::ApplicationX11()
    : wake_up_atom_(None),
      wm_delete_atom_(None),
#if SB_IS(PLAYER_PUNCHED_OUT)
      composite_event_id_(kSbEventIdInvalid),
      frame_read_index_(0),
      frame_written_(false),
#endif  // SB_IS(PLAYER_PUNCHED_OUT)
      display_(NULL) {
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
  delete window;
  if (windows_.empty()) {
    StopX();
  }
  return true;
}

#if SB_IS(PLAYER_PUNCHED_OUT)
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
      int index = -1;
      {
        ScopedLock lock(frame_mutex_);
        if (frame_written_) {
          // Clear the old frame, now that we are done with it.
          frame_infos_[frame_read_index_].frame = VideoFrame();

          // Increment the index to the next frame, which has been written.
          frame_read_index_ = (frame_read_index_ + 1) % kNumFrames;

          // Clear the frame written flag, so we will not advance frames until
          // the next frame is written.
          frame_written_ = false;
        }
        index = frame_read_index_;
      }
      FrameInfo& frame_info = frame_infos_[frame_read_index_];

      if (!frame_info.frame.IsEndOfStream() &&
          frame_info.frame.format() != VideoFrame::kBGRA32) {
        frame_info.frame = frame_info.frame.ConvertTo(VideoFrame::kBGRA32);
      }
      window->Composite(frame_info.x, frame_info.y, frame_info.width,
                        frame_info.height, &frame_info.frame);
    }
  }
  composite_event_id_ =
      SbEventSchedule(&CompositeCallback, this, kSbTimeSecond / 60);
}

void ApplicationX11::AcceptFrame(SbPlayer player,
                                 const VideoFrame& frame,
                                 int x,
                                 int y,
                                 int width,
                                 int height) {
  int write_index = -1;
  {
    ScopedLock lock(frame_mutex_);
    // Always write ahead 1 frame of the current read frame.
    write_index = (frame_read_index_ + 1) % kNumFrames;

    // Since we are about to modify the next frame, we need to ensure that the
    // reader will not try to advance frames concurrently, so we clear the flag
    // stating the frame has been written.
    frame_written_ = false;
  }

  // Copy the frame.
  frame_infos_[write_index].frame = frame;
  frame_infos_[write_index].x = x;
  frame_infos_[write_index].y = y;
  frame_infos_[write_index].width = width;
  frame_infos_[write_index].height = height;

  {
    ScopedLock lock(frame_mutex_);
    // The next frame is now ready to be read.
    frame_written_ = true;
  }
}
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

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

  XEvent x_event;

  if (XNextEventTimed(display_, &x_event, time)) {
    return XEventToEvent(&x_event);
  } else {
    return NULL;
  }
}

void ApplicationX11::WakeSystemEventWait() {
  SB_DCHECK(!windows_.empty());
  XSendAtom((*windows_.begin())->window, wake_up_atom_);
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
    const char *display_environment = getenv("DISPLAY");
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

#if SB_IS(PLAYER_PUNCHED_OUT)
  Composite();
#endif  // SB_IS(PLAYER_PUNCHED_OUT)
  return true;
}

void ApplicationX11::StopX() {
  if (!display_) {
    return;
  }

#if SB_IS(PLAYER_PUNCHED_OUT)
  SbEventCancel(composite_event_id_);
  composite_event_id_ = kSbEventIdInvalid;
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

  XCloseDisplay(display_);
  display_ = NULL;
  wake_up_atom_ = None;
  wm_delete_atom_ = None;
}

shared::starboard::Application::Event* ApplicationX11::XEventToEvent(
    XEvent* x_event) {
  if (x_event->type == ClientMessage) {
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

  if (x_event->type == KeyPress || x_event->type == KeyRelease) {
    // User pressed key.
    XKeyEvent* x_key_event = reinterpret_cast<XKeyEvent*>(x_event);
    SbInputData* data = new SbInputData();
    SbMemorySet(data, 0, sizeof(*data));
    data->window = FindWindow(x_key_event->window);
    SB_DCHECK(SbWindowIsValid(data->window));
    data->type = (x_event->type == KeyPress ? kSbInputEventTypePress
                                            : kSbInputEventTypeUnpress);
    data->device_type = kSbInputDeviceTypeKeyboard;
    data->device_id = kKeyboardDeviceId;
    data->key = XKeyEventToSbKey(x_key_event);
    data->key_location = XKeyEventToSbKeyLocation(x_key_event);
    data->key_modifiers = XKeyEventToSbKeyModifiers(x_key_event);
    return new Event(kSbEventTypeInput, data, &DeleteDestructor<SbInputData>);
  } else if (x_event->type == FocusIn) {
    Unpause(NULL, NULL);
    return NULL;
  } else if (x_event->type == FocusOut) {
    Pause(NULL, NULL);
    return NULL;
  } else if (x_event->type == MapNotify) {
    Resume(NULL, NULL);
    return NULL;
  } else if (x_event->type == UnmapNotify) {
    Suspend(NULL, NULL);
    return NULL;
  }

  SB_DLOG(INFO) << "Unrecognized event type = " << x_event->type;

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
