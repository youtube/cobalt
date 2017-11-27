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

#ifndef STARBOARD_SHARED_DIRECTFB_APPLICATION_DIRECTFB_H_
#define STARBOARD_SHARED_DIRECTFB_APPLICATION_DIRECTFB_H_

#include <directfb.h>

#include <vector>

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard {

class ApplicationDirectFB : public shared::starboard::QueueApplication {
 public:
  ApplicationDirectFB();
  ~ApplicationDirectFB() override;

  static ApplicationDirectFB* Get() {
    return static_cast<ApplicationDirectFB*>(
        shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);

  IDirectFB* GetDirectFB();
  SbWindow GetWindow();

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
  Event* PollNextSystemEvent() override;
  Event* WaitForSystemEventWithTimeout(SbTime time) override;
  void WakeSystemEventWait() override;

 private:
  Event* DFBEventToEvent(const DFBInputEvent& event);

  // Pointer to the IDirectFB object that is owned by ApplicationDirectFB.
  // This is used to access all DirectFB functionality.
  IDirectFB* directfb_;

  // Handle to the single SbWindow object, if it exists.  If it does not exist,
  // this is set to kSbWindowInvalid.  DirectFB applications can have a maximum
  // of one window.
  SbWindow window_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_DIRECTFB_APPLICATION_DIRECTFB_H_
