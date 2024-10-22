// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/starboard/starboard_platform_event_source.h"

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

    // Keys which are not used by the Cast SDK, but are defined in the HDMI CEC
    // specification.
    {kSbKeyMenu, ui::DomCode::HOME},
    {kSbKeyChannelUp, ui::DomCode::CHANNEL_UP},
    {kSbKeyChannelDown, ui::DomCode::CHANNEL_DOWN},
    {kSbKeyClosedCaption, ui::DomCode::CLOSED_CAPTION_TOGGLE},
#if SB_API_VERSION >= 15
    {kSbKeyRecord, ui::DomCode::MEDIA_RECORD},
#endif  // SB_API_VERSION >=15
});
// Hack:
static scoped_refptr<base::TaskRunner> g_reply_runner_;

void DeliverEventHandler(std::unique_ptr<ui::Event> ui_event) {
    CHECK(ui::PlatformEventSource::GetInstance());
    LOG(INFO) << "PlatformEventSource::GetInstance=" << ui::PlatformEventSource::GetInstance();
    static_cast<StarboardPlatformEventSource*>(ui::PlatformEventSource::GetInstance())->DeliverEvent(std::move(ui_event));
}

void StarboardPlatformEventSource::SbEventHandle(const SbEvent* event) {
    SbLogRawFormatF("SbEvent type=%d\n", event->type);

  if (event->type != kSbEventTypeInput) {
    return;
  }

  SbLogRawFormatF("kSbEventTypeInput data=%p\n", event->data);
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
  LOG(INFO) << "SbInputData type=" << type_name
      << " device_type=" << input_data->device_type
      << " device_id=" << input_data->device_id
      << " key=" << input_data->key
      << " character=" << static_cast<int>(input_data->character)
      << " key_location=" << input_data->key_location
      << " key_modifiers=" << input_data->key_modifiers
      << " postion.x=" << input_data->position.x
      << " postion.y=" << input_data->position.y
      << " delta.x=" << input_data->delta.x
      << " delta.y=" << input_data->delta.y
      << " pressure=" << input_data->pressure
      << " size.x=" << input_data->size.x
      << " size.y=" << input_data->size.y
      << " tilt.x=" << input_data->tilt.x
      << " tilt.y=" << input_data->tilt.y
      << " input_text=" << (input_data->input_text? input_data->input_text: "null")
      << " is_composing=" << input_data->is_composing;

  std::unique_ptr<ui::Event> ui_event;

  if (input_data->device_type == kSbInputDeviceTypeKeyboard) {
    SbKey raw_key = input_data->key;
    LOG(INFO) << "raw_type=" << raw_type;
    if (raw_type != kSbInputEventTypePress &&
        raw_type != kSbInputEventTypeUnpress) {
      LOG(INFO) << "Not press/unpress";
      return;
    }

    auto it = kSbKeyToDomCodeMap.find(raw_key);
    if (it == kSbKeyToDomCodeMap.end()) {
      LOG(INFO) << "Not in the map";
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
      int flag  = ui::EF_NONE;
      if (kSbInputEventTypePress) {
        flag = ui::EF_LEFT_MOUSE_BUTTON;
      }

        ui_event = std::make_unique<ui::MouseEvent>(event_type, gfx::PointF(input_data->position.x,input_data->position.y),
      gfx::PointF{}, base::TimeTicks() + base::Microseconds(raw_timestamp), flag, ui::EF_LEFT_MOUSE_BUTTON);
  } else {
    LOG(INFO) << "Ignoring unknown device type";
    return;
  }

  g_reply_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DeliverEventHandler, std::move(ui_event)));
}


StarboardPlatformEventSource::StarboardPlatformEventSource() {
  LOG(INFO) << "StarboardPlatformEventSource::StarboardPlatformEventSource";
  sb_main_ = std::make_unique<std::thread>(
      &SbRunStarboardMain, /*argc=*/0, /*argv=*/nullptr,
      &SbEventHandle);
    g_reply_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    sb_main_->detach();
}

uint32_t StarboardPlatformEventSource::DeliverEvent(std::unique_ptr<ui::Event> ui_event) {
  LOG(INFO) << "DispatchEvent ";
  return DispatchEvent(ui_event.get());
}

StarboardPlatformEventSource:: ~StarboardPlatformEventSource() {
}

}
