// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_X11_WINDOW_INTERNAL_H_
#define STARBOARD_SHARED_X11_WINDOW_INTERNAL_H_

#include <X11/Xlib.h>

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/window.h"

#if SB_IS(PLAYER_PUNCHED_OUT)
#include <X11/extensions/Xrender.h>
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

struct SbWindowPrivate {
  SbWindowPrivate(Display* display, const SbWindowOptions* options);
  ~SbWindowPrivate();

  Window window;

#if SB_IS(PLAYER_PUNCHED_OUT)
  // Composites graphics and the given video frame video for this window. In
  // PLAYER_PUNCHED_OUT mode, this is the only way any graphics or video is
  // presented in the window.
  void Composite(::starboard::shared::starboard::player::VideoFrame* frame);

  // The cached XRender Picture that represents the window that is the
  // destination of the composition.
  Picture window_picture;

  // This pixmap is used to composite video and graphics before rendering onto
  // the actual window. This is necessary to avoid horrible flickering.
  Pixmap composition_pixmap;

  // This window is bound to EGL and used to get the GL graphics output into an
  // XRender Picture that can be manually composited with video.
  Window gl_window;
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

  Display* display;

  int width;
  int height;
};

#endif  // STARBOARD_SHARED_X11_WINDOW_INTERNAL_H_
