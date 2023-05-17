// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_RASPI_SHARED_APPLICATION_DISPMANX_H_
#define STARBOARD_RASPI_SHARED_APPLICATION_DISPMANX_H_

#include <memory>

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/raspi/shared/dispmanx_util.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/linux/dev_input/dev_input.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard {
namespace raspi {
namespace shared {

// This application engine combines the generic queue with the X11 event queue.
class ApplicationDispmanx
    : public ::starboard::shared::starboard::QueueApplication {
 public:
#if SB_API_VERSION >= 15
  explicit ApplicationDispmanx(SbEventHandleCallback sb_event_handle_callback)
      : window_(kSbWindowInvalid), QueueApplication(sb_event_handle_callback) {}
#else
  ApplicationDispmanx() : window_(kSbWindowInvalid) {}
#endif  // SB_API_VERSION >= 15
  ~ApplicationDispmanx() override {}

  static ApplicationDispmanx* Get() {
    return static_cast<ApplicationDispmanx*>(
        ::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);

 protected:
  // --- Application overrides ---
  void Initialize() override;
  void Teardown() override;
  void OnSuspend() override;
  void OnResume() override;
  void AcceptFrame(SbPlayer player,
                   const scoped_refptr<VideoFrame>& frame,
                   int z_index,
                   int x,
                   int y,
                   int width,
                   int height) override;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() override;
  Event* PollNextSystemEvent() override;
  Event* WaitForSystemEventWithTimeout(SbTime duration) override;
  void WakeSystemEventWait() override;

  bool IsStartImmediate() override { return !HasPreloadSwitch(); }
  bool IsPreloadImmediate() override { return HasPreloadSwitch(); }

 private:
  // Returns whether DISPMANX has been initialized.
  bool IsDispmanxInitialized() { return display_ != NULL; }

  // Ensures that X is up, display is populated and connected.
  void InitializeDispmanx();

  // Shuts X down.
  void ShutdownDispmanx();

  // The DISPMANX display.
  scoped_ptr<DispmanxDisplay> display_;

  // DISPMANX helper to render video frames.
  scoped_ptr<DispmanxVideoRenderer> video_renderer_;

  // The single open window, if any.
  SbWindow window_;

  // The /dev/input input handler. Only set when there is an open window.
  std::unique_ptr<::starboard::shared::dev_input::DevInput> input_;
};

}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_APPLICATION_DISPMANX_H_
