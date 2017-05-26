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

#ifndef STARBOARD_SHARED_UWP_APPLICATION_UWP_H_
#define STARBOARD_SHARED_UWP_APPLICATION_UWP_H_

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace __winRT {
// TODO: without this, we get the following error at CoreApplication::Run:
// 'long __winRT::__getActivationFactoryByPCWSTR(i
//  void *,Platform::Guid &,void **)':
//  cannot convert argument 1 from 'const wchar_t [46]' to 'void *'
inline long __getActivationFactoryByPCWSTR(const wchar_t* a,
                                           Platform::Guid& b,
                                           void** c) {
  return __getActivationFactoryByPCWSTR(
      static_cast<void*>(const_cast<wchar_t*>(a)), b, c);
}
}  // namespace __winRT

namespace starboard {
namespace shared {
namespace uwp {

class ApplicationUwp : public shared::starboard::Application {
 public:
  ApplicationUwp();
  ~ApplicationUwp() SB_OVERRIDE;

  static ApplicationUwp* Get() {
    return static_cast<ApplicationUwp*>(shared::starboard::Application::Get());
  }

// Do not use the macro from windows.h.
#undef CreateWindow
#undef CreateWindowW
  SbWindow CreateWindow(const SbWindowOptions* options);

  bool DestroyWindow(SbWindow window);

  void DispatchStart() {
    shared::starboard::Application::DispatchStart();
  }

 private:
  // --- Application overrides ---
  bool IsStartImmediate() SB_OVERRIDE { return false; }
  void Initialize() SB_OVERRIDE;
  void Teardown() SB_OVERRIDE;
  Event* GetNextEvent() SB_OVERRIDE;
  bool DispatchNextEvent() SB_OVERRIDE;
  void Inject(Event* event) SB_OVERRIDE;
  void InjectTimedEvent(TimedEvent* timed_event) SB_OVERRIDE;
  void CancelTimedEvent(SbEventId event_id) SB_OVERRIDE;
  TimedEvent* GetNextDueTimedEvent() SB_OVERRIDE;
  SbTimeMonotonic GetNextTimedEventTargetTime() SB_OVERRIDE;

  // The single open window, if any.
  SbWindow window_;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_APPLICATION_UWP_H_
