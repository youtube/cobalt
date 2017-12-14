---
layout: doc
title: "Starboard Module Reference: input.h"
---

Defines input events and associated data types.

## Enums ##

### SbInputDeviceType ###

Identifies possible input subsystem types. The types of events that each device
type produces correspond to `SbInputEventType` values.

#### Values ####

*   `kSbInputDeviceTypeGesture`

    Input from a gesture-detection mechanism. Examples include Kinect, Wiimotes,
    etc...

    Produces `Move`, `Press` and `Unpress` events.
*   `kSbInputDeviceTypeGamepad`

    Input from a gamepad, following the layout provided in the W3C Web Gamepad
    API. [ [https://www.w3.org/TR/gamepad/](https://www.w3.org/TR/gamepad/) ]

    Produces `Move`, `Press` and `Unpress` events.
*   `kSbInputDeviceTypeKeyboard`

    Keyboard input from a traditional keyboard or game controller chatpad.

    Produces `Press` and `Unpress` events.
*   `kSbInputDeviceTypeMouse`

    Input from a traditional mouse.

    Produces `Move`, `Press`, and `Unpress` events.
*   `kSbInputDeviceTypeRemote`

    Input from a TV remote-control-style device.

    Produces `Press` and `Unpress` events.
*   `kSbInputDeviceTypeTouchScreen`

    Input from a single- or multi-touchscreen.

    Produces `Move`, `Press`, and `Unpress` events.
*   `kSbInputDeviceTypeTouchPad`

    Input from a touchpad that is not masquerading as a mouse.

    Produces `Move`, `Press`, and `Unpress` events.
*   `kSbInputDeviceTypeOnScreenKeyboard`

    Keyboard input from an on screen keyboard.

    Produces `Input` events.

### SbInputEventType ###

The action that an input event represents.

#### Values ####

*   `kSbInputEventTypeMove`

    Device Movement. In the case of `Mouse`, and perhaps `Gesture`, the movement
    tracks an absolute cursor position. In the case of `TouchPad`, only relative
    movements are provided.
*   `kSbInputEventTypePress`

    Key or button press activation. This could be a key on a keyboard, a button
    on a mouse or game controller, a push from a touch screen, or a gesture. An
    `Unpress` event is subsequently delivered when the `Press` event terminates,
    such as when the key/button/finger is raised. Injecting repeat presses is up
    to the client.
*   `kSbInputEventTypeUnpress`

    Key or button deactivation. The counterpart to the `Press` event, this event
    is sent when the key or button being pressed is released.
*   `kSbInputEventTypeWheel`

    Wheel movement. Provides relative movements of the `Mouse` wheel.
*   `kSbInputEventTypeInput`

    [https://w3c.github.io/uievents/#event-type-input](https://w3c.github.io/uievents/#event-type-input)

## Structs ##

### SbInputData ###

Event data for `kSbEventTypeInput` events.

#### Members ####

*   `SbWindow window`

    The window in which the input was generated.
*   `SbInputEventType type`

    The type of input event that this represents.
*   `SbInputDeviceType device_type`

    The type of device that generated this input event.
*   `int device_id`

    An identifier that is unique among all connected devices.
*   `SbKey key`

    An identifier that indicates which keyboard key or mouse button was involved
    in this event, if any. All known keys for all devices are mapped to a single
    ID space, defined by the `SbKey` enum in `key.h`.
*   `wchar_t character`

    The character that corresponds to the key. For an external keyboard, this
    character also depends on the keyboard language. The value is `0` if there
    is no corresponding character.
*   `SbKeyLocation key_location`

    The location of the specified key, in cases where there are multiple
    instances of the button on the keyboard. For example, some keyboards have
    more than one "shift" key.
*   `unsigned int key_modifiers`

    Key modifiers (e.g. `Ctrl`, `Shift`) held down during this input event.
*   `SbInputVector position`

    The (x, y) coordinates of the persistent cursor controlled by this device.
    The value is `0` if this data is not applicable. For events with type
    kSbInputEventTypeMove and device_type kSbInputDeviceTypeGamepad, this field
    is interpreted as a stick position with the range [-1, 1], with positive
    values for the down and right direction.
*   `SbInputVector delta`

    The relative motion vector of this input. The value is `0` if this data is
    not applicable.
*   `float pressure`

    The normalized pressure of the pointer input in the range of [0,1], where 0
    and 1 represent the minimum and maximum pressure the hardware is capable of
    detecting, respectively. Use NaN for devices that do not report pressure.
    This value is used for input events with device type mouse or touch screen.
*   `SbInputVector size`

    The (width, height) of the contact geometry of the pointer. This defines the
    size of the area under the pointer position. If (NaN, NaN) is specified, the
    value (0,0) will be used. This value is used for input events with device
    type mouse or touch screen.
*   `SbInputVector tilt`

    The (x, y) angle in degrees, in the range of [-90, 90] of the pointer,
    relative to the z axis. Positive values are for tilt to the right (x), and
    towards the user (y). Use (NaN, NaN) for devices that do not report tilt.
    This value is used for input events with device type mouse or touch screen.
*   `const char * input_text`

    The text to input for events of type `Input`.

### SbInputVector ###

A 2-dimensional vector used to represent points and motion vectors.

#### Members ####

*   `float x`
*   `float y`

