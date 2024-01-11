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

#ifndef STARBOARD_SHARED_WIN32_WINDOW_INTERNAL_H_
#define STARBOARD_SHARED_WIN32_WINDOW_INTERNAL_H_

#include <EGL/egl.h>

// Windows headers.
#include <windows.h>

#include "starboard/atomic.h"
#include "starboard/window.h"

struct SbWindowPrivate {
  SbWindowPrivate(const SbWindowOptions* options, HWND window_handle);
  ~SbWindowPrivate();

  HWND GetWindowHandle() { return window_handle_; }

  // The width of this window.
  int width;

  // The height of this window.
  int height;

 private:
  HWND window_handle_;
};

#endif  // STARBOARD_SHARED_WIN32_WINDOW_INTERNAL_H_
