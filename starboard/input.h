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

// Module Overview: Starboard Input module
//
// Defines input events and associated data types.

#ifndef STARBOARD_INPUT_H_
#define STARBOARD_INPUT_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/key.h"
#include "starboard/types.h"
#include "starboard/window.h"

#ifdef __cplusplus
extern "C" {
#endif

// Identifies possible input subsystem types. The types of events that each
// device type produces correspond to |SbInputEventType| values.
typedef enum SbInputDeviceType {
  // Input from a gesture-detection mechanism. Examples include Kinect,
  // Wiimotes, etc...
  //
  // Produces |Move|, |Press| and |Unpress| events.
  kSbInputDeviceTypeGesture,

  // Input from a gamepad, following the layout provided in the W3C Web Gamepad
  // API. [https://www.w3.org/TR/gamepad/]
  //
  // Produces |Move|, |Press| and |Unpress| events.
  kSbInputDeviceTypeGamepad,

  // Keyboard input from a traditional keyboard or game controller chatpad.
  //
  // Produces |Press| and |Unpress| events.
  kSbInputDeviceTypeKeyboard,

#if SB_API_VERSION < 5
  // Input from a microphone that would provide audio data to the caller, who
  // may then find some way to detect speech or other sounds within it. It may
  // have processed or filtered the audio in some way before it arrives.
  //
  // Produces |Audio| events.
  kSbInputDeviceTypeMicrophone,
#endif

  // Input from a traditional mouse.
  //
  // Produces |Move|, |Press|, and |Unpress| events.
  kSbInputDeviceTypeMouse,

  // Input from a TV remote-control-style device.
  //
  // Produces |Press| and |Unpress| events.
  kSbInputDeviceTypeRemote,

#if SB_API_VERSION < 5
  // Input from a speech command analyzer, which is some hardware or software
  // that, given a set of known phrases, activates when one of the registered
  // phrases is heard.
  //
  // Produces |Command| events.
  kSbInputDeviceTypeSpeechCommand,
#endif

  // Input from a single- or multi-touchscreen.
  //
  // Produces |Move|, |Press|, and |Unpress| events.
  kSbInputDeviceTypeTouchScreen,

  // Input from a touchpad that is not masquerading as a mouse.
  //
  // Produces |Move|, |Press|, and |Unpress| events.
  kSbInputDeviceTypeTouchPad,

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // Keyboard input from an on screen keyboard.
  //
  // Produces |Input| events.
  kSbInputDeviceTypeOnScreenKeyboard,
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
} SbInputDeviceType;

// The action that an input event represents.
typedef enum SbInputEventType {
#if SB_API_VERSION < 5
  // Receipt of Audio. Some audio data was received by the input microphone.
  kSbInputEventTypeAudio,
  // Receipt of a command. A command was received from some semantic source,
  // like a speech recognizer.
  kSbInputEventTypeCommand,
  // Grab activation. This event type is deprecated.
  kSbInputEventTypeGrab,
#endif

  // Device Movement. In the case of |Mouse|, and perhaps |Gesture|, the
  // movement tracks an absolute cursor position. In the case of |TouchPad|,
  // only relative movements are provided.
  kSbInputEventTypeMove,

  // Key or button press activation. This could be a key on a keyboard, a button
  // on a mouse or game controller, a push from a touch screen, or a gesture. An
  // |Unpress| event is subsequently delivered when the |Press| event
  // terminates, such as when the key/button/finger is raised. Injecting repeat
  // presses is up to the client.
  kSbInputEventTypePress,

#if SB_API_VERSION < 5
  // Grab deactivation. This event type is deprecated.
  kSbInputEventTypeUngrab,
#endif

  // Key or button deactivation. The counterpart to the |Press| event, this
  // event is sent when the key or button being pressed is released.
  kSbInputEventTypeUnpress,

#if SB_API_VERSION >= 6
  // Wheel movement. Provides relative movements of the |Mouse| wheel.
  kSbInputEventTypeWheel,
#endif

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // https://w3c.github.io/uievents/#event-type-input
  kSbInputEventTypeInput,
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
} SbInputEventType;

// A 2-dimensional vector used to represent points and motion vectors.
typedef struct SbInputVector {
  float x;
  float y;
} SbInputVector;

// Event data for |kSbEventTypeInput| events.
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
  // to a single ID space, defined by the |SbKey| enum in |key.h|.
  SbKey key;

  // The character that corresponds to the key. For an external keyboard, this
  // character also depends on the keyboard language. The value is |0| if there
  // is no corresponding character.
  wchar_t character;

  // The location of the specified key, in cases where there are multiple
  // instances of the button on the keyboard. For example, some keyboards have
  // more than one "shift" key.
  SbKeyLocation key_location;

  // Key modifiers (e.g. |Ctrl|, |Shift|) held down during this input event.
  unsigned int key_modifiers;

  // The (x, y) coordinates of the persistent cursor controlled by this device.
  // The value is |0| if this data is not applicable. For events with type
  // kSbInputEventTypeMove and device_type kSbInputDeviceTypeGamepad, this field
  // is interpreted as a stick position with the range [-1, 1], with positive
  // values for the down and right direction.
  SbInputVector position;

  // The relative motion vector of this input. The value is |0| if this data is
  // not applicable.
  SbInputVector delta;

#if SB_API_VERSION >= 6
  // The normalized pressure of the pointer input in the range of [0,1], where 0
  // and 1 represent the minimum and maximum pressure the hardware is capable of
  // detecting, respectively. Use NaN for devices that do not report pressure.
  // This value is used for input events with device type mouse or touch screen.
  float pressure;

  // The (width, height) of the contact geometry of the pointer. This defines
  // the size of the area under the pointer position. If (NaN, NaN) is
  // specified, the value (0,0) will be used. This value is used for input
  // events with device type mouse or touch screen.
  SbInputVector size;

  // The (x, y) angle in degrees, in the range of [-90, 90] of the pointer,
  // relative to the z axis. Positive values are for tilt to the right (x), and
  // towards the user (y). Use (NaN, NaN) for devices that do not report tilt.
  // This value is used for input events with device type mouse or touch screen.
  SbInputVector tilt;
#endif
#if SB_HAS(ON_SCREEN_KEYBOARD)
  // The text to input for events of type |Input|.
  const char* input_text;
#endif
} SbInputData;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_INPUT_H_
