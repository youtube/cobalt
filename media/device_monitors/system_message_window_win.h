// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_DEVICE_MONITORS_SYSTEM_MESSAGE_WINDOW_WIN_H_
#define MEDIA_DEVICE_MONITORS_SYSTEM_MESSAGE_WINDOW_WIN_H_

#include <windows.h>

#include <memory>

#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT SystemMessageWindowWin {
 public:
  SystemMessageWindowWin();

  SystemMessageWindowWin(const SystemMessageWindowWin&) = delete;
  SystemMessageWindowWin& operator=(const SystemMessageWindowWin&) = delete;

  virtual ~SystemMessageWindowWin();

  virtual LRESULT OnDeviceChange(UINT event_type, LPARAM data);

 private:
  void Init();

  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);

  static LRESULT CALLBACK WndProcThunk(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam) {
    SystemMessageWindowWin* msg_wnd = reinterpret_cast<SystemMessageWindowWin*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (msg_wnd)
      return msg_wnd->WndProc(hwnd, message, wparam, lparam);
    return ::DefWindowProc(hwnd, message, wparam, lparam);
  }

  HMODULE instance_;
  HWND window_;
  class DeviceNotifications;
  std::unique_ptr<DeviceNotifications> device_notifications_;
};

}  // namespace media

#endif  // MEDIA_DEVICE_MONITORS_SYSTEM_MESSAGE_WINDOW_WIN_H_
