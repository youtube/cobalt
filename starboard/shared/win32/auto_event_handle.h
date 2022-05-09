// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_AUTO_EVENT_HANDLE_H_
#define STARBOARD_SHARED_WIN32_AUTO_EVENT_HANDLE_H_

#include <winsock2.h>

namespace starboard {
namespace shared {
namespace win32 {

class AutoEventHandle {
 public:
  explicit AutoEventHandle(WSAEVENT event) : event_(event) {}

  ~AutoEventHandle() { CleanupExistingEvent(); }

  void Reset(WSAEVENT new_event) {
    CleanupExistingEvent();
    event_ = new_event;
  }

  bool IsValid() const { return event_ != WSA_INVALID_EVENT; }

  WSAEVENT GetEvent() { return event_; }

 private:
  AutoEventHandle(const AutoEventHandle&) = delete;
  AutoEventHandle& operator=(const AutoEventHandle&) = delete;

  void CleanupExistingEvent() {
    if (IsValid()) {
      WSACloseEvent(event_);
      event_ = WSA_INVALID_EVENT;
    }
  }

  WSAEVENT event_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_AUTO_EVENT_HANDLE_H_
