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

#include "starboard/shared/x11/window_internal.h"

#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "starboard/log.h"

namespace {
const int kWindowWidth = 1920;
const int kWindowHeight = 1080;
}

SbWindowPrivate::SbWindowPrivate(Display* display,
                                 const SbWindowOptions* options)
    : window(None), display(display) {
  // Request a 32-bit depth visual for our Window.
  XVisualInfo x_visual_info;
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor,
                   &x_visual_info);

  Window root_window = RootWindow(display, x_visual_info.screen);

  XSetWindowAttributes swa;
  swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;
  swa.colormap =
      XCreateColormap(display, root_window, x_visual_info.visual, AllocNone);
  // Setting border_pixel to 0 is required if the requested window depth (e.g.
  // 32) is not equal to the parent window's depth (e.g. 24).
  swa.border_pixel = 0;

  int window_width = kWindowWidth;
  int window_height = kWindowHeight;
  if (options && options->size.width > 0 && options->size.height > 0) {
    window_width = options->size.width;
    window_height = options->size.height;
  }

  window =
      XCreateWindow(display, root_window, 0, 0, window_width, window_height, 0,
                    x_visual_info.depth, InputOutput, x_visual_info.visual,
                    CWBorderPixel | CWEventMask | CWColormap, &swa);

  SB_CHECK(window) << "Failed to create the X window.";

  width = window_width;
  height = window_height;

  const char* name = "Cobalt";
  if (options && options->name) {
    name = options->name;
  }
  XStoreName(display, window, name);

  Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(display, window, &wm_delete, 1);

  XMapWindow(display, window);
}

SbWindowPrivate::~SbWindowPrivate() {
  XDestroyWindow(display, window);
}
