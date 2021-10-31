// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_
#define COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_

#include "base/optional.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/math/size.h"
#include "cobalt/system_window/input_event.h"
#include "starboard/input.h"
#include "starboard/key.h"

namespace cobalt {
namespace system_window {

// A SystemWindow represents a window on desktop systems as well as as a
// mechanism for routing system events and notifications.  The SystemWindow
// routes callbacks for user input, and provides the information necessary to
// create a display render target for a graphics system.
class SystemWindow {
 public:
  SystemWindow(base::EventDispatcher* event_dispatcher,
               const base::Optional<math::Size>& window_size);
  ~SystemWindow();

  // Returns the dimensions of the window.
  math::Size GetWindowSize() const;

  // Gets the size of the diagonal length between opposing corners of the
  // screen. This is queried from the window_.
  float GetDiagonalSizeInches() const;

  // device pixel ratio = resolution of video output / resolution of window. Its
  // value is usually 1.0.  Set it to a value greater than 1.0 allows the video
  // to be played in higher resolution than the window.
  float GetDevicePixelRatio() const;

  // Returns the resolution of the video output size, which may be different
  // from the window size. This will generally be GetWindowSize() multiplied by
  // GetDevicePixelRatio().
  math::Size GetVideoOutputResolution() const;

  base::EventDispatcher* event_dispatcher() const { return event_dispatcher_; }

  // Returns a handle to the Starboard window object.
  SbWindow GetSbWindow();

  // Gets the platform-specific window handle as a void*.
  void* GetWindowHandle();

  // Handles a single Starboard input event, dispatching any appropriate events.
  void HandleInputEvent(const SbEvent* event);

  // Returns the primary SystemWindow currently in use by the application.
  // Only one SystemWindow is currently supported, and this method will return
  // that window, or |nullptr| if it does not exist.
  static SystemWindow* PrimaryWindow();

 private:
  void UpdateModifiers(SbKey key, bool pressed);
  InputEvent::Modifiers GetModifiers();

  void DispatchInputEvent(const SbEvent* event,
                          InputEvent::Type type, bool is_repeat);

  void HandlePointerInputEvent(const SbEvent* event);

  base::EventDispatcher* event_dispatcher_;

  SbWindow window_;

  bool key_down_;
};

}  // namespace system_window
}  // namespace cobalt

#endif  // COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_
