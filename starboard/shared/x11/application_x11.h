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

#ifndef STARBOARD_SHARED_X11_APPLICATION_X11_H_
#define STARBOARD_SHARED_X11_APPLICATION_X11_H_

#include <X11/Xlib.h>

#include <map>
#include <queue>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/linux/dev_input/dev_input.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace x11 {

// This application engine combines the generic queue with the X11 event queue.
class ApplicationX11 : public shared::starboard::QueueApplication {
 public:
  ApplicationX11();
  ~ApplicationX11() override;

  static ApplicationX11* Get() {
    return static_cast<ApplicationX11*>(shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);

  void Composite();

 protected:
  void AcceptFrame(SbPlayer player,
                   const scoped_refptr<VideoFrame>& frame,
                   int z_index,
                   int x,
                   int y,
                   int width,
                   int height) override;

#if SB_API_VERSION >= 6
  bool IsStartImmediate() override { return !HasPreloadSwitch(); }
  bool IsPreloadImmediate() override { return HasPreloadSwitch(); }
#endif  // SB_API_VERSION >= 6

 protected:
  // --- Application overrides ---
  void Initialize() override;
  void Teardown() override;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() override;
  Event* WaitForSystemEventWithTimeout(SbTime time) override;
  void WakeSystemEventWait() override;

 private:
  typedef std::vector<SbWindow> SbWindowVector;

  struct FrameInfo {
    SbPlayer player;
    scoped_refptr<VideoFrame> frame;
    int z_index;
    int x;
    int y;
    int width;
    int height;
  };

  // Ensures that X is up, display is populated and connected, returning whether
  // it succeeded.
  bool EnsureX();

  // Shuts X down.
  void StopX();

  // Retreive the next pending event, such as keypresses from a paste buffer.
  // Returns NULL if there are no pending events.
  Event* GetPendingEvent();

  // Creates a new shared::Application::Event from an XEvent, passing ownership
  // of the Event to the caller.
  Event* XEventToEvent(XEvent* x_event);

  // Finds the SbWindow associated with the given X Window.
  SbWindow FindWindow(Window window);

  Atom wake_up_atom_;
  Atom wm_delete_atom_;

  SbEventId composite_event_id_;
  Mutex frame_mutex_;
  int frame_read_index_;
  bool frames_updated_;

  static const int kNumFrames = 2;
  // Video frames from different videos sorted by their z indices.
  std::map<int, FrameInfo> frame_infos_[kNumFrames];

  Display* display_;
  SbWindowVector windows_;

  // Storage for characters pending from a clipboard paste.
  std::queue<unsigned char> paste_buffer_pending_characters_;
  // Indicates whether a key press event that requires a matching release has
  // been dispatched.
  bool paste_buffer_key_release_pending_;

  // The /dev/input input handler. Only set when there is an open window.
  scoped_ptr<::starboard::shared::dev_input::DevInput> dev_input_;
};

}  // namespace x11
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_X11_APPLICATION_X11_H_
