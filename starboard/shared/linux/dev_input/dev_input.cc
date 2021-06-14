// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/linux/dev_input/dev_input.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/time_internal.h"

namespace starboard {
namespace shared {
namespace dev_input {
namespace {

using ::starboard::shared::starboard::Application;

typedef int FileDescriptor;
const FileDescriptor kInvalidFd = -ENODEV;

enum InputDeviceIds {
  kKeyboardDeviceId = 1,
  kGamepadDeviceId,
};

enum TouchPadPositionState {
  kTouchPadPositionNone = 0,
  kTouchPadPositionX = 1,
  kTouchPadPositionY = 2,
  kTouchPadPositionAll = kTouchPadPositionX | kTouchPadPositionY
};

class ControllerTuning {
 public:
  ~ControllerTuning() {}

  uint16_t GetKeyCode(uint16_t from);
  void MapKeyCode(uint16_t from, uint16_t to) { key_map_[from] = to; }

  float GetMinimumFlat() { return minimum_flat_; }
  void SetMinimumFlat(float value) { minimum_flat_ = value; }

 protected:
  // Only create this object from DeviceMap::CreateControllerTuning()
  ControllerTuning() {}

 private:
  std::unordered_map<int, int> key_map_;
  float minimum_flat_ = 0;

  friend class DeviceMap;
};

uint16_t ControllerTuning::GetKeyCode(uint16_t from) {
  auto mapped_code = key_map_.find(from);
  if (mapped_code != key_map_.end()) {
    return mapped_code->second;
  }
  return from;
}

class DeviceMap {
 public:
  DeviceMap();
  ~DeviceMap() = delete;

  // Gets the singleton instance of the default DeviceMap. This
  // is created the first time it is used.
  static DeviceMap* Get();

  // Return the controller tuning object for a given device. Return null if none
  // is registered.
  ControllerTuning* GetControllerTuning(const std::string& device) const;

 private:
  // Create a controllertuning object.
  ControllerTuning* CreateControllerTuning();

  // Register a controller tuning object for a given device.
  void RegisterControllerTuning(const std::string& device,
                                ControllerTuning* tuning);

  std::unordered_map<std::string, ControllerTuning*> device_map_;

  // Registry for all known controller tunings.
  std::vector<std::unique_ptr<ControllerTuning>> controller_tunings_;
};

DeviceMap::DeviceMap() {
  // Add key mappings for the Xbox controllers.
  ControllerTuning* xbox_one_controller = CreateControllerTuning();
  RegisterControllerTuning("Microsoft X-Box One S pad", xbox_one_controller);
  RegisterControllerTuning("Microsoft X-Box One pad", xbox_one_controller);
  RegisterControllerTuning("Microsoft X-Box 360 pad", xbox_one_controller);
  xbox_one_controller->SetMinimumFlat(0.125);
  xbox_one_controller->MapKeyCode(BTN_NORTH, BTN_WEST);  // Map for 'X'
  xbox_one_controller->MapKeyCode(BTN_WEST, BTN_NORTH);  // Map for 'Y'
  xbox_one_controller->MapKeyCode(BTN_TL, BTN_TL2);      // Map for left bumper.
  xbox_one_controller->MapKeyCode(BTN_TR, BTN_TR2);  // Map for right bumper.

  // Add key mappings for the PlayStation 4 controller.
  ControllerTuning* ps4_controller = CreateControllerTuning();
  RegisterControllerTuning("Sony Interactive Entertainment Wireless Controller",
                           ps4_controller);
  RegisterControllerTuning("Sony Computer Entertainment Wireless Controller",
                           ps4_controller);
  RegisterControllerTuning("Sony PLAYSTATION(R)3 Controller", ps4_controller);

  ps4_controller->MapKeyCode(BTN_TL2, BTN_TL);  // Map for left trigger.
  ps4_controller->MapKeyCode(BTN_TR2, BTN_TR);  // Map for right trigger.
  ps4_controller->MapKeyCode(BTN_TL, BTN_TL2);  // Map for left bumper.
  ps4_controller->MapKeyCode(BTN_TR, BTN_TR2);  // Map for right bumper.

  // Add key mappings for the Horipad Switch wired controller.
  ControllerTuning* switch_controller = CreateControllerTuning();
  RegisterControllerTuning("HORI CO.,LTD. HORIPAD S", switch_controller);

  switch_controller->MapKeyCode(BTN_C, BTN_SOUTH);     // Map for 'A'.
  switch_controller->MapKeyCode(BTN_A, BTN_NORTH);     // Map for 'Y'.
  switch_controller->MapKeyCode(BTN_NORTH, BTN_WEST);  // Map for 'X'.
  switch_controller->MapKeyCode(BTN_WEST, BTN_TL2);    // Map for left bumper.
  switch_controller->MapKeyCode(BTN_Z, BTN_TR2);       // Map for right bumper.

  switch_controller->MapKeyCode(BTN_THUMBL, BTN_SELECT);  // Map for 'capture'.
  switch_controller->MapKeyCode(BTN_MODE, KEY_HOME);      // Map for 'home'.

  switch_controller->MapKeyCode(BTN_TL2, KEY_KPMINUS);  // Map for '-'.
  switch_controller->MapKeyCode(BTN_TR2, KEY_KPPLUS);   // Map for '+'.

  switch_controller->MapKeyCode(BTN_SELECT, BTN_THUMBL);  // Map for left hat.
  switch_controller->MapKeyCode(BTN_START, BTN_THUMBR);   // Map for right hat.
}

void DeviceMap::RegisterControllerTuning(const std::string& device,
                                         ControllerTuning* tuning) {
  SB_DCHECK(GetControllerTuning(device) == NULL ||
            GetControllerTuning(device) == tuning);
  device_map_[device] = tuning;
}

ControllerTuning* DeviceMap::CreateControllerTuning() {
  ControllerTuning* tuning = new ControllerTuning();
  controller_tunings_.emplace_back(tuning);
  return tuning;
}

ControllerTuning* DeviceMap::GetControllerTuning(
    const std::string& device) const {
  auto mapped_device = device_map_.find(device);
  if (mapped_device != device_map_.end()) {
    return mapped_device->second;
  }
  return NULL;
}

SB_ONCE_INITIALIZE_FUNCTION(DeviceMap, GetDeviceMap);

DeviceMap* DeviceMap::Get() {
  DeviceMap* device_map = GetDeviceMap();
  return device_map;
}

struct InputDeviceInfo {
  InputDeviceInfo() : fd(-1), touchpad_position_state(kTouchPadPositionNone) {}

  // Device Name
  std::string name;

  // File descriptor open for the device
  FileDescriptor fd;
  // Absolute Axis info.
  std::map<int, struct input_absinfo> axis_info;
  std::map<int, float> axis_value;
  int touchpad_position_state;
  ControllerTuning* tuning;
};

bool IsTouchpadPositionKnown(InputDeviceInfo* device_info) {
  return device_info->touchpad_position_state == kTouchPadPositionAll;
}

// Private implementation of DevInput.
class DevInputImpl : public DevInput {
 public:
  explicit DevInputImpl(SbWindow window);
  DevInputImpl(SbWindow window, FileDescriptor wake_up_fd);
  ~DevInputImpl() override;

  void InitDevInputImpl(SbWindow window);

  Event* PollNextSystemEvent() override;
  Event* WaitForSystemEventWithTimeout(SbTime time) override;
  void WakeSystemEventWait() override;

 private:
  // Converts an input_event into a kSbEventInput Application::Event. The caller
  // is responsible for deleting the returned event.
  Event* InputToApplicationEvent(const struct input_event& event,
                                 int modifiers,
                                 InputDeviceInfo* device_info);

  // Converts an input_event containing a key input into a kSbEventInput
  // Application::Event. The caller is responsible for deleting the returned
  // event.
  Event* KeyInputToApplicationEvent(const struct input_event& event,
                                    int modifiers,
                                    InputDeviceInfo* device_info);

  // Converts an input_event containing an axis event into a kSbEventInput
  // Application::Event. The caller is responsible for deleting the returned
  // event.
  Event* AxisInputToApplicationEvent(const struct input_event& event,
                                     int modifiers,
                                     InputDeviceInfo* device_info);

  // The window to attribute /dev/input events to.
  SbWindow window_;

  // A set of read-only file descriptor of keyboard input devices.
  std::vector<InputDeviceInfo> input_devices_;

  // A file descriptor of the write end of a pipe that can be written to from
  // any thread to wake up this waiter in a thread-safe manner.
  FileDescriptor wakeup_write_fd_;

  // A file descriptor of the read end of a pipe that this waiter will wait on
  // to allow it to be signalled safely from other threads.
  FileDescriptor wakeup_read_fd_;

  FileDescriptor wake_up_fd_;
};

// Helper class to manage a file descriptor set.
class FdSet {
 public:
  FdSet() : max_fd_(kInvalidFd) { FD_ZERO(&set_); }

  void Set(FileDescriptor fd) {
    FD_SET(fd, &set_);
    if (fd > max_fd_) {
      max_fd_ = fd;
    }
  }

  bool IsSet(FileDescriptor fd) { return FD_ISSET(fd, &set_); }

  fd_set* set() { return &set_; }
  FileDescriptor max_fd() { return max_fd_; }

 private:
  fd_set set_;
  FileDescriptor max_fd_;
};

// Converts an input_event code into an SbKey.
SbKey KeyCodeToSbKey(uint16_t code) {
  switch (code) {
    case KEY_BACKSPACE:
      return kSbKeyBack;
    case KEY_DELETE:
      return kSbKeyDelete;
    case KEY_TAB:
      return kSbKeyTab;
    case KEY_LINEFEED:
    case KEY_ENTER:
    case KEY_KPENTER:
      return kSbKeyReturn;
    case KEY_CLEAR:
      return kSbKeyClear;
    case KEY_SPACE:
      return kSbKeySpace;
    case KEY_HOME:
      return kSbKeyHome;
    case KEY_END:
      return kSbKeyEnd;
    case KEY_PAGEUP:
      return kSbKeyPrior;
    case KEY_PAGEDOWN:
      return kSbKeyNext;
    case KEY_LEFT:
      return kSbKeyLeft;
    case KEY_RIGHT:
      return kSbKeyRight;
    case KEY_DOWN:
      return kSbKeyDown;
    case KEY_UP:
      return kSbKeyUp;
    case KEY_ESC:
      return kSbKeyEscape;
    case KEY_KATAKANA:
    case KEY_HIRAGANA:
    case KEY_KATAKANAHIRAGANA:
      return kSbKeyKana;
    case KEY_HANGEUL:
      return kSbKeyHangul;
    case KEY_HANJA:
      return kSbKeyHanja;
    case KEY_HENKAN:
      return kSbKeyConvert;
    case KEY_MUHENKAN:
      return kSbKeyNonconvert;
    case KEY_ZENKAKUHANKAKU:
      return kSbKeyDbeDbcschar;
    case KEY_A:
      return kSbKeyA;
    case KEY_B:
      return kSbKeyB;
    case KEY_C:
      return kSbKeyC;
    case KEY_D:
      return kSbKeyD;
    case KEY_E:
      return kSbKeyE;
    case KEY_F:
      return kSbKeyF;
    case KEY_G:
      return kSbKeyG;
    case KEY_H:
      return kSbKeyH;
    case KEY_I:
      return kSbKeyI;
    case KEY_J:
      return kSbKeyJ;
    case KEY_K:
      return kSbKeyK;
    case KEY_L:
      return kSbKeyL;
    case KEY_M:
      return kSbKeyM;
    case KEY_N:
      return kSbKeyN;
    case KEY_O:
      return kSbKeyO;
    case KEY_P:
      return kSbKeyP;
    case KEY_Q:
      return kSbKeyQ;
    case KEY_R:
      return kSbKeyR;
    case KEY_S:
      return kSbKeyS;
    case KEY_T:
      return kSbKeyT;
    case KEY_U:
      return kSbKeyU;
    case KEY_V:
      return kSbKeyV;
    case KEY_W:
      return kSbKeyW;
    case KEY_X:
      return kSbKeyX;
    case KEY_Y:
      return kSbKeyY;
    case KEY_Z:
      return kSbKeyZ;

    case KEY_0:
      return kSbKey0;
    case KEY_1:
      return kSbKey1;
    case KEY_2:
      return kSbKey2;
    case KEY_3:
      return kSbKey3;
    case KEY_4:
      return kSbKey4;
    case KEY_5:
      return kSbKey5;
    case KEY_6:
      return kSbKey6;
    case KEY_7:
      return kSbKey7;
    case KEY_8:
      return kSbKey8;
    case KEY_9:
      return kSbKey9;

    case KEY_NUMERIC_0:
    case KEY_NUMERIC_1:
    case KEY_NUMERIC_2:
    case KEY_NUMERIC_3:
    case KEY_NUMERIC_4:
    case KEY_NUMERIC_5:
    case KEY_NUMERIC_6:
    case KEY_NUMERIC_7:
    case KEY_NUMERIC_8:
    case KEY_NUMERIC_9:
      return static_cast<SbKey>(kSbKey0 + (code - KEY_NUMERIC_0));

    case KEY_KP0:
      return kSbKeyNumpad0;
    case KEY_KP1:
      return kSbKeyNumpad1;
    case KEY_KP2:
      return kSbKeyNumpad2;
    case KEY_KP3:
      return kSbKeyNumpad3;
    case KEY_KP4:
      return kSbKeyNumpad4;
    case KEY_KP5:
      return kSbKeyNumpad5;
    case KEY_KP6:
      return kSbKeyNumpad6;
    case KEY_KP7:
      return kSbKeyNumpad7;
    case KEY_KP8:
      return kSbKeyNumpad8;
    case KEY_KP9:
      return kSbKeyNumpad9;

    case KEY_KPASTERISK:
      return kSbKeyMultiply;
    case KEY_KPDOT:
      return kSbKeyDecimal;
    case KEY_KPSLASH:
      return kSbKeyDivide;
    case KEY_KPPLUS:
    case KEY_EQUAL:
      return kSbKeyOemPlus;
    case KEY_COMMA:
      return kSbKeyOemComma;
    case KEY_KPMINUS:
    case KEY_MINUS:
      return kSbKeyOemMinus;
    case KEY_DOT:
      return kSbKeyOemPeriod;
    case KEY_SEMICOLON:
      return kSbKeyOem1;
    case KEY_SLASH:
      return kSbKeyOem2;
    case KEY_GRAVE:
      return kSbKeyOem3;
    case KEY_LEFTBRACE:
      return kSbKeyOem4;
    case KEY_BACKSLASH:
      return kSbKeyOem5;
    case KEY_RIGHTBRACE:
      return kSbKeyOem6;
    case KEY_APOSTROPHE:
      return kSbKeyOem7;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return kSbKeyShift;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return kSbKeyControl;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return kSbKeyMenu;
    case KEY_PAUSE:
      return kSbKeyPause;
    case KEY_CAPSLOCK:
      return kSbKeyCapital;
    case KEY_NUMLOCK:
      return kSbKeyNumlock;
    case KEY_SCROLLLOCK:
      return kSbKeyScroll;
    case KEY_SELECT:
      return kSbKeySelect;
    case KEY_PRINT:
      return kSbKeyPrint;
    case KEY_INSERT:
      return kSbKeyInsert;
    case KEY_HELP:
      return kSbKeyHelp;
    case KEY_MENU:
      return kSbKeyApps;
    case KEY_FN_F1:
    case KEY_FN_F2:
    case KEY_FN_F3:
    case KEY_FN_F4:
    case KEY_FN_F5:
    case KEY_FN_F6:
    case KEY_FN_F7:
    case KEY_FN_F8:
    case KEY_FN_F9:
    case KEY_FN_F10:
    case KEY_FN_F11:
    case KEY_FN_F12:
      return static_cast<SbKey>(kSbKeyF1 + (code - KEY_FN_F1));

    // For supporting multimedia buttons on a USB keyboard.
    case KEY_BACK:
      return kSbKeyBrowserBack;
    case KEY_FORWARD:
      return kSbKeyBrowserForward;
    case KEY_REFRESH:
      return kSbKeyBrowserRefresh;
    case KEY_STOP:
      return kSbKeyBrowserStop;
    case KEY_SEARCH:
      return kSbKeyBrowserSearch;
    case KEY_FAVORITES:
      return kSbKeyBrowserFavorites;
    case KEY_HOMEPAGE:
      return kSbKeyBrowserHome;
    case KEY_MUTE:
      return kSbKeyVolumeMute;
    case KEY_VOLUMEDOWN:
      return kSbKeyVolumeDown;
    case KEY_VOLUMEUP:
      return kSbKeyVolumeUp;
    case KEY_NEXTSONG:
      return kSbKeyMediaNextTrack;
    case KEY_PREVIOUSSONG:
      return kSbKeyMediaPrevTrack;
    case KEY_STOPCD:
      return kSbKeyMediaStop;
    case KEY_PLAYPAUSE:
      return kSbKeyMediaPlayPause;
    case KEY_MAIL:
      return kSbKeyMediaLaunchMail;
    case KEY_CALC:
      return kSbKeyMediaLaunchApp2;
    case KEY_WLAN:
      return kSbKeyWlan;
    case KEY_POWER:
      return kSbKeyPower;
    case KEY_BRIGHTNESSDOWN:
      return kSbKeyBrightnessDown;
    case KEY_BRIGHTNESSUP:
      return kSbKeyBrightnessUp;

    // Gamepad buttons.
    //   https://www.kernel.org/doc/Documentation/input/gamepad.txt
    case BTN_TL:
      return kSbKeyGamepadLeftTrigger;
    case BTN_TR:
      return kSbKeyGamepadRightTrigger;
    case BTN_DPAD_DOWN:
      return kSbKeyGamepadDPadDown;
    case BTN_DPAD_UP:
      return kSbKeyGamepadDPadUp;
    case BTN_DPAD_LEFT:
      return kSbKeyGamepadDPadLeft;
    case BTN_DPAD_RIGHT:
      return kSbKeyGamepadDPadRight;
    case BTN_TL2:
      return kSbKeyGamepadLeftBumper;
    case BTN_TR2:
      return kSbKeyGamepadRightBumper;
    case BTN_SOUTH:
      return kSbKeyGamepad1;
    case BTN_EAST:
      return kSbKeyGamepad2;
    case BTN_WEST:
      return kSbKeyGamepad3;
    case BTN_NORTH:
      return kSbKeyGamepad4;
    case BTN_SELECT:
      return kSbKeyGamepad5;
    case BTN_START:
      return kSbKeyGamepad6;
    case BTN_MODE:
      return kSbKeyGamepadSystem;
    case BTN_THUMBL:
      return kSbKeyGamepadLeftStick;
    case BTN_THUMBR:
      return kSbKeyGamepadRightStick;
  }
  SB_DLOG(WARNING) << "Unknown code: 0x" << std::hex << code;
  return kSbKeyUnknown;
}  // NOLINT(readability/fn_size)

// Get a SbKeyLocation from an input_event.code.
SbKeyLocation KeyCodeToSbKeyLocation(uint16_t code) {
  switch (code) {
    case KEY_LEFTALT:
    case KEY_LEFTCTRL:
    case KEY_LEFTMETA:
    case KEY_LEFTSHIFT:
      return kSbKeyLocationLeft;
    case KEY_RIGHTALT:
    case KEY_RIGHTCTRL:
    case KEY_RIGHTMETA:
    case KEY_RIGHTSHIFT:
      return kSbKeyLocationRight;
  }

  return kSbKeyLocationUnspecified;
}

// Returns the number of bytes needed to represent a bit set
// of |bit_count| size.
int BytesNeededForBitSet(int bit_count) {
  return (bit_count + 7) / 8;
}

// Returns true if |bit| is set in |bitset|.
bool IsBitSet(const std::vector<uint8_t>& bitset, int bit) {
  return !!(bitset.at(bit / 8) & (1 << (bit % 8)));
}

bool IsAxisFlat(int minimum_flat,
                float rest_value,
                const struct input_absinfo& axis_info) {
  SB_DCHECK((axis_info.flat * 2) <= (axis_info.maximum - axis_info.minimum));
  int flat = std::max(minimum_flat, axis_info.flat);
  return (flat != 0) && (axis_info.value > rest_value - flat) &&
         (axis_info.value < rest_value + flat);
}

float GetAxisValue(bool is_trigger,
                   float minimum_flat,
                   const struct input_absinfo& axis_info) {
  float median =
      static_cast<float>(axis_info.maximum + axis_info.minimum) / 2.0f;
  float range = static_cast<float>(axis_info.maximum - axis_info.minimum);
  float radius = range / 2.0f;
  if (IsAxisFlat(minimum_flat * radius, is_trigger ? axis_info.minimum : median,
                 axis_info)) {
    return is_trigger ? -1 : 0;
  }
  // Scale the axis value to [-1, 1].
  float axis_value = (static_cast<float>(axis_info.value) - median) / radius;

  if (axis_info.flat != 0) {
    // Calculate the flat value scaled to [0, 1].
    float flat = static_cast<float>(axis_info.flat) / range;

    int sign = axis_value < 0.0f ? -1 : 1;
    // Rescale the range:
    // [-1.0f, -flat] to [-1.0f, 0.0f] and [flat, 1.0f] to [0.0f, 1.0f].
    axis_value = (axis_value - sign * flat) / (1 - flat);
  }
  return axis_value;
}

void GetInputDeviceAbsoluteAxisInfo(int axis,
                                    const std::vector<uint8_t>& bits,
                                    InputDeviceInfo* info) {
  if (IsBitSet(bits, axis)) {
    struct input_absinfo axis_info;
    int result = ioctl(info->fd, EVIOCGABS(axis), &axis_info);
    if (result < 0) {
      return;
    }
    info->axis_info.insert(std::make_pair(axis, axis_info));
    bool is_trigger = axis == ABS_Z || axis == ABS_RZ;
    info->axis_value.insert(std::make_pair(
        axis,
        GetAxisValue(is_trigger, info->tuning->GetMinimumFlat(), axis_info)));
  }
}

void GetInputDeviceInfo(InputDeviceInfo* info) {
  char device_name[1024];
  if (ioctl(info->fd, EVIOCGNAME(sizeof(device_name)), device_name) >= 0) {
    size_t length = strnlen(device_name, sizeof(device_name));
    info->name.assign(device_name, length);
  }

  info->tuning = DeviceMap::Get()->GetControllerTuning(info->name);

  std::vector<uint8_t> axis_bits(BytesNeededForBitSet(KEY_MAX));
  int result =
      ioctl(info->fd, EVIOCGBIT(EV_ABS, axis_bits.size()), axis_bits.data());
  if (result < 0) {
    return;
  }

  GetInputDeviceAbsoluteAxisInfo(ABS_X, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_Y, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_Z, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_RZ, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_RX, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_RY, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_HAT0X, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_HAT0Y, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_MT_POSITION_X, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_MT_POSITION_Y, axis_bits, info);
  GetInputDeviceAbsoluteAxisInfo(ABS_MT_TRACKING_ID, axis_bits, info);
  // TODO: Handle multi-touch using ABS_MT_SLOT.
}

FileDescriptor OpenDeviceIfKeyboardOrGamepad(const char* path) {
  FileDescriptor fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    // Open can fail if the application doesn't have permission to access
    // the input device directly.
    return kInvalidFd;
  }

  std::vector<uint8_t> ev_bits(BytesNeededForBitSet(EV_CNT));
  std::vector<uint8_t> key_bits(BytesNeededForBitSet(KEY_MAX));

  int result = ioctl(fd, EVIOCGBIT(0, ev_bits.size()), ev_bits.data());
  if (result < 0) {
    close(fd);
    return kInvalidFd;
  }

  bool has_ev_key = IsBitSet(ev_bits, EV_KEY);
  if (!has_ev_key) {
    close(fd);
    return kInvalidFd;
  }

  result = ioctl(fd, EVIOCGBIT(EV_KEY, key_bits.size()), key_bits.data());
  if (result < 0) {
    close(fd);
    return kInvalidFd;
  }

  bool has_key_space = IsBitSet(key_bits, KEY_SPACE);
  bool has_gamepad_button = IsBitSet(key_bits, BTN_GAMEPAD);
  if (!has_key_space && !has_gamepad_button) {
    // If it doesn't have a space key or gamepad button, it may be a mouse.
    close(fd);
    return kInvalidFd;
  }

  result = ioctl(fd, EVIOCGRAB, 1);
  if (result != 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": "
                   << "Unable to get exclusive access to \"" << path << "\".";
    close(fd);
    return kInvalidFd;
  }
  return fd;
}

// Searches for the keyboard and game controller /dev/input devices, opens them
// and returns the device info with a file descriptor and absolute axis details.
std::vector<InputDeviceInfo> GetInputDevices() {
  const char kDevicePath[] = "/dev/input";
  SbDirectory directory = SbDirectoryOpen(kDevicePath, NULL);
  std::vector<InputDeviceInfo> input_devices;
  if (!SbDirectoryIsValid(directory)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": No /dev/input support, "
                   << "unable to open: " << kDevicePath;
    return input_devices;
  }

  while (true) {
#if SB_API_VERSION >= 12
    std::vector<char> entry(kSbFileMaxName);

    if (!SbDirectoryGetNext(directory, entry.data(), kSbFileMaxName)) {
      break;
    }

    std::string path = kDevicePath;
    path += "/";
    path += entry.data();
#else   // SB_API_VERSION >= 12
    SbDirectoryEntry entry;

    if (!SbDirectoryGetNext(directory, &entry)) {
      break;
    }

    std::string path = kDevicePath;
    path += "/";
    path += entry.name;
#endif  // SB_API_VERSION >= 12

    if (SbDirectoryCanOpen(path.c_str())) {
      // This is a subdirectory. Skip.
      continue;
    }

    FileDescriptor fd = OpenDeviceIfKeyboardOrGamepad(path.c_str());
    if (fd == kInvalidFd) {
      continue;
    }
    InputDeviceInfo info;
    info.fd = fd;
    GetInputDeviceInfo(&info);

    SB_DCHECK(info.fd != kInvalidFd);
    input_devices.push_back(info);
  }

  SbDirectoryClose(directory);
  return input_devices;
}

// Returns whether |key_code|'s bit is set in the bitmap |map|, assuming
// |key_code| fits into |map|.
bool TestKey(int key_code, char* map) {
  return map[key_code / 8] & (1 << (key_code % 8));
}

// Returns whether |key_code|'s bit is set in the bitmap |map|, assuming
// |key_code| fits into |map|.
int GetModifier(int left_key_code,
                int right_key_code,
                SbKeyModifiers modifier,
                char* map) {
  if (TestKey(left_key_code, map) || TestKey(right_key_code, map)) {
    return modifier;
  }

  return 0;
}

// Polls the given input file descriptor for an input_event. If there are no
// bytes available, assumes that there is no input event to read. If it gets a
// partial event, it will assume that it will be completed, and spins until it
// receives an entire event.
bool PollInputEvent(InputDeviceInfo* device_info,
                    struct input_event* out_event,
                    int* out_modifiers) {
  if (device_info->fd == kInvalidFd) {
    return false;
  }

  SB_DCHECK(out_event);
  SB_DCHECK(out_modifiers);

  const size_t kEventSize = sizeof(struct input_event);
  size_t remaining = kEventSize;
  char* buffer = reinterpret_cast<char*>(out_event);
  while (remaining > 0) {
    int bytes_read = read(device_info->fd, buffer, remaining);
    if (bytes_read <= 0) {
      if (errno == EAGAIN || bytes_read == 0) {
        if (remaining == kEventSize) {
          // Normal nothing there case.
          return false;
        }

        // We have a partial read, so keep trying.
        continue;
      }

      // Some unexpected type of read error occurred.
      SB_DLOG(ERROR) << __FUNCTION__ << ": Error reading input: " << errno
                     << " - " << strerror(errno);
      return false;
    }

    SB_DCHECK(bytes_read <= remaining)
        << "bytes_read=" << bytes_read << ", remaining=" << remaining;
    remaining -= bytes_read;
    buffer += bytes_read;
  }

  if ((out_event->type != EV_KEY) && (out_event->type != EV_ABS)) {
    return false;
  }

  // Calculate modifiers.
  int modifiers = 0;
  char map[(KEY_MAX / 8) + 1] = {0};
  errno = 0;
  int result = ioctl(device_info->fd, EVIOCGKEY(sizeof(map)), map);
  if (result != -1) {
    modifiers |=
        GetModifier(KEY_LEFTSHIFT, KEY_RIGHTSHIFT, kSbKeyModifiersShift, map);
    modifiers |=
        GetModifier(KEY_LEFTALT, KEY_RIGHTALT, kSbKeyModifiersAlt, map);
    modifiers |=
        GetModifier(KEY_LEFTCTRL, KEY_RIGHTCTRL, kSbKeyModifiersCtrl, map);
    modifiers |=
        GetModifier(KEY_LEFTMETA, KEY_RIGHTMETA, kSbKeyModifiersMeta, map);
  } else {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error getting modifiers: " << errno
                   << " - " << strerror(errno);
  }

  *out_modifiers = modifiers;
  return true;
}

// Simple helper to close a FileDescriptor if set, and to set it to kInvalidFd,
// to ensure it doesn't get used after close.
void CloseFdSafely(FileDescriptor* fd) {
  if (*fd != kInvalidFd) {
    close(*fd);
    *fd = kInvalidFd;
  }
}

// Also in starboard/shared/libevent/socket_waiter_internal.cc
// TODO: Consider consolidating.
int SetNonBlocking(FileDescriptor fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    flags = 0;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

DevInputImpl::DevInputImpl(SbWindow window)
    : window_(window),
      input_devices_(GetInputDevices()),
      wakeup_write_fd_(kInvalidFd),
      wakeup_read_fd_(kInvalidFd),
      wake_up_fd_(kInvalidFd) {
  InitDevInputImpl(window);
}

DevInputImpl::DevInputImpl(SbWindow window, FileDescriptor wake_up_fd)
    : window_(window),
      input_devices_(GetInputDevices()),
      wakeup_write_fd_(kInvalidFd),
      wakeup_read_fd_(kInvalidFd),
      wake_up_fd_(wake_up_fd) {
  InitDevInputImpl(window);
}

void DevInputImpl::InitDevInputImpl(SbWindow window) {
  // Initialize wakeup pipe.
  FileDescriptor fds[2] = {kInvalidFd, kInvalidFd};
  int result = pipe(fds);
  SB_DCHECK(result == 0) << "result=" << result;

  wakeup_read_fd_ = fds[0];
  SB_DCHECK(wakeup_read_fd_ != kInvalidFd);
  result = SetNonBlocking(wakeup_read_fd_);
  SB_DCHECK(result == 0) << "result=" << result;

  wakeup_write_fd_ = fds[1];
  SB_DCHECK(wakeup_write_fd_ != kInvalidFd);
  result = SetNonBlocking(wakeup_write_fd_);
  SB_DCHECK(result == 0) << "result=" << result;
}

DevInputImpl::~DevInputImpl() {
  for (const auto& device : input_devices_) {
    close(device.fd);
  }
  CloseFdSafely(&wakeup_write_fd_);
  CloseFdSafely(&wakeup_read_fd_);
}

DevInput::Event* DevInputImpl::PollNextSystemEvent() {
  struct input_event event;
  int modifiers = 0;
  for (auto& device : input_devices_) {
    if (!PollInputEvent(&device, &event, &modifiers)) {
      continue;
    }

    return InputToApplicationEvent(event, modifiers, &device);
  }
  return NULL;
}

DevInput::Event* DevInputImpl::WaitForSystemEventWithTimeout(SbTime duration) {
  Event* event = PollNextSystemEvent();
  if (event) {
    return event;
  }

  FdSet read_set;
  if (wake_up_fd_ != kInvalidFd) {
    read_set.Set(wake_up_fd_);
  }

  for (std::vector<InputDeviceInfo>::const_iterator it = input_devices_.begin();
       it != input_devices_.end(); ++it) {
    read_set.Set(it->fd);
  }
  read_set.Set(wakeup_read_fd_);

  struct timeval tv;
  SbTime clamped_duration = std::max(duration, (SbTime)0);
  ToTimevalDuration(clamped_duration, &tv);
  if (select(read_set.max_fd() + 1, read_set.set(), NULL, NULL, &tv) == 0) {
    // This is the timeout case.
    return NULL;
  }

  // We may have been woken up, so let's consume one wakeup, if selected.
  if (read_set.IsSet(wakeup_read_fd_)) {
    char buf;
    int bytes_read = HANDLE_EINTR(read(wakeup_read_fd_, &buf, 1));
    SB_DCHECK(bytes_read == 1);
  }

  return PollNextSystemEvent();
}

void DevInputImpl::WakeSystemEventWait() {
  char buf = 1;
  while (true) {
    int bytes_written = HANDLE_EINTR(write(wakeup_write_fd_, &buf, 1));
    if (bytes_written > 0) {
      SB_DCHECK(bytes_written == 1) << "bytes_written=" << bytes_written;
      return;
    }

    if (errno == EAGAIN) {
      continue;
    }

    SB_DLOG(FATAL) << __FUNCTION__ << ": errno=" << errno << " - "
                   << strerror(errno);
  }
}

namespace {

// Creates a key event for an analog button input.
DevInput::Event* CreateAnalogButtonKeyEvent(SbWindow window,
                                            float axis_value,
                                            float previous_axis_value,
                                            SbKey key,
                                            SbKeyLocation location,
                                            int modifiers,
                                            const struct input_event& event) {
  SbInputEventType previous_type =
      (std::abs(previous_axis_value) > 0.5 ? kSbInputEventTypePress
                                           : kSbInputEventTypeUnpress);
  SbInputEventType type =
      (std::abs(axis_value) > 0.5 ? kSbInputEventTypePress
                                  : kSbInputEventTypeUnpress);
  if (previous_type == type) {
    // Key press/unpress state did not change.
    return NULL;
  }

  SbInputData* data = new SbInputData();
  memset(data, 0, sizeof(*data));
#if SB_API_VERSION < 13
  data->timestamp = SbTimeGetMonotonicNow();
#endif  // SB_API_VERSION < 13
  data->window = window;
  data->type = type;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->device_id = kGamepadDeviceId;
  data->key = key;
  data->key_location = location;
  data->key_modifiers = modifiers;
  return new DevInput::Event(kSbEventTypeInput, data,
                             &Application::DeleteDestructor<SbInputData>);
}

// Creates a move event with key for a stick input.
DevInput::Event* CreateMoveEventWithKey(SbWindow window,
                                        SbKey key,
                                        SbKeyLocation location,
                                        int modifiers,
                                        const SbInputVector& input_vector) {
  SbInputData* data = new SbInputData();
  memset(data, 0, sizeof(*data));
#if SB_API_VERSION < 13
  data->timestamp = SbTimeGetMonotonicNow();
#endif  // SB_API_VERSION < 13
  data->window = window;
  data->type = kSbInputEventTypeMove;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->device_id = kGamepadDeviceId;

  data->key = key;
  data->key_location = location;
  data->key_modifiers = modifiers;
  data->position = input_vector;
  data->pressure = NAN;
  data->size = {NAN, NAN};
  data->tilt = {NAN, NAN};

  return new DevInput::Event(kSbEventTypeInput, data,
                             &Application::DeleteDestructor<SbInputData>);
}

DevInput::Event* CreateTouchPadEvent(SbWindow window,
                                     SbInputEventType type,
                                     SbKey key,
                                     SbKeyLocation location,
                                     int modifiers,
                                     const SbInputVector& input_vector) {
  SbInputData* data = new SbInputData();
  memset(data, 0, sizeof(*data));
#if SB_API_VERSION < 13
  data->timestamp = SbTimeGetMonotonicNow();
#endif  // SB_API_VERSION < 13
  data->window = window;
  data->type = type;
  data->device_type = kSbInputDeviceTypeTouchPad;
  data->device_id = kGamepadDeviceId;

  data->key = key;
  data->key_location = location;
  data->key_modifiers = modifiers;
  data->position = input_vector;
  data->pressure = NAN;
  data->size = {NAN, NAN};
  data->tilt = {NAN, NAN};

  return new DevInput::Event(kSbEventTypeInput, data,
                             &Application::DeleteDestructor<SbInputData>);
}

}  // namespace

DevInput::Event* DevInputImpl::AxisInputToApplicationEvent(
    const struct input_event& event,
    int modifiers,
    InputDeviceInfo* device_info) {
  SB_DCHECK(event.type == EV_ABS);
  SbKey key = kSbKeyUnknown;
  float axis_value = 0;
  float previous_axis_value = 0;
  auto axis_info_it = device_info->axis_info.find(event.code);
  if (axis_info_it != device_info->axis_info.end()) {
    struct input_absinfo& axis_info = axis_info_it->second;
    axis_info.value = event.value;
    bool is_trigger = event.code == ABS_Z || event.code == ABS_RZ;
    axis_value = GetAxisValue(is_trigger, device_info->tuning->GetMinimumFlat(),
                              axis_info);
    float& stored_axis_value = device_info->axis_value[event.code];
    previous_axis_value = stored_axis_value;
    if (previous_axis_value == axis_value) {
      // If the value is unchanged, don't do anything.
      return NULL;
    }
    stored_axis_value = axis_value;
  }

  SbKeyLocation location = kSbKeyLocationUnspecified;
  SbInputVector input_vector;
  // The mapping for the axis codes can vary from controller to controller.
  // TODO: Include axis mapping for controllers with different layout.
  // Report up and left as negative values.
  switch (event.code) {
    case ABS_X:
      input_vector.x = axis_value;
      input_vector.y = device_info->axis_value[ABS_Y];
      key = kSbKeyGamepadLeftStickLeft;
      location = kSbKeyLocationLeft;
      return CreateMoveEventWithKey(window_, key, location, modifiers,
                                    input_vector);
    case ABS_Y: {
      input_vector.x = device_info->axis_value[ABS_X];
      input_vector.y = axis_value;
      key = kSbKeyGamepadLeftStickUp;
      location = kSbKeyLocationLeft;
      return CreateMoveEventWithKey(window_, key, location, modifiers,
                                    input_vector);
    }
    case ABS_RX:
      input_vector.x = axis_value;
      input_vector.y = device_info->axis_value[ABS_RZ];
      key = kSbKeyGamepadRightStickLeft;
      location = kSbKeyLocationRight;
      return CreateMoveEventWithKey(window_, key, location, modifiers,
                                    input_vector);
    case ABS_RY:
      input_vector.x = device_info->axis_value[ABS_Z];
      input_vector.y = axis_value;
      key = kSbKeyGamepadRightStickUp;
      location = kSbKeyLocationRight;
      return CreateMoveEventWithKey(window_, key, location, modifiers,
                                    input_vector);
    case ABS_Z: {
      key = kSbKeyGamepadLeftTrigger;
      location = kSbKeyLocationLeft;
      // For trigger buttons, the range is [0..1].
      float trigger_value = (axis_value + 1) / 2;
      float previous_trigger_value = (previous_axis_value + 1) / 2;
      return CreateAnalogButtonKeyEvent(window_, trigger_value,
                                        previous_trigger_value, key, location,
                                        modifiers, event);
    }
    case ABS_RZ: {
      key = kSbKeyGamepadRightTrigger;
      location = kSbKeyLocationRight;
      // For trigger buttons, the range is [0..1].
      float trigger_value = (axis_value + 1) / 2;
      float previous_trigger_value = (previous_axis_value + 1) / 2;
      return CreateAnalogButtonKeyEvent(window_, trigger_value,
                                        previous_trigger_value, key, location,
                                        modifiers, event);
    }
    case ABS_HAT0X: {
      float axis_value_for_key =
          std::abs(axis_value) > 0.5f ? axis_value : previous_axis_value;
      key = (axis_value_for_key < 0) ? kSbKeyGamepadDPadLeft
                                     : kSbKeyGamepadDPadRight;
      return CreateAnalogButtonKeyEvent(window_, axis_value,
                                        previous_axis_value, key, location,
                                        modifiers, event);
    }
    case ABS_HAT0Y: {
      float axis_value_for_key =
          std::abs(axis_value) > 0.5f ? axis_value : previous_axis_value;
      key = (axis_value_for_key < 0) ? kSbKeyGamepadDPadUp
                                     : kSbKeyGamepadDPadDown;
      return CreateAnalogButtonKeyEvent(window_, axis_value,
                                        previous_axis_value, key, location,
                                        modifiers, event);
    }
    case ABS_MT_TRACKING_ID:
      if (event.value == -1) {
        bool touchpad_position_is_known = IsTouchpadPositionKnown(device_info);
        device_info->touchpad_position_state = kTouchPadPositionNone;
        if (touchpad_position_is_known) {
          // Touch point is released, report last known position as unpress.
          input_vector.x = device_info->axis_value[ABS_MT_POSITION_X];
          input_vector.y = device_info->axis_value[ABS_MT_POSITION_Y];
          return CreateTouchPadEvent(window_, kSbInputEventTypeUnpress, key,
                                     location, modifiers, input_vector);
        }
      }
      return NULL;
    case ABS_MT_POSITION_X: {
      // If all positions were known before this event, then this event is a
      // move.
      SbInputEventType type = IsTouchpadPositionKnown(device_info)
                                  ? kSbInputEventTypeMove
                                  : kSbInputEventTypePress;
      device_info->touchpad_position_state |= kTouchPadPositionX;
      if (IsTouchpadPositionKnown(device_info)) {
        // For touchpads, the unit range is [-1..1]. Negative values for top
        // left.
        input_vector.x = axis_value;
        input_vector.y = device_info->axis_value[ABS_MT_POSITION_Y];
        return CreateTouchPadEvent(window_, type, key, location, modifiers,
                                   input_vector);
      }
      // Not all axis positions are known yet.
      return NULL;
    }
    case ABS_MT_POSITION_Y: {
      // If all positions were known before this event, then this event is a
      // move.
      SbInputEventType type = IsTouchpadPositionKnown(device_info)
                                  ? kSbInputEventTypeMove
                                  : kSbInputEventTypePress;
      device_info->touchpad_position_state |= kTouchPadPositionY;
      if (IsTouchpadPositionKnown(device_info)) {
        // For touchpads, the range is [-1..1]. Negative values for top left.
        input_vector.x = device_info->axis_value[ABS_MT_POSITION_X];
        input_vector.y = axis_value;
        return CreateTouchPadEvent(window_, type, key, location, modifiers,
                                   input_vector);
      }
      // Not all axis positions are known yet.
      return NULL;
    }
    default:
      // Ignored event codes.
      return NULL;
  }

  SB_NOTREACHED();
  return NULL;
}

DevInput::Event* DevInputImpl::KeyInputToApplicationEvent(
    const struct input_event& event,
    int modifiers,
    InputDeviceInfo* device_info) {
  SB_DCHECK(event.type == EV_KEY);
  SB_DCHECK(event.value <= 2);

  SbKey key = KeyCodeToSbKey(device_info->tuning
                                 ? device_info->tuning->GetKeyCode(event.code)
                                 : event.code);
  if (key == kSbKeyUnknown) {
    return NULL;
  }

  SbInputData* data = new SbInputData();
  memset(data, 0, sizeof(*data));
#if SB_API_VERSION < 13
  data->timestamp = SbTimeGetMonotonicNow();
#endif  // SB_API_VERSION < 13
  data->window = window_;
  data->type =
      (event.value == 0 ? kSbInputEventTypeUnpress : kSbInputEventTypePress);
  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = kKeyboardDeviceId;
  data->key = key;
  data->key_location = KeyCodeToSbKeyLocation(event.code);
  data->key_modifiers = modifiers;
  return new Event(kSbEventTypeInput, data,
                   &Application::DeleteDestructor<SbInputData>);
}

DevInput::Event* DevInputImpl::InputToApplicationEvent(
    const struct input_event& event,
    int modifiers,
    InputDeviceInfo* device_info) {
  // EV_ABS events are axis values: Sticks, dpad, and touchpad.
  // https://www.kernel.org/doc/Documentation/input/event-codes.txt
  switch (event.type) {
    case EV_ABS:
      return AxisInputToApplicationEvent(event, modifiers, device_info);
    case EV_KEY:
      return KeyInputToApplicationEvent(event, modifiers, device_info);
  }
  return NULL;
}

}  // namespace

// static
DevInput* DevInput::Create(SbWindow window) {
  return new DevInputImpl(window);
}

// static
DevInput* DevInput::Create(SbWindow window, int wake_up_fd) {
  return new DevInputImpl(window, wake_up_fd);
}

}  // namespace dev_input
}  // namespace shared
}  // namespace starboard
