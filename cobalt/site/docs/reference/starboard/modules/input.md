---
layout: doc
title: "Starboard Module Reference: input.h"
---

Defines input events and associated data types.

## Enums

### SbInputDeviceType

Identifies possible input subsystem types. The types of events that each
device type produces correspond to `SbInputEventType` values.

**Values**

*   `kSbInputDeviceTypeGesture` - Input from a gesture-detection mechanism. Examples include Kinect,Wiimotes, etc...<br>Produces |Move|, |Press| and |Unpress| events.
*   `kSbInputDeviceTypeGamepad` - Input from a gamepad, following the layout provided in the W3C Web GamepadAPI. [https://www.w3.org/TR/gamepad/]<br>Produces |Move|, |Press| and |Unpress| events.
*   `kSbInputDeviceTypeKeyboard` - Keyboard input from a traditional keyboard or game controller chatpad.<br>Produces |Press| and |Unpress| events.
*   `kSbInputDeviceTypeMicrophone` - Input from a microphone that would provide audio data to the caller, whomay then find some way to detect speech or other sounds within it. It mayhave processed or filtered the audio in some way before it arrives.<br>Produces |Audio| events.
*   `kSbInputDeviceTypeMouse` - Input from a traditional mouse.<br>Produces |Move|, |Press|, and |Unpress| events.
*   `kSbInputDeviceTypeRemote` - Input from a TV remote-control-style device.<br>Produces |Press| and |Unpress| events.
*   `kSbInputDeviceTypeSpeechCommand` - Input from a speech command analyzer, which is some hardware or softwarethat, given a set of known phrases, activates when one of the registeredphrases is heard.<br>Produces |Command| events.
*   `kSbInputDeviceTypeTouchScreen` - Input from a single- or multi-touchscreen.<br>Produces |Move|, |Press|, and |Unpress| events.
*   `kSbInputDeviceTypeTouchPad` - Input from a touchpad that is not masquerading as a mouse.<br>Produces |Move|, |Press|, and |Unpress| events.
*   `kSbInputDeviceTypeOnScreenKeyboard` - Keyboard input from an on screen keyboard.<br>Produces |Input| events.

### SbInputEventType

The action that an input event represents.

**Values**

*   `kSbInputEventTypeAudio` - Receipt of Audio. Some audio data was received by the input microphone.
*   `kSbInputEventTypeCommand` - Receipt of a command. A command was received from some semantic source,like a speech recognizer.
*   `kSbInputEventTypeGrab` - Grab activation. This event type is deprecated.
*   `kSbInputEventTypeMove` - Device Movement. In the case of |Mouse|, and perhaps |Gesture|, themovement tracks an absolute cursor position. In the case of |TouchPad|,only relative movements are provided.
*   `kSbInputEventTypePress` - Key or button press activation. This could be a key on a keyboard, a buttonon a mouse or game controller, a push from a touch screen, or a gesture. An|Unpress| event is subsequently delivered when the |Press| eventterminates, such as when the key/button/finger is raised. Injecting repeatpresses is up to the client.
*   `kSbInputEventTypeUngrab` - Grab deactivation. This event type is deprecated.
*   `kSbInputEventTypeUnpress` - Key or button deactivation. The counterpart to the |Press| event, thisevent is sent when the key or button being pressed is released.
*   `kSbInputEventTypeWheel` - Wheel movement. Provides relative movements of the |Mouse| wheel.
*   `kSbInputEventTypeInput` - https://w3c.github.io/uievents/#event-type-input

## Structs

### SbInputData

Event data for `kSbEventTypeInput` events.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbWindow</code><br>        <code>window</code></td>    <td>The window in which the input was generated.</td>  </tr>
  <tr>
    <td><code>SbInputEventType</code><br>        <code>type</code></td>    <td>The type of input event that this represents.</td>  </tr>
  <tr>
    <td><code>SbInputDeviceType</code><br>        <code>device_type</code></td>    <td>The type of device that generated this input event.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>device_id</code></td>    <td>An identifier that is unique among all connected devices.</td>  </tr>
  <tr>
    <td><code>SbKey</code><br>        <code>key</code></td>    <td>An identifier that indicates which keyboard key or mouse button was
involved in this event, if any. All known keys for all devices are mapped
to a single ID space, defined by the <code>SbKey</code> enum in <code>key.h</code>.</td>  </tr>
  <tr>
    <td><code>wchar_t</code><br>        <code>character</code></td>    <td>The character that corresponds to the key. For an external keyboard, this
character also depends on the keyboard language. The value is <code>0</code> if there
is no corresponding character.</td>  </tr>
  <tr>
    <td><code>SbKeyLocation</code><br>        <code>key_location</code></td>    <td>The location of the specified key, in cases where there are multiple
instances of the button on the keyboard. For example, some keyboards have
more than one "shift" key.</td>  </tr>
  <tr>
    <td><code>unsigned</code><br>        <code>int key_modifiers</code></td>    <td>Key modifiers (e.g. <code>Ctrl</code>, <code>Shift</code>) held down during this input event.</td>  </tr>
  <tr>
    <td><code>SbInputVector</code><br>        <code>position</code></td>    <td>The (x, y) coordinates of the persistent cursor controlled by this device.
The value is <code>0</code> if this data is not applicable. For events with type
kSbInputEventTypeMove and device_type kSbInputDeviceTypeGamepad, this field
is interpreted as a stick position with the range [-1, 1], with positive
values for the down and right direction.</td>  </tr>
  <tr>
    <td><code>SbInputVector</code><br>        <code>delta</code></td>    <td>The relative motion vector of this input. The value is <code>0</code> if this data is
not applicable.</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>pressure</code></td>    <td>The normalized pressure of the pointer input in the range of [0,1], where 0
and 1 represent the minimum and maximum pressure the hardware is capable of
detecting, respectively. Use NaN for devices that do not report pressure.
This value is used for input events with device type mouse or touch screen.</td>  </tr>
  <tr>
    <td><code>SbInputVector</code><br>        <code>size</code></td>    <td>The (width, height) of the contact geometry of the pointer. This defines
the size of the area under the pointer position. If (NaN, NaN) is
specified, the value (0,0) will be used. This value is used for input
events with device type mouse or touch screen.</td>  </tr>
  <tr>
    <td><code>SbInputVector</code><br>        <code>tilt</code></td>    <td>The (x, y) angle in degrees, in the range of [-90, 90] of the pointer,
relative to the z axis. Positive values are for tilt to the right (x), and
towards the user (y). Use (NaN, NaN) for devices that do not report tilt.
This value is used for input events with device type mouse or touch screen.</td>  </tr>
  <tr>
    <td><code>const</code><br>        <code>char* input_text</code></td>    <td>The text to input for events of type <code>Input</code>.</td>  </tr>
</table>

### SbInputVector

A 2-dimensional vector used to represent points and motion vectors.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>float</code><br>        <code>x</code></td>    <td></td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>y</code></td>    <td></td>  </tr>
</table>

