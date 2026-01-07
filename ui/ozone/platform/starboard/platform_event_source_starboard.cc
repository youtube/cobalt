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

#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_starboard.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"

namespace ui {

namespace {

std::unique_ptr<ui::Event> CreateTouchInputEvent(const SbEvent* event) {
  CHECK(event) << "CreateTouchInputEvent: missing event";
  CHECK(event->data) << "CreateTouchInputEvent: missing event data";
  SbInputData* input_data = static_cast<SbInputData*>(event->data);
  const SbInputData& data = *input_data;
  ui::EventType event_type;
  switch (data.type) {
    case kSbInputEventTypePress:
      event_type = ui::EventType::kTouchPressed;
      break;
    case kSbInputEventTypeUnpress:
      event_type = ui::EventType::kTouchReleased;
      break;
    case kSbInputEventTypeMove:
      event_type = ui::EventType::kTouchMoved;
      break;
    default:
      NOTREACHED();
  }
  float pressure = data.pressure;
  if (!std::isnan(pressure) && (event_type == ui::EventType::kTouchPressed ||
                                event_type == ui::EventType::kTouchMoved)) {
    pressure = std::max(pressure, 0.5f);
  }
  return std::make_unique<ui::TouchEvent>(
      event_type, gfx::PointF(data.position.x, data.position.y), gfx::PointF{},
      base::TimeTicks() + base::Microseconds(event->timestamp),
      ui::PointerDetails(ui::EventPointerType::kTouch, data.device_id,
                         data.size.x, data.size.y, pressure));
}

}  // namespace

void DeliverEventHandler(std::unique_ptr<ui::Event> ui_event) {
  CHECK(ui::PlatformEventSource::GetInstance());
  static_cast<PlatformEventSourceStarboard*>(
      ui::PlatformEventSource::GetInstance())
      ->DeliverEvent(std::move(ui_event));
}

void PlatformEventSourceStarboard::HandleEvent(const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  if ((input_data->device_type == kSbInputDeviceTypeKeyboard) ||
      (input_data->device_type == kSbInputDeviceTypeRemote)) {
    SbKey raw_key = input_data->key;
    if (raw_type != kSbInputEventTypePress &&
        raw_type != kSbInputEventTypeUnpress) {
      return;
    }

    int flags = 0;
    bool shift = false;
    if (input_data->key_modifiers & kSbKeyModifiersShift) {
      flags |= ui::EF_SHIFT_DOWN;
      shift = true;
    }
    if (input_data->key_modifiers & kSbKeyModifiersCtrl) {
      flags |= ui::EF_CONTROL_DOWN;
    }
    if (input_data->key_modifiers & kSbKeyModifiersAlt) {
      flags |= ui::EF_ALT_DOWN;
    }
    if (input_data->key_modifiers & kSbKeyModifiersMeta) {
      flags |= ui::EF_COMMAND_DOWN;
    }

    ui::DomCode dom_code = ui::SbKeyToDomCode(raw_key);
    ui::DomKey dom_key = ui::SbKeyToDomKey(raw_key, shift);
    ui::KeyboardCode key_code = ui::SbKeyToKeyboardCode(raw_key);

    // Key press.
    ui::EventType event_type = raw_type == kSbInputEventTypePress
                                   ? ui::EventType::kKeyPressed
                                   : ui::EventType::kKeyReleased;
    ui_event = std::make_unique<ui::KeyEvent>(
        event_type, key_code, dom_code, flags, dom_key,
        /*time_stamp=*/
        base::TimeTicks() + base::Microseconds(raw_timestamp));
  } else if (input_data->device_type == kSbInputDeviceTypeMouse) {
    ui::EventType event_type = ui::EventType::kUnknown;
    switch (input_data->type) {
      case kSbInputEventTypeMove:
        event_type = ui::EventType::kMouseMoved;
        break;
      case kSbInputEventTypePress:
        event_type = ui::EventType::kMousePressed;
        break;
      case kSbInputEventTypeUnpress:
        event_type = ui::EventType::kMouseReleased;
        break;
      case kSbInputEventTypeWheel:
        event_type = ui::EventType::kMousewheel;
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
    if (event_type == ui::EventType::kMousewheel) {
      return;
    }
    ui_event = std::make_unique<ui::MouseEvent>(
        event_type, gfx::PointF(input_data->position.x, input_data->position.y),
        gfx::PointF{}, base::TimeTicks() + base::Microseconds(raw_timestamp),
        flag, ui::EF_LEFT_MOUSE_BUTTON);
  } else if (input_data->device_type == kSbInputDeviceTypeTouchScreen ||
             input_data->device_type == kSbInputDeviceTypeTouchPad) {
    ui_event = CreateTouchInputEvent(event);
  } else {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DeliverEventHandler, std::move(ui_event)));
}

void PlatformEventSourceStarboard::HandleFocusEvent(const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event->type != kSbEventTypeFocus && event->type != kSbEventTypeBlur) {
    return;
  }
  const bool is_focused = event->type == kSbEventTypeFocus;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformEventSourceStarboard::DispatchFocusEvent,
                     weak_factory_.GetWeakPtr(), is_focused));
}

void PlatformEventSourceStarboard::DispatchFocusEvent(bool is_focused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (PlatformEventObserverStarboard& observer : sb_observers_) {
    observer.ProcessFocusEvent(is_focused);
  }
}

PlatformEventSourceStarboard::PlatformEventSourceStarboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

uint32_t PlatformEventSourceStarboard::DeliverEvent(
    std::unique_ptr<ui::Event> ui_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DispatchEvent(ui_event.get());
}

void PlatformEventSourceStarboard::AddPlatformEventObserverStarboard(
    PlatformEventObserverStarboard* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);
  sb_observers_.AddObserver(observer);
}

void PlatformEventSourceStarboard::RemovePlatformEventObserverStarboard(
    PlatformEventObserverStarboard* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sb_observers_.RemoveObserver(observer);
}

void PlatformEventSourceStarboard::DispatchWindowSizeChanged(int width,
                                                             int height) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (PlatformEventObserverStarboard& observer : sb_observers_) {
    observer.ProcessWindowSizeChangedEvent(width, height);
  }
}

void PlatformEventSourceStarboard::HandleWindowSizeChangedEvent(
    const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event->type != kSbEventTypeWindowSizeChanged) {
    return;
  }
  if (event->data == nullptr) {
    return;
  }
  auto* input_data = static_cast<SbEventWindowSizeChangedData*>(event->data);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformEventSourceStarboard::DispatchWindowSizeChanged,
                     weak_factory_.GetWeakPtr(), input_data->size.width,
                     input_data->size.height));
  return;
}

PlatformEventSourceStarboard::~PlatformEventSourceStarboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace ui
