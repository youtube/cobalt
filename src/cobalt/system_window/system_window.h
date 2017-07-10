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

#ifndef COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_
#define COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/math/size.h"
#include "cobalt/system_window/input_event.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/system.h"

namespace cobalt {
namespace system_window {

// A SystemWindow represents a window on desktop systems as well as as a
// mechanism for routing system events and notifications.  The SystemWindow
// routes callbacks for user input, and provides the information necessary to
// create a display render target for a graphics system.
class SystemWindow {
 public:
  // Enumeration of possible responses for the dialog callback.
  enum DialogResponse {
    kDialogPositiveResponse,
    kDialogNegativeResponse,
    kDialogCancelResponse
  };

  // Type of callback to run when user closes a dialog.
  typedef base::Callback<void(DialogResponse response)> DialogCallback;

  // Enumeration of possible message codes for a dialog.
  enum DialogMessageCode {
    kDialogConnectionError
  };

  // Options structure for dialog creation. It is expected that each platform
  // will implement a modal dialog with possible support for:
  // A message code specifying the text to be displayed, which should be
  // localized according to the platform.
  // A callback indicating the user's response: positive, negative or cancel.
  struct DialogOptions {
    DialogMessageCode message_code;
    DialogCallback callback;
  };

  SystemWindow(base::EventDispatcher* event_dispatcher,
               const base::optional<math::Size>& window_size);
  ~SystemWindow();

  // Launches a system dialog.
  void ShowDialog(const DialogOptions& options);

  // Returns the dimensions of the window.
  math::Size GetWindowSize() const;

  // video pixel ratio = resolution of video output / resolution of window.  Its
  // value is usually 1.0.  Set it to a value greater than 1.0 allows the video
  // to be played in higher resolution than the window.
  float GetVideoPixelRatio() const;

  base::EventDispatcher* event_dispatcher() const { return event_dispatcher_; }

  // Returns a handle to the Starboard window object.
  SbWindow GetSbWindow();

  // Gets the platform-specific window handle as a void*.
  void* GetWindowHandle();

  // Handles a single Starboard input event, dispatching any appropriate events.
  void HandleInputEvent(const SbInputData& data);

  // Called when the user closes the dialog.
  void HandleDialogClose(SbSystemPlatformErrorResponse response);

 private:
  void UpdateModifiers(SbKey key, bool pressed);
  InputEvent::Modifiers GetModifiers();
  void DispatchInputEvent(const SbInputData& data, InputEvent::Type type,
                          bool is_repeat);
  void HandlePointerInputEvent(const SbInputData& data);

  base::EventDispatcher* event_dispatcher_;

  SbWindow window_;

  bool key_down_;

  // The current dialog callback. Only one dialog may be open at a time.
  DialogCallback current_dialog_callback_;
};

}  // namespace system_window
}  // namespace cobalt

#endif  // COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_
