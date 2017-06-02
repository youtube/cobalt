// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_UWP_WINDOW_INTERNAL_H_
#define STARBOARD_SHARED_UWP_WINDOW_INTERNAL_H_

#include "starboard/atomic.h"
#include "starboard/time.h"
#include "starboard/window.h"

struct SbWindowPrivate {
  explicit SbWindowPrivate(const SbWindowOptions* options);
  ~SbWindowPrivate();

  // Wait for the next video out vblank event.
  void WaitForVBlank();

  // Get the process time of the latest video out vblank event.
  SbTime GetVBlankProcessTime();

  // Get the video out refresh rate with the provided pointer. Return value
  // indicates success.
  bool GetRefreshRate(uint64_t* refresh_rate);

  // The width of this window.
  int width;

  // The height of this window.
  int height;

  // The actual output resolution width.
  int output_width;

  // The actual output resolution height.
  int output_height;

 private:
  // Open and Initialize the video output port.
  void SetupVideo();

  // Close the video output port.
  void ShutdownVideo();

  // TODO: Ensure this gets called when the output resolution may have changed,
  // such as when resuming after suspend.
  // Retrieve the width and height from the video output resolution.
  void RefreshOutputResolution();
};

#endif  // STARBOARD_SHARED_UWP_WINDOW_INTERNAL_H_
