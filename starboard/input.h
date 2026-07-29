// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Input module
//
// Defines input events and associated data types.

#ifndef STARBOARD_INPUT_H_
#define STARBOARD_INPUT_H_

#include <stdbool.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/key.h"
#include "starboard/window.h"

#ifdef __cplusplus
extern "C" {
#endif

// Identifies possible input device types. The events produced by each device
// type correspond to |SbInputEventType| values.
typedef enum SbInputDeviceType {
  // Input from a gesture-detection mechanism. Examples include Kinect and
  // Wiimotes.
  //
  // Produces `Move`, `Press` and `Unpress` events.
  kSbInputDeviceTypeGesture,

  // Input from a gamepad, following the layout provided in the
  // [W3C Web Gamepad API](https://www.w3.org/TR/gamepad/).
  //
  // Produces `Move`, `Press` and `Unpress` events.
  kSbInputDeviceTypeGamepad,

  // Keyboard input from a traditional keyboard or game controller chatpad.
  //
  // Produces `Press` and `Unpress` events.
  kSbInputDeviceTypeKeyboard,

  // Input from a traditional mouse.
  //
  // Produces `Move`, `Press`, and `Unpress` events.
  kSbInputDeviceTypeMouse,

  // Input from a TV remote-control-style device.
  //
  // Produces `Press` and `Unpress` events.
  kSbInputDeviceTypeRemote,

  // Input from a single- or multi-touchscreen.
  //
  // Produces `Move`, `Press`, and `Unpress` events.
  kSbInputDeviceTypeTouchScreen,

  // Input from a touchpad that is not masquerading as a mouse.
  //
  // Produces `Move`, `Press`, and `Unpress` events.
  kSbInputDeviceTypeTouchPad,

  // Keyboard input from an on-screen keyboard.
  //
  // Produces `Input` events.
  kSbInputDeviceTypeOnScreenKeyboard,
} SbInputDeviceType;

// The action that an input event represents.
typedef enum SbInputEventType {
  // Device movement. For `Mouse` and `Gesture` devices, movement tracks an
  // absolute cursor position. For `TouchPad` devices, only relative movement is
  // provided.
  kSbInputEventTypeMove,

  // Key or button press. This can be a keyboard key, mouse or game controller
  // button, touchscreen press, or gesture. An `Unpress` event is dispatched
  // when the `Press` event terminates (for example, when releasing the key or
  // raising the finger). The client is responsible for generating repeat press
  // events.
  kSbInputEventTypePress,

  // Key or button release. The counterpart to `Press`, this event is sent when
  // the pressed key or button is released.
  kSbInputEventTypeUnpress,

  // Wheel movement. Provides relative movement of the `Mouse` wheel.
  kSbInputEventTypeWheel,

  //
  // [W3C Event Type Input](https://w3c.github.io/uievents/#event-type-input)
  kSbInputEventTypeInput,
} SbInputEventType;

// A 2-dimensional vector used to represent points and motion vectors.
typedef struct SbInputVector {
  float x;
  float y;
} SbInputVector;

// Event data for `kSbEventTypeInput` events.
typedef struct SbInputData {
  // The window in which the input was generated.
  SbWindow window;

  // The type of input event that this represents.
  SbInputEventType type;

  // The type of device that generated this input event.
  SbInputDeviceType device_type;

  // An identifier that is unique among all connected devices.
  int device_id;

  // An identifier that indicates which keyboard key or mouse button was
  // involved in this event, if any. All known keys for all devices are mapped
  // to a single ID space, defined by the `SbKey` enum in `key.h`.
  SbKey key;

  // The character that corresponds to the key. For external keyboards, this
  // character depends on the keyboard layout. The value is `0` if there is no
  // corresponding character.
  wchar_t character;

  // The location of the specified key, when multiple instances of a key exist
  // (for example, left and right "Shift" keys).
  SbKeyLocation key_location;

  // Key modifiers (e.g. `Ctrl`, `Shift`) held down during this input event.
  unsigned int key_modifiers;

  // The (x, y) coordinates of the persistent cursor controlled by this device.
  // The value is `0` if this data is not applicable. For
  // |kSbInputEventTypeMove| events from |kSbInputDeviceTypeGamepad| devices,
  // this field represents stick position in the range `[-1, 1]`, where positive
  // values indicate down and right directions.
  SbInputVector position;

  // The relative motion vector of this input. The value is `0` if this data is
  // not applicable.
  SbInputVector delta;

  // The normalized pointer pressure in the range `[0, 1]`, where `0` and `1`
  // represent minimum and maximum detectable pressure. Use `NaN` if the device
  // does not report pressure. This value applies to mouse and touchscreen input
  // events.
  float pressure;

  // The contact geometry size `(width, height)` of the pointer, defining the
  // contact area. If `(NaN, NaN)` is specified, `(0, 0)` is used. This value
  // applies to mouse and touchscreen input events.
  SbInputVector size;

  // The tilt angle `(x, y)` in degrees, in the range `[-90, 90]` relative to
  // the z-axis. Positive values indicate tilt to the right (x) and towards the
  // user (y). Use `(NaN, NaN)` if the device does not report tilt. This value
  // applies to mouse and touchscreen input events.
  SbInputVector tilt;

  // The text to input for events of type `Input`.
  const char* input_text;

  // Set to `true` if the input event is part of an ongoing composition.
  bool is_composing;
} SbInputData;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_INPUT_H_
