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
namespace {

constexpr auto kSbKeyToDomCodeMap = base::MakeFixedFlatMap<SbKey, ui::DomCode>({
    {kSbKeySpace, ui::DomCode::MEDIA_PLAY_PAUSE},

    {kSbKeyReturn, ui::DomCode::ENTER},
    {kSbKeySelect, ui::DomCode::SELECT},
    {kSbKeyUp, ui::DomCode::ARROW_UP},
    {kSbKeyDown, ui::DomCode::ARROW_DOWN},
    {kSbKeyLeft, ui::DomCode::ARROW_LEFT},
    {kSbKeyRight, ui::DomCode::ARROW_RIGHT},
    {kSbKeyBack, ui::DomCode::BROWSER_BACK},

    {kSbKeyMediaPlayPause, ui::DomCode::MEDIA_PLAY_PAUSE},
    {kSbKeyMediaRewind, ui::DomCode::MEDIA_REWIND},
    {kSbKeyMediaFastForward, ui::DomCode::MEDIA_FAST_FORWARD},
    {kSbKeyMediaNextTrack, ui::DomCode::MEDIA_TRACK_NEXT},
    {kSbKeyMediaPrevTrack, ui::DomCode::MEDIA_TRACK_PREVIOUS},
    {kSbKeyPause, ui::DomCode::MEDIA_PAUSE},
    {kSbKeyPlay, ui::DomCode::MEDIA_PLAY},
    {kSbKeyMediaStop, ui::DomCode::MEDIA_STOP},

    {kSbKeyMenu, ui::DomCode::HOME},
    {kSbKeyChannelUp, ui::DomCode::CHANNEL_UP},
    {kSbKeyChannelDown, ui::DomCode::CHANNEL_DOWN},
    {kSbKeyClosedCaption, ui::DomCode::CLOSED_CAPTION_TOGGLE},
#if SB_API_VERSION >= 15
    {kSbKeyRecord, ui::DomCode::MEDIA_RECORD},
#endif  // SB_API_VERSION >=15
});

// TODO(tholcombe): replace this with SequencedTaskRunner. See
// chromecast/starboard/chromecast/events/starboard_event_source.cc for
// implementation reference.
static scoped_refptr<base::TaskRunner> g_reply_runner_;
} // namespace

void DeliverEventHandler(std::unique_ptr<ui::Event> ui_event) {
    CHECK(ui::PlatformEventSource::GetInstance());
    static_cast<StarboardPlatformEventSource*>(ui::PlatformEventSource::GetInstance())->DeliverEvent(std::move(ui_event));
}

void StarboardPlatformEventSource::SbEventHandle(const SbEvent* event) {
  if (event->type != kSbEventTypeInput) {
    return;
  }

  if (event->data == nullptr) {
    return;
  }
  auto* input_data = static_cast<SbInputData*>(event->data);

  int64_t raw_timestamp = event->timestamp;
  SbInputEventType raw_type = input_data->type;

  std::unique_ptr<ui::Event> ui_event;

  if (input_data->device_type == kSbInputDeviceTypeKeyboard) {
    SbKey raw_key = input_data->key;
    // Ignore keyboard input events that aren't keypresses.
    if (raw_type != kSbInputEventTypePress &&
        raw_type != kSbInputEventTypeUnpress) {
      return;
    }

    // Check that the keypress is supported.
    auto it = kSbKeyToDomCodeMap.find(raw_key);
    if (it == kSbKeyToDomCodeMap.end()) {
      LOG(INFO) << "Key not supported.";
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
      int flag  = ui::EF_NONE;
      switch (input_data->type) {
        case kSbInputEventTypeMove:
          event_type = ui::EventType::ET_MOUSE_MOVED;
          break;
        case kSbInputEventTypePress:
          event_type = ui::EventType::ET_MOUSE_PRESSED;
          flag = ui::EF_LEFT_MOUSE_BUTTON;
          break;
        case kSbInputEventTypeUnpress:
          event_type = ui::EventType::ET_MOUSE_RELEASED;
          break;
        case kSbInputEventTypeWheel:
          // TODO(tholcombe): Mouse wheel events don't map to ui::MouseEvents and will cause a crash if those events are sent. Map to the correct event type.
          event_type = ui::EventType::ET_UNKNOWN;
          break;
        case kSbInputEventTypeInput:
          break;
      }

      if (event_type == ui::EventType::ET_UNKNOWN) {
        LOG(INFO) << "Ignoring unknown event type";
        return;
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
  sb_main_ = std::make_unique<std::thread>(
      &SbRunStarboardMain, /*argc=*/0, /*argv=*/nullptr,
      &SbEventHandle);
    g_reply_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    sb_main_->detach();
}

uint32_t StarboardPlatformEventSource::DeliverEvent(std::unique_ptr<ui::Event> ui_event) {
  return DispatchEvent(ui_event.get());
}

StarboardPlatformEventSource:: ~StarboardPlatformEventSource() {
}

}
