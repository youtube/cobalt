// Copyright 2016 Google Inc. All Rights Reserved.
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
#include <string>

#include "starboard/configuration.h"
#include "starboard/directory.h"
#include "starboard/input.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/time_internal.h"
#include "starboard/string.h"

namespace starboard {
namespace shared {
namespace dev_input {
namespace {

using ::starboard::shared::starboard::Application;

typedef int FileDescriptor;
const FileDescriptor kInvalidFd = -1;
const int kKeyboardDeviceId = 1;

// Private implementation of DevInput.
class DevInputImpl : public DevInput {
 public:
  explicit DevInputImpl(SbWindow window);
  ~DevInputImpl() SB_OVERRIDE;

  Event* PollNextSystemEvent() SB_OVERRIDE;
  Event* WaitForSystemEventWithTimeout(SbTime time) SB_OVERRIDE;
  void WakeSystemEventWait() SB_OVERRIDE;

 private:
  // Converts an input_event into a kSbEventInput Application::Event. The caller
  // is responsible for deleting the returnede event.
  Event* InputToApplicationEvent(const struct input_event& event);

  // The window to attribute /dev/input events to.
  SbWindow window_;

  // A read-only file descriptor of the keyboard input device.
  FileDescriptor keyboard_fd_;

  // A file descriptor of the write end of a pipe that can be written to from
  // any thread to wake up this waiter in a thread-safe manner.
  FileDescriptor wakeup_write_fd_;

  // A file descriptor of the read end of a pipe that this waiter will wait on
  // to allow it to be signalled safely from other threads.
  FileDescriptor wakeup_read_fd_;
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
    case BTN_DPAD_LEFT:
      return kSbKeyLeft;
    case KEY_RIGHT:
    case BTN_DPAD_RIGHT:
      return kSbKeyRight;
    case KEY_DOWN:
    case BTN_DPAD_DOWN:
      return kSbKeyDown;
    case KEY_UP:
    case BTN_DPAD_UP:
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
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
      return static_cast<SbKey>(kSbKey0 + (code - KEY_0));

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
    case KEY_KP1:
    case KEY_KP2:
    case KEY_KP3:
    case KEY_KP4:
    case KEY_KP5:
    case KEY_KP6:
    case KEY_KP7:
    case KEY_KP8:
    case KEY_KP9:
      return static_cast<SbKey>(kSbKeyNumpad0 + (code - KEY_KP0));

    case KEY_KPASTERISK:
      return kSbKeyMultiply;
    case KEY_KPDOT:
      return kSbKeyDecimal;
    case KEY_KPSLASH:
      return kSbKeyDivide;
    case KEY_KPPLUS:
      return kSbKeyOemPlus;
    case KEY_COMMA:
      return kSbKeyOemComma;
    case KEY_KPMINUS:
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

// Searches for the keyboard /dev/input device, and opens it, returning the file
// descriptor. Returns kInvalidFd if unable to do so.
FileDescriptor GetKeyboardFd() {
  const char kDevicePath[] = "/dev/input/by-id";
  const char kKeyboardDeviceMatch[] = "event-kbd";
  SbDirectory directory = SbDirectoryOpen(kDevicePath, NULL);
  if (!SbDirectoryIsValid(directory)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": No /dev/input support, "
                   << "unable to open: " << kDevicePath;
    return kInvalidFd;
  }

  FileDescriptor fd = kInvalidFd;
  while (true) {
    SbDirectoryEntry entry;
    if (!SbDirectoryGetNext(directory, &entry)) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": No /dev/input support, "
                     << "unable to find \"" << kKeyboardDeviceMatch
                     << "\" device.";
      break;
    }

    if (SbStringFindString(entry.name, kKeyboardDeviceMatch) == NULL) {
      continue;
    }

    std::string path = kDevicePath;
    path += "/";
    path += entry.name;
    fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": No /dev/input support, "
                     << "unable to open \"" << path << "\".";
      fd = kInvalidFd;
      break;
    }

    int result = ioctl(fd, EVIOCGRAB, 1);
    if (result != 0) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": No /dev/input support, "
                     << "unable to get exclusive access to \"" << path << "\".";
      close(fd);
      fd = kInvalidFd;
      break;
    }

    SB_DCHECK(fd != kInvalidFd);
    break;
  }

  SbDirectoryClose(directory);
  return fd;
}

// Polls the given keyboard file descriptor for an input_event. If there are no
// bytes available, assumes that there is no input event to read. If it gets a
// partial event, it will assume that it will be completed, and spins until it
// receives an entire event.
bool PollKeyboardEvent(FileDescriptor fd, struct input_event* out_event) {
  if (fd == kInvalidFd) {
    return false;
  }

  const size_t kEventSize = sizeof(struct input_event);
  size_t remaining = kEventSize;
  char* buffer = reinterpret_cast<char*>(out_event);
  while (remaining > 0) {
    int bytes_read = HANDLE_EINTR(read(fd, buffer, remaining));
    if (bytes_read <= 0) {
      if (errno == EAGAIN || bytes_read == 0) {
        if (remaining == kEventSize) {
          // Normal nothing there case.
          return false;
        }

        // We have a partial read, so keep trying.
        continue;
      }

      // Some unexpected type of read error occured.
      SB_DLOG(ERROR) << __FUNCTION__ << ": error reading keyboard: " << errno
                     << " - " << strerror(errno);
      return false;
    }

    SB_DCHECK(bytes_read <= remaining) << "bytes_read=" << bytes_read
                                       << ", remaining=" << remaining;
    remaining -= bytes_read;
    buffer += bytes_read;
  }

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
int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

DevInputImpl::DevInputImpl(SbWindow window)
    : window_(window),
      keyboard_fd_(GetKeyboardFd()),
      wakeup_write_fd_(kInvalidFd),
      wakeup_read_fd_(kInvalidFd) {
  SB_DLOG(INFO) << "Keyboard File Descriptor: " << keyboard_fd_;

  // Initialize wakeup pipe.
  int fds[2] = {kInvalidFd, kInvalidFd};
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
  CloseFdSafely(&keyboard_fd_);
  CloseFdSafely(&wakeup_write_fd_);
  CloseFdSafely(&wakeup_read_fd_);
}

DevInput::Event* DevInputImpl::PollNextSystemEvent() {
  struct input_event event;
  if (!PollKeyboardEvent(keyboard_fd_, &event)) {
    return NULL;
  }

  return InputToApplicationEvent(event);
}

DevInput::Event* DevInputImpl::WaitForSystemEventWithTimeout(SbTime duration) {
  Event* event = PollNextSystemEvent();
  if (event) {
    return event;
  }

  FdSet read_set;
  if (keyboard_fd_ != kInvalidFd) {
    read_set.Set(keyboard_fd_);
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

  if (keyboard_fd_ != kInvalidFd && read_set.IsSet(keyboard_fd_)) {
    return PollNextSystemEvent();
  }

  return NULL;
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

DevInput::Event* DevInputImpl::InputToApplicationEvent(
    const struct input_event& event) {
  if (event.type != EV_KEY) {
    return NULL;
  }

  // Only support press/release for now.
  if (event.value > 1) {
    return NULL;
  }

  SbInputData* data = new SbInputData();
  SbMemorySet(data, 0, sizeof(*data));
  data->window = window_;
  data->type =
      (event.value == 1 ? kSbInputEventTypePress : kSbInputEventTypeUnpress);
  data->device_type = kSbInputDeviceTypeKeyboard;
  data->device_id = kKeyboardDeviceId;
  data->key = KeyCodeToSbKey(event.code);
  data->key_location = KeyCodeToSbKeyLocation(event.code);
  return new Event(kSbEventTypeInput, data,
                   &Application::DeleteDestructor<SbInputData>);
}

}  // namespace

// static
DevInput* DevInput::Create(SbWindow window) {
  return new DevInputImpl(window);
}

}  // namespace dev_input
}  // namespace shared
}  // namespace starboard
