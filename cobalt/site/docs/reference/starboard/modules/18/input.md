Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `input.h`

Defines input events and associated data types.

## Enums

### SbInputDeviceType

Identifies possible input device types. The events produced by each device type
correspond to `SbInputEventType` values.

#### Values

*   `kSbInputDeviceTypeGesture`

    Input from a gesture-detection mechanism. Examples include Kinect and
    Wiimotes.

    Produces `Move`, `Press` and `Unpress` events.
*   `kSbInputDeviceTypeGamepad`

    Input from a gamepad, following the layout provided in the [W3C Web Gamepad API](https://www.w3.org/TR/gamepad/).

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

    Keyboard input from an on-screen keyboard.

    Produces `Input` events.

### SbInputEventType

The action that an input event represents.

#### Values

*   `kSbInputEventTypeMove`

    Device movement. For `Mouse` and `Gesture` devices, movement tracks an
    absolute cursor position. For `TouchPad` devices, only relative movement is
    provided.
*   `kSbInputEventTypePress`

    Key or button press. This can be a keyboard key, mouse or game controller
    button, touchscreen press, or gesture. An `Unpress` event is dispatched when
    the `Press` event terminates (for example, when releasing the key or raising
    the finger). The client is responsible for generating repeat press events.
*   `kSbInputEventTypeUnpress`

    Key or button release. The counterpart to `Press`, this event is sent when
    the pressed key or button is released.
*   `kSbInputEventTypeWheel`

    Wheel movement. Provides relative movement of the `Mouse` wheel.
*   `kSbInputEventTypeInput`

    [W3C Event Type Input](https://w3c.github.io/uievents/#event-type-input)

## Structs

### SbInputData

Event data for `kSbEventTypeInput` events.

#### Members

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

    The character that corresponds to the key. For external keyboards, this
    character depends on the keyboard layout. The value is `0` if there is no
    corresponding character.
*   `SbKeyLocation key_location`

    The location of the specified key, when multiple instances of a key exist
    (for example, left and right "Shift" keys).
*   `unsigned int key_modifiers`

    Key modifiers (e.g. `Ctrl`, `Shift`) held down during this input event.
*   `SbInputVector position`

    The (x, y) coordinates of the persistent cursor controlled by this device.
    The value is `0` if this data is not applicable. For `kSbInputEventTypeMove`
    events from `kSbInputDeviceTypeGamepad` devices, this field represents stick
    position in the range `[-1, 1]`, where positive values indicate down and
    right directions.
*   `SbInputVector delta`

    The relative motion vector of this input. The value is `0` if this data is
    not applicable.
*   `float pressure`

    The normalized pointer pressure in the range `[0, 1]`, where `0` and `1`
    represent minimum and maximum detectable pressure. Use `NaN` if the device
    does not report pressure. This value applies to mouse and touchscreen input
    events.
*   `SbInputVector size`

    The contact geometry size `(width, height)` of the pointer, defining the
    contact area. If `(NaN, NaN)` is specified, `(0, 0)` is used. This value
    applies to mouse and touchscreen input events.
*   `SbInputVector tilt`

    The tilt angle `(x, y)` in degrees, in the range `[-90, 90]` relative to the
    z-axis. Positive values indicate tilt to the right (x) and towards the user
    (y). Use `(NaN, NaN)` if the device does not report tilt. This value applies
    to mouse and touchscreen input events.
*   `const char* input_text`

    The text to input for events of type `Input`.
*   `bool is_composing`

    Set to `true` if the input event is part of an ongoing composition.

### SbInputVector

A 2-dimensional vector used to represent points and motion vectors.

#### Members

*   `float x`
*   `float y`
