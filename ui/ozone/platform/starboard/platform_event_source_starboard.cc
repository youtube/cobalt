// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"

#include "base/logging.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "ui/events/event.h"

#include "base/containers/fixed_flat_map.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

#include "ui/events/types/event_type.h"

namespace starboard {

constexpr auto kSbKeyToDomCodeMap = base::MakeFixedFlatMap<SbKey, ui::DomCode>({
    // Convenience keys for keyboard support.
    {kSbKeySpace, ui::DomCode::MEDIA_PLAY_PAUSE},
    // {kSbKeySpace, ui::DomCode::SPACE}, Should this be space or play/pause

    // Keys which are used by the Cast SDK.
    {kSbKeyReturn, ui::DomCode::ENTER},
    {kSbKeySelect, ui::DomCode::SELECT},
    {kSbKeyUp, ui::DomCode::ARROW_UP},
    {kSbKeyDown, ui::DomCode::ARROW_DOWN},
    {kSbKeyLeft, ui::DomCode::ARROW_LEFT},
    {kSbKeyRight, ui::DomCode::ARROW_RIGHT},
    {kSbKeyBack, ui::DomCode::BROWSER_BACK},

    // Keys which are used by the Cast SDK when the DPAD UI is enabled.
    {kSbKeyMediaPlayPause, ui::DomCode::MEDIA_PLAY_PAUSE},
    {kSbKeyMediaRewind, ui::DomCode::MEDIA_REWIND},
    {kSbKeyMediaFastForward, ui::DomCode::MEDIA_FAST_FORWARD},
    {kSbKeyMediaNextTrack, ui::DomCode::MEDIA_TRACK_NEXT},
    {kSbKeyMediaPrevTrack, ui::DomCode::MEDIA_TRACK_PREVIOUS},
    {kSbKeyPause, ui::DomCode::MEDIA_PAUSE},
    {kSbKeyPlay, ui::DomCode::MEDIA_PLAY},
    {kSbKeyMediaStop, ui::DomCode::MEDIA_STOP},

    // Keys which are not used by the Cast SDK, but are defined in the HDMI
    // CEC specification.
    {kSbKeyMenu, ui::DomCode::HOME},
    {kSbKeyChannelUp, ui::DomCode::CHANNEL_UP},
    {kSbKeyChannelDown, ui::DomCode::CHANNEL_DOWN},
    {kSbKeyClosedCaption, ui::DomCode::CLOSED_CAPTION_TOGGLE},
    {kSbKeyRecord, ui::DomCode::MEDIA_RECORD},

    // 0-9, a-z
    {kSbKey0, ui::DomCode::DIGIT0},
    {kSbKey1, ui::DomCode::DIGIT1},
    {kSbKey2, ui::DomCode::DIGIT2},
    {kSbKey3, ui::DomCode::DIGIT3},
    {kSbKey4, ui::DomCode::DIGIT4},
    {kSbKey5, ui::DomCode::DIGIT5},
    {kSbKey6, ui::DomCode::DIGIT6},
    {kSbKey7, ui::DomCode::DIGIT7},
    {kSbKey8, ui::DomCode::DIGIT8},
    {kSbKey9, ui::DomCode::DIGIT9},
    {kSbKeyA, ui::DomCode::US_A},
    {kSbKeyB, ui::DomCode::US_B},
    {kSbKeyC, ui::DomCode::US_C},
    {kSbKeyD, ui::DomCode::US_D},
    {kSbKeyE, ui::DomCode::US_E},
    {kSbKeyF, ui::DomCode::US_F},
    {kSbKeyG, ui::DomCode::US_G},
    {kSbKeyH, ui::DomCode::US_H},
    {kSbKeyI, ui::DomCode::US_I},
    {kSbKeyJ, ui::DomCode::US_J},
    {kSbKeyK, ui::DomCode::US_K},
    {kSbKeyL, ui::DomCode::US_L},
    {kSbKeyM, ui::DomCode::US_M},
    {kSbKeyN, ui::DomCode::US_N},
    {kSbKeyO, ui::DomCode::US_O},
    {kSbKeyP, ui::DomCode::US_P},
    {kSbKeyQ, ui::DomCode::US_Q},
    {kSbKeyR, ui::DomCode::US_R},
    {kSbKeyS, ui::DomCode::US_S},
    {kSbKeyT, ui::DomCode::US_T},
    {kSbKeyU, ui::DomCode::US_U},
    {kSbKeyV, ui::DomCode::US_V},
    {kSbKeyW, ui::DomCode::US_W},
    {kSbKeyX, ui::DomCode::US_X},
    {kSbKeyY, ui::DomCode::US_Y},
    {kSbKeyZ, ui::DomCode::US_Z},

    // Numpad keys
    {kSbKeyNumpad0, ui::DomCode::NUMPAD0},
    {kSbKeyNumpad1, ui::DomCode::NUMPAD1},
    {kSbKeyNumpad2, ui::DomCode::NUMPAD2},
    {kSbKeyNumpad3, ui::DomCode::NUMPAD3},
    {kSbKeyNumpad4, ui::DomCode::NUMPAD4},
    {kSbKeyNumpad5, ui::DomCode::NUMPAD5},
    {kSbKeyNumpad6, ui::DomCode::NUMPAD6},
    {kSbKeyNumpad7, ui::DomCode::NUMPAD7},
    {kSbKeyNumpad8, ui::DomCode::NUMPAD8},
    {kSbKeyNumpad9, ui::DomCode::NUMPAD9},
    {kSbKeyMultiply, ui::DomCode::NUMPAD_MULTIPLY},
    {kSbKeyAdd, ui::DomCode::NUMPAD_ADD},
    {kSbKeySubtract, ui::DomCode::NUMPAD_SUBTRACT},
    {kSbKeyDecimal, ui::DomCode::NUMPAD_DECIMAL},
    {kSbKeyDivide, ui::DomCode::NUMPAD_DIVIDE},
    {kSbKeyNumlock, ui::DomCode::NUM_LOCK},

    // function keys
    {kSbKeyF1, ui::DomCode::F1},
    {kSbKeyF2, ui::DomCode::F2},
    {kSbKeyF3, ui::DomCode::F3},
    {kSbKeyF4, ui::DomCode::F4},
    {kSbKeyF5, ui::DomCode::F5},
    {kSbKeyF6, ui::DomCode::F6},
    {kSbKeyF7, ui::DomCode::F7},
    {kSbKeyF8, ui::DomCode::F8},
    {kSbKeyF9, ui::DomCode::F9},
    {kSbKeyF10, ui::DomCode::F10},
    {kSbKeyF11, ui::DomCode::F11},
    {kSbKeyF12, ui::DomCode::F12},
    {kSbKeyF13, ui::DomCode::F13},
    {kSbKeyF14, ui::DomCode::F14},
    {kSbKeyF15, ui::DomCode::F15},
    {kSbKeyF16, ui::DomCode::F16},
    {kSbKeyF17, ui::DomCode::F17},
    {kSbKeyF18, ui::DomCode::F18},
    {kSbKeyF19, ui::DomCode::F19},
    {kSbKeyF20, ui::DomCode::F20},
    {kSbKeyF21, ui::DomCode::F21},
    {kSbKeyF22, ui::DomCode::F22},
    {kSbKeyF23, ui::DomCode::F23},
    {kSbKeyF24, ui::DomCode::F24},

    // Browser keys
    {kSbKeyBrowserBack, ui::DomCode::BROWSER_BACK},
    {kSbKeyBrowserForward, ui::DomCode::BROWSER_FORWARD},
    {kSbKeyBrowserRefresh, ui::DomCode::BROWSER_REFRESH},
    {kSbKeyBrowserStop, ui::DomCode::BROWSER_STOP},
    {kSbKeyBrowserSearch, ui::DomCode::BROWSER_SEARCH},
    {kSbKeyBrowserFavorites, ui::DomCode::BROWSER_FAVORITES},
    {kSbKeyBrowserHome, ui::DomCode::BROWSER_HOME},

    // Left/Right specific keys
    {kSbKeyLwin, ui::DomCode::META_LEFT},
    {kSbKeyRwin, ui::DomCode::META_RIGHT},
    {kSbKeyLshift, ui::DomCode::SHIFT_LEFT},
    {kSbKeyRshift, ui::DomCode::SHIFT_RIGHT},
    {kSbKeyLcontrol, ui::DomCode::CONTROL_LEFT},
    {kSbKeyRcontrol, ui::DomCode::CONTROL_RIGHT},

    // Oem keys
    {kSbKeyOemPlus, ui::DomCode::EQUAL},
    {kSbKeyOemComma, ui::DomCode::COMMA},
    {kSbKeyOem1, ui::DomCode::SEMICOLON},
    {kSbKeyOem2, ui::DomCode::SLASH},
    {kSbKeyOem4, ui::DomCode::BRACKET_LEFT},
    {kSbKeyOem5, ui::DomCode::BACKSLASH},
    {kSbKeyOem6, ui::DomCode::BRACKET_RIGHT},

    // Volume keys
    {kSbKeyVolumeMute, ui::DomCode::VOLUME_MUTE},
    {kSbKeyVolumeDown, ui::DomCode::VOLUME_DOWN},
    {kSbKeyVolumeUp, ui::DomCode::VOLUME_UP},

    // Brightness keys
    {kSbKeyBrightnessDown, ui::DomCode::BRIGHTNESS_DOWN},
    {kSbKeyBrightnessUp, ui::DomCode::BRIGHTNESS_UP},
    {kSbKeyKbdBrightnessDown, ui::DomCode::BRIGHTNESS_DOWN},
    {kSbKeyKbdBrightnessUp, ui::DomCode::BRIGHTNESS_UP},

    {kSbKeyPrior, ui::DomCode::PAGE_UP},
    {kSbKeyNext, ui::DomCode::PAGE_DOWN},
    {kSbKeyEnd, ui::DomCode::END},
    {kSbKeyHome, ui::DomCode::HOME},
    {kSbKeyPrint, ui::DomCode::PRINT},
    {kSbKeyExecute, ui::DomCode::OPEN},
    {kSbKeyInsert, ui::DomCode::INSERT},
    {kSbKeyDelete, ui::DomCode::DEL},
    {kSbKeyCapital, ui::DomCode::CAPS_LOCK},

    {kSbKeySleep, ui::DomCode::SLEEP},
    {kSbKeyPower, ui::DomCode::POWER},
    {kSbKeyCancel, ui::DomCode::ABORT},
    {kSbKeyTab, ui::DomCode::TAB},
    {kSbKeyEscape, ui::DomCode::ESCAPE},
});
// TODO(b/391414243): Implement this in a clean way.
#if BUILDFLAG(ENABLE_COBALT_HACKS)
static scoped_refptr<base::TaskRunner> g_reply_runner_;
#endif  // BUILDFLAG(ENABLE_COBALT_HACKS)

void DeliverEventHandler(std::unique_ptr<ui::Event> ui_event) {
  CHECK(ui::PlatformEventSource::GetInstance());
  static_cast<PlatformEventSourceStarboard*>(
      ui::PlatformEventSource::GetInstance())
      ->DeliverEvent(std::move(ui_event));
}

void PlatformEventSourceStarboard::SbEventHandle(const SbEvent* event) {
  if (event->type != kSbEventTypeInput) {
    return;
  }

  if (event->data == nullptr) {
    return;
  }
  auto* input_data = static_cast<SbInputData*>(event->data);

  int64_t raw_timestamp = event->timestamp;
  SbInputEventType raw_type = input_data->type;

  std::string type_name;
  switch (input_data->type) {
    case kSbInputEventTypeMove:
      type_name = "kSbInputEventTypeMove";
      break;
    case kSbInputEventTypePress:
      type_name = "kSbInputEventTypePress";
      break;
    case kSbInputEventTypeUnpress:
      type_name = "kSbInputEventTypeUnpress";
      break;
    case kSbInputEventTypeWheel:
      type_name = "kSbInputEventTypeWheel";
      break;
    case kSbInputEventTypeInput:
      type_name = "kSbInputEventTypeInput";
      break;
  }

  std::unique_ptr<ui::Event> ui_event;

  if (input_data->device_type == kSbInputDeviceTypeKeyboard) {
    SbKey raw_key = input_data->key;
    if (raw_type != kSbInputEventTypePress &&
        raw_type != kSbInputEventTypeUnpress) {
      return;
    }

    auto it = kSbKeyToDomCodeMap.find(raw_key);
    if (it == kSbKeyToDomCodeMap.end()) {
      return;
    }

    ui::DomKey dom_key;
    ui::KeyboardCode key_code;
    ui::DomCode dom_code = it->second;

    int flags = 0;
    if (!DomCodeToUsLayoutDomKey(dom_code, flags, &dom_key, &key_code)) {
      return;
    }

    // Key press.
    ui::EventType event_type = raw_type == kSbInputEventTypePress
                                   ? ui::EventType::ET_KEY_PRESSED
                                   : ui::EventType::ET_KEY_RELEASED;
    ui_event = std::make_unique<ui::KeyEvent>(
        event_type, key_code, dom_code, flags, dom_key,
        /*time_stamp=*/
        base::TimeTicks() + base::Microseconds(raw_timestamp));
  } else if (input_data->device_type == kSbInputDeviceTypeMouse) {
    ui::EventType event_type = ui::EventType::ET_UNKNOWN;
    switch (input_data->type) {
      case kSbInputEventTypeMove:
        event_type = ui::EventType::ET_MOUSE_MOVED;
        break;
      case kSbInputEventTypePress:
        event_type = ui::EventType::ET_MOUSE_PRESSED;
        break;
      case kSbInputEventTypeUnpress:
        event_type = ui::EventType::ET_MOUSE_RELEASED;
        break;
      case kSbInputEventTypeWheel:
        event_type = ui::EventType::ET_MOUSEWHEEL;
        break;
      case kSbInputEventTypeInput:
        break;
    }
    int flag = ui::EF_NONE;
    if (kSbInputEventTypePress) {
      flag = ui::EF_LEFT_MOUSE_BUTTON;
    }

    // Mouse wheel scrolls are separate from MouseEvent and will crash here. We
    // need to handle them properly.
    if (event_type == ui::EventType::ET_MOUSEWHEEL) {
      return;
    }
    ui_event = std::make_unique<ui::MouseEvent>(
        event_type, gfx::PointF(input_data->position.x, input_data->position.y),
        gfx::PointF{}, base::TimeTicks() + base::Microseconds(raw_timestamp),
        flag, ui::EF_LEFT_MOUSE_BUTTON);
  } else {
    return;
  }
  // TODO(b/391414243): Implement this in a clean way.
#if BUILDFLAG(ENABLE_COBALT_HACKS)
  g_reply_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeliverEventHandler, std::move(ui_event)));
#endif  // BUILDFLAG(ENABLE_COBALT_HACKS)
}

PlatformEventSourceStarboard::PlatformEventSourceStarboard() {
  // TODO(b/391414243): Initialize Starboard correctly.
#if BUILDFLAG(ENABLE_COBALT_HACKS)
  sb_main_ = std::make_unique<std::thread>(&SbRunStarboardMain, /*argc=*/0,
                                           /*argv=*/nullptr, &SbEventHandle);
  g_reply_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  sb_main_->detach();
#endif  // BUILDFLAG(ENABLE_COBALT_HACKS)
}

uint32_t PlatformEventSourceStarboard::DeliverEvent(
    std::unique_ptr<ui::Event> ui_event) {
  return DispatchEvent(ui_event.get());
}

PlatformEventSourceStarboard::~PlatformEventSourceStarboard() {}
}  // namespace starboard
