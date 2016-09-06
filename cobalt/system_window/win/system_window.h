/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SYSTEM_WINDOW_WIN_SYSTEM_WINDOW_H_
#define COBALT_SYSTEM_WINDOW_WIN_SYSTEM_WINDOW_H_

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/system_window/system_window.h"

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace cobalt {
namespace system_window {

// Windows implementation of the SystemWindow class.
// Each window has its own thread and event loop on which messages are
// received and processed.
class SystemWindowWin : public SystemWindow {
 public:
  explicit SystemWindowWin(base::EventDispatcher* event_dispatcher);
  SystemWindowWin(base::EventDispatcher* event_dispatcher,
                  const math::Size& window_size);

  ~SystemWindowWin() OVERRIDE;

  math::Size GetWindowSize() const OVERRIDE { return window_size_; }

  HWND window_handle() const { return window_handle_; }

 protected:
  static LRESULT CALLBACK
      WndProc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);
  void StartWindow();
  void EndWindow();

  // Checks whether this is a repeat key event.
  static bool IsKeyRepeat(LPARAM l_param);

 private:
  void Win32WindowThread();

  base::Thread thread;
  base::WaitableEvent window_initialized;
  HWND window_handle_;
  math::Size window_size_;
};

}  // namespace system_window
}  // namespace cobalt

#endif  // COBALT_SYSTEM_WINDOW_WIN_SYSTEM_WINDOW_H_
