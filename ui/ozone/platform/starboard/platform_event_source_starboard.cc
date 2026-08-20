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

#include <cmath>

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

struct ModifierMapping {
  unsigned int sb_modifier;
  ui::EventFlags ui_flag;
};

constexpr ModifierMapping kModifierMappings[] = {
    {kSbKeyModifiersShift, ui::EF_SHIFT_DOWN},
    {kSbKeyModifiersCtrl, ui::EF_CONTROL_DOWN},
    {kSbKeyModifiersAlt, ui::EF_ALT_DOWN},
    {kSbKeyModifiersMeta, ui::EF_COMMAND_DOWN},
    {kSbKeyModifiersPointerButtonLeft, ui::EF_LEFT_MOUSE_BUTTON},
    {kSbKeyModifiersPointerButtonRight, ui::EF_RIGHT_MOUSE_BUTTON},
    {kSbKeyModifiersPointerButtonMiddle, ui::EF_MIDDLE_MOUSE_BUTTON},
    {kSbKeyModifiersPointerButtonBack, ui::EF_BACK_MOUSE_BUTTON},
    {kSbKeyModifiersPointerButtonForward, ui::EF_FORWARD_MOUSE_BUTTON},
};

int GetEventFlags(const SbInputData& input_data) {
  int flags = ui::EF_NONE;
  for (const auto& mapping : kModifierMappings) {
    if (input_data.key_modifiers & mapping.sb_modifier) {
      flags |= mapping.ui_flag;
    }
  }
  return flags;
}

int GetMouseButtonFromKey(SbKey key) {
  switch (key) {
    case kSbKeyMouse1:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case kSbKeyMouse2:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    case kSbKeyMouse3:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case kSbKeyMouse4:
    case kSbKeyBrowserBack:
      return ui::EF_BACK_MOUSE_BUTTON;
    case kSbKeyMouse5:
    case kSbKeyBrowserForward:
      return ui::EF_FORWARD_MOUSE_BUTTON;
    default:
      return ui::EF_LEFT_MOUSE_BUTTON;
  }
}

}  // namespace

std::unique_ptr<ui::Event>
PlatformEventSourceStarboard::CreateKeyboardRemoteInputEvent(
    const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event || !event->data) {
    return nullptr;
  }
  const auto* input_data = static_cast<const SbInputData*>(event->data);
  SbInputEventType raw_type = input_data->type;
  if (raw_type != kSbInputEventTypePress &&
      raw_type != kSbInputEventTypeUnpress) {
    return nullptr;
  }

  SbKey raw_key = input_data->key;
  int flags = GetEventFlags(*input_data);
  bool shift = (input_data->key_modifiers & kSbKeyModifiersShift) != 0;

  ui::DomCode dom_code = ui::SbKeyToDomCode(raw_key);
  ui::DomKey dom_key = ui::SbKeyToDomKey(raw_key, shift);
  ui::KeyboardCode key_code = ui::SbKeyToKeyboardCode(raw_key);

  ui::EventType event_type = raw_type == kSbInputEventTypePress
                                 ? ui::EventType::kKeyPressed
                                 : ui::EventType::kKeyReleased;
  return std::make_unique<ui::KeyEvent>(
      event_type, key_code, dom_code, flags, dom_key,
      base::TimeTicks() + base::Microseconds(event->timestamp));
}

std::unique_ptr<ui::Event>
PlatformEventSourceStarboard::CreateMouseWheelInputEvent(const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event || !event->data) {
    return nullptr;
  }
  const auto* input_data = static_cast<const SbInputData*>(event->data);
  int flags = GetEventFlags(*input_data);
  // In Chromium ui::MouseWheelEvent, offset.x > 0 is left, offset.y > 0 is up.
  // Starboard sends delta.x > 0 for right, delta.y > 0 for down.
  // Hence offset = -delta * kWheelDelta.
  gfx::Vector2d offset(
      static_cast<int>(
          std::round(-input_data->delta.x * ui::MouseWheelEvent::kWheelDelta)),
      static_cast<int>(
          std::round(-input_data->delta.y * ui::MouseWheelEvent::kWheelDelta)));
  return std::make_unique<ui::MouseWheelEvent>(
      offset, gfx::PointF(input_data->position.x, input_data->position.y),
      gfx::PointF(input_data->position.x, input_data->position.y),
      base::TimeTicks() + base::Microseconds(event->timestamp), flags,
      /*changed_button_flags=*/0);
}

std::unique_ptr<ui::Event> PlatformEventSourceStarboard::CreateMouseInputEvent(
    const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event || !event->data) {
    return nullptr;
  }
  const auto* input_data = static_cast<const SbInputData*>(event->data);

  if (input_data->type == kSbInputEventTypeWheel) {
    return CreateMouseWheelInputEvent(event);
  }

  int flags = GetEventFlags(*input_data);
  int changed_button_flags = 0;
  ui::EventType event_type = ui::EventType::kUnknown;

  switch (input_data->type) {
    case kSbInputEventTypeMove:
      event_type = ui::EventType::kMouseMoved;
      flags |= current_pressed_mouse_buttons_;
      break;
    case kSbInputEventTypePress:
      event_type = ui::EventType::kMousePressed;
      changed_button_flags = GetMouseButtonFromKey(input_data->key);
      current_pressed_mouse_buttons_ |= changed_button_flags;
      flags |= current_pressed_mouse_buttons_;
      break;
    case kSbInputEventTypeUnpress:
      event_type = ui::EventType::kMouseReleased;
      if (input_data->key != kSbKeyUnknown) {
        changed_button_flags = GetMouseButtonFromKey(input_data->key);
      } else if (current_pressed_mouse_buttons_ != 0) {
        // Fallback: if key is omitted on release, release the currently held
        // button.
        changed_button_flags = current_pressed_mouse_buttons_;
      } else {
        changed_button_flags = ui::EF_LEFT_MOUSE_BUTTON;
      }
      current_pressed_mouse_buttons_ &= ~changed_button_flags;
      flags |= current_pressed_mouse_buttons_;
      flags &= ~changed_button_flags;
      break;
    default:
      return nullptr;
  }

  return std::make_unique<ui::MouseEvent>(
      event_type, gfx::PointF(input_data->position.x, input_data->position.y),
      gfx::PointF{}, base::TimeTicks() + base::Microseconds(event->timestamp),
      flags, changed_button_flags);
}

std::unique_ptr<ui::Event> PlatformEventSourceStarboard::CreateTouchInputEvent(
    const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!event || !event->data) {
    return nullptr;
  }
  const auto* input_data = static_cast<const SbInputData*>(event->data);
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
      return nullptr;
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

namespace {

void DeliverEventHandler(std::unique_ptr<ui::Event> ui_event) {
  static_cast<PlatformEventSourceStarboard*>(
      ui::PlatformEventSource::GetInstance())
      ->DeliverEvent(std::move(ui_event));
}

}  // namespace

void PlatformEventSourceStarboard::HandleEvent(const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(event) << __func__ << ": missing event";
  if (event->type != kSbEventTypeInput) {
    return;
  }
  CHECK(event->data) << __func__ << ": missing event data";
  const auto* input_data = static_cast<const SbInputData*>(event->data);
  std::unique_ptr<ui::Event> ui_event;

  switch (input_data->device_type) {
    case kSbInputDeviceTypeKeyboard:
    case kSbInputDeviceTypeRemote:
      ui_event = CreateKeyboardRemoteInputEvent(event);
      break;
    case kSbInputDeviceTypeMouse:
      ui_event = CreateMouseInputEvent(event);
      break;
    case kSbInputDeviceTypeTouchScreen:
    case kSbInputDeviceTypeTouchPad:
      ui_event = CreateTouchInputEvent(event);
      break;
    default:
      return;
  }

  if (!ui_event) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DeliverEventHandler, std::move(ui_event)));
}

void PlatformEventSourceStarboard::HandleFocusEvent(const SbEvent* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(event) << __func__ << ": missing event";
  if (event->type != kSbEventTypeFocus && event->type != kSbEventTypeBlur) {
    return;
  }
  if (event->type == kSbEventTypeBlur) {
    current_pressed_mouse_buttons_ = 0;
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
