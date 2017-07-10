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

#include "cobalt/input/input_device_manager_desktop.h"

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/dom/wheel_event_init.h"
#include "cobalt/input/create_default_camera_3d.h"
#include "cobalt/input/input_poller_impl.h"
#include "cobalt/system_window/input_event.h"

namespace cobalt {
namespace input {

InputDeviceManagerDesktop::InputDeviceManagerDesktop(
    const KeyboardEventCallback& keyboard_event_callback,
    const PointerEventCallback& pointer_event_callback,
    const WheelEventCallback& wheel_event_callback,
    system_window::SystemWindow* system_window)
    : system_window_(system_window),
      input_event_callback_(
          base::Bind(&InputDeviceManagerDesktop::HandleInputEvent,
                     base::Unretained(this))),
      keypress_generator_filter_(keyboard_event_callback),
      pointer_event_callback_(pointer_event_callback),
      wheel_event_callback_(wheel_event_callback) {
  input_poller_ = new InputPollerImpl();
  DCHECK(system_window_);
  camera_3d_ =
      CreatedDefaultCamera3D(system_window->GetSbWindow(), input_poller_);

  if (system_window_) {
    // Add this object's keyboard event callback to the system window.
    system_window_->event_dispatcher()->AddEventCallback(
        system_window::InputEvent::TypeId(), input_event_callback_);
  }
}

InputDeviceManagerDesktop::~InputDeviceManagerDesktop() {
  // If we have an associated system window, remove our callback from it.
  if (system_window_) {
    system_window_->event_dispatcher()->RemoveEventCallback(
        system_window::InputEvent::TypeId(), input_event_callback_);
  }
}

namespace {

COMPILE_ASSERT(static_cast<uint32_t>(kSbKeyModifiersNone) ==
                       system_window::InputEvent::kNoModifier &&
                   static_cast<uint32_t>(kSbKeyModifiersAlt) ==
                       system_window::InputEvent::kAltKey &&
                   static_cast<uint32_t>(kSbKeyModifiersCtrl) ==
                       system_window::InputEvent::kCtrlKey &&
                   static_cast<uint32_t>(kSbKeyModifiersMeta) ==
                       system_window::InputEvent::kMetaKey &&
                   static_cast<uint32_t>(kSbKeyModifiersShift) ==
                       system_window::InputEvent::kShiftKey,
               Mismatched_modifier_enums);
#if SB_API_VERSION >= SB_POINTER_INPUT_API_VERSION
COMPILE_ASSERT(static_cast<uint32_t>(kSbKeyModifiersPointerButtonLeft) ==
                       system_window::InputEvent::kLeftButton &&
                   static_cast<uint32_t>(kSbKeyModifiersPointerButtonRight) ==
                       system_window::InputEvent::kRightButton &&
                   static_cast<uint32_t>(kSbKeyModifiersPointerButtonMiddle) ==
                       system_window::InputEvent::kMiddleButton &&
                   static_cast<uint32_t>(kSbKeyModifiersPointerButtonBack) ==
                       system_window::InputEvent::kBackButton &&
                   static_cast<uint32_t>(kSbKeyModifiersPointerButtonForward) ==
                       system_window::InputEvent::kForwardButton,
               Mismatched_modifier_enums);
#endif

void UpdateEventModifierInit(const system_window::InputEvent* input_event,
                             EventModifierInit* event) {
  const uint32 modifiers = input_event->modifiers();
  event->set_ctrl_key(modifiers & system_window::InputEvent::kCtrlKey);
  event->set_shift_key(modifiers & system_window::InputEvent::kShiftKey);
  event->set_alt_key(modifiers & system_window::InputEvent::kAltKey);
  event->set_meta_key(modifiers & system_window::InputEvent::kMetaKey);
}

void UpdateMouseEventInitButton(int key_code, MouseEventInit* event) {
  // The value of the button attribute MUST be as follows:
  //  https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-button-1
  if (key_code == kSbKeyMouse1) {
    // 0 MUST indicate the primary button of the device (in general, the left
    // button or the only button on single-button devices, used to activate a
    // user interface control or select text) or the un-initialized value.
    event->set_button(0);
  } else if (key_code == kSbKeyMouse2) {
    // 1 MUST indicate the auxiliary button (in general, the middle button,
    // often combined with a mouse wheel).
    event->set_button(1);
  } else if (key_code == kSbKeyMouse3) {
    // 2 MUST indicate the secondary button (in general, the right button, often
    // used to display a context menu).
    event->set_button(2);
  }
}

void UpdateMouseEventInitButtons(uint32 modifiers, MouseEventInit* event) {
  // The value of the buttons attribute MUST be as follows:
  //  https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-buttons-3

  // 0 MUST indicate no button is currently active.
  uint16 buttons = 0;
  if (modifiers & system_window::InputEvent::kLeftButton) {
    // 1 MUST indicate the primary button of the device (in general, the left
    // button or the only button on single-button devices, used to activate a
    // user interface control or select text).
    buttons |= 1;
  }
  if (modifiers & system_window::InputEvent::kRightButton) {
    // 2 MUST indicate the secondary button (in general, the right button, often
    // used to display a context menu), if present.
    buttons |= 2;
  }
  if (modifiers & system_window::InputEvent::kMiddleButton) {
    // 4 MUST indicate the auxiliary button (in general, the middle button,
    // often combined with a mouse wheel).
    buttons |= 4;
  }
  event->set_buttons(buttons);
}

void UpdateMouseEventInit(const system_window::InputEvent* input_event,
                          dom::MouseEventInit* mouse_event) {
  UpdateEventModifierInit(input_event, mouse_event);
  UpdateMouseEventInitButton(input_event->key_code(), mouse_event);
  UpdateMouseEventInitButtons(input_event->modifiers(), mouse_event);

  mouse_event->set_screen_x(input_event->position().x());
  mouse_event->set_screen_y(input_event->position().y());
  mouse_event->set_client_x(input_event->position().x());
  mouse_event->set_client_y(input_event->position().y());
}

}  // namespace

void InputDeviceManagerDesktop::HandleKeyboardEvent(
    bool is_key_down, const system_window::InputEvent* input_event,
    int key_code) {
  base::Token type =
      is_key_down ? base::Tokens::keydown() : base::Tokens::keyup();
  dom::KeyboardEvent::KeyLocationCode location =
      dom::KeyboardEvent::KeyCodeToKeyLocation(key_code);
  dom::KeyboardEventInit keyboard_event;
  UpdateEventModifierInit(input_event, &keyboard_event);
  keyboard_event.set_location(location);
  keyboard_event.set_repeat(input_event->is_repeat());
  keyboard_event.set_char_code(key_code);
  keyboard_event.set_key_code(key_code);
  keypress_generator_filter_.HandleKeyboardEvent(type, keyboard_event);
}

void InputDeviceManagerDesktop::HandlePointerEvent(
    base::Token type, const system_window::InputEvent* input_event) {
  dom::PointerEventInit pointer_event;
  UpdateMouseEventInit(input_event, &pointer_event);

  pointer_event.set_pointer_id(input_event->device_id());
#if SB_API_VERSION >= SB_POINTER_INPUT_API_VERSION
  pointer_event.set_width(input_event->size().x());
  pointer_event.set_height(input_event->size().y());
  pointer_event.set_pressure(input_event->pressure());
  pointer_event.set_tilt_x(input_event->tilt().x());
  pointer_event.set_tilt_y(input_event->tilt().y());
#endif
  pointer_event.set_is_primary(true);
  pointer_event_callback_.Run(type, pointer_event);
}

void InputDeviceManagerDesktop::HandleWheelEvent(
    const system_window::InputEvent* input_event) {
  base::Token type = base::Tokens::wheel();
  dom::WheelEventInit wheel_event;
  UpdateMouseEventInit(input_event, &wheel_event);

  wheel_event.set_delta_x(input_event->delta().x());
  wheel_event.set_delta_y(input_event->delta().y());
  wheel_event.set_delta_z(0);
  wheel_event.set_delta_mode(dom::WheelEvent::kDomDeltaLine);

  wheel_event_callback_.Run(type, wheel_event);
}

void InputDeviceManagerDesktop::HandleInputEvent(const base::Event* event) {
  // The user has pressed a key on the keyboard.
  const system_window::InputEvent* input_event =
      base::polymorphic_downcast<const system_window::InputEvent*>(event);
  SB_DCHECK(input_event);
  int key_code = input_event->key_code();

  switch (input_event->type()) {
    case system_window::InputEvent::kKeyDown:
      HandleKeyboardEvent(true, input_event, key_code);
      break;
    case system_window::InputEvent::kKeyUp:
      HandleKeyboardEvent(false, input_event, key_code);
      break;
    case system_window::InputEvent::kPointerDown:
    case system_window::InputEvent::kPointerUp: {
      if ((kSbKeyBrowserBack == key_code) ||
          (kSbKeyBrowserForward == key_code) || (kSbKeyMouse2 == key_code)) {
        // For consistency with behavior on current browsers, the 'Forward' and
        // 'Back' mouse buttons are reported as keypress input for the 'Forward'
        // and 'Back' navigation keys, not as Pointer Events for the X1 and X2
        // buttons.

        if (kSbKeyMouse2 == key_code) {
          // Temporarily Report middle button presses as the enter key.
          key_code = kSbKeyReturn;
        }
        HandleKeyboardEvent(
            input_event->type() == system_window::InputEvent::kPointerDown,
            input_event, key_code);
      } else {
        base::Token type =
            input_event->type() == system_window::InputEvent::kPointerDown
                ? base::Tokens::pointerdown()
                : base::Tokens::pointerup();
        HandlePointerEvent(type, input_event);
      }
      break;
    }
    case system_window::InputEvent::kPointerMove: {
      HandlePointerEvent(base::Tokens::pointermove(), input_event);
      break;
    }
    case system_window::InputEvent::kWheel: {
      HandleWheelEvent(input_event);
    }
    default:
      break;
  }

  InputPollerImpl* input_poller_impl =
      static_cast<InputPollerImpl*>(input_poller_.get());
  input_poller_impl->UpdateInputEvent(input_event);
}

}  // namespace input
}  // namespace cobalt
