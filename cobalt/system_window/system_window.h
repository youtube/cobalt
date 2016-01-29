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

#ifndef COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_
#define COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace system_window {

// A SystemWindow represents a window on desktop systems as well as
// as a mechanism for routing system events and notifications.
// The SystemWindow routes callbacks for user input, and provides the
// information necessary to create a display render target for a graphics
// system. A SystemWindow is typically created via a call to
// CreateSystemWindow(), which should be implemented on every Cobalt
// platform.
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

  // Type to indicate dialog severity. May be used by the platform-specific
  // implementation to control aspects of dialog presentation.
  enum DialogSeverity { kDialogInfo, kDialogWarning, kDialogError };

  // Options structure for dialog creation. It is expected that each platform
  // will implement a modal dialog with possible support for:
  // A text message.
  // An indication of severity: info, warning or error.
  // 1-3 buttons, where usually 1 = OK, 2 = Yes/No, 3 = Yes/No/Cancel.
  // A callback indicating the user's response: positive, negative or cancel.
  struct DialogOptions {
    DialogOptions() : severity(kDialogInfo), num_buttons(1) {}
    DialogSeverity severity;
    int num_buttons;
    std::string message;
    DialogCallback callback;
  };

  SystemWindow(base::EventDispatcher* event_dispatcher,
               const math::Size& window_size)
      : event_dispatcher_(event_dispatcher), window_size_(window_size) {}
  virtual ~SystemWindow();

  // Launches a system dialog.
  virtual void ShowDialog(const DialogOptions& options);

  base::EventDispatcher* event_dispatcher() const { return event_dispatcher_; }

  const math::Size& window_size() const { return window_size_; }

 private:
  base::EventDispatcher* event_dispatcher_;
  const math::Size window_size_;
};

// The implementation of this function should be platform specific, and will
// create and return a platform-specific system window object. The system
// window object routes callbacks for user input and provides the information
// necessary to create a display render target for a graphics system.
scoped_ptr<SystemWindow> CreateSystemWindow(base::EventDispatcher*,
                                            const math::Size& window_size);

}  // namespace system_window
}  // namespace cobalt

#endif  // COBALT_SYSTEM_WINDOW_SYSTEM_WINDOW_H_
