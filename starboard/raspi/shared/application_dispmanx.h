// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <bcm_host.h>

#include <vector>

#include "starboard/configuration.h"
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
  ApplicationDispmanx()
      : display_(DISPMANX_NO_HANDLE), window_(kSbWindowInvalid) {}
  ~ApplicationDispmanx() SB_OVERRIDE {}

  static ApplicationDispmanx* Get() {
    return static_cast<ApplicationDispmanx*>(
        ::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);

 protected:
  // --- Application overrides ---
  void Initialize() SB_OVERRIDE;
  void Teardown() SB_OVERRIDE;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() SB_OVERRIDE;
  Event* PollNextSystemEvent() SB_OVERRIDE;
  Event* WaitForSystemEventWithTimeout(SbTime duration) SB_OVERRIDE;
  void WakeSystemEventWait() SB_OVERRIDE;

 private:
  // Returns whether DISPMANX has been initialized.
  bool IsDispmanxInitialized() { return display_ != DISPMANX_NO_HANDLE; }

  // Ensures that X is up, display is populated and connected.
  void InitializeDispmanx();

  // Shuts X down.
  void ShutdownDispmanx();

  // The DISPMANX display.
  DISPMANX_DISPLAY_HANDLE_T display_;

  // The single open window, if any.
  SbWindow window_;

  // The /dev/input input handler. Only set when there is an open window.
  ::starboard::shared::dev_input::DevInput* input_;
};

}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_APPLICATION_DISPMANX_H_
