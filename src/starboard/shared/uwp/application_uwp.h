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

#include <D3D12.h>
#include <agile.h>

#include <string>
#include <unordered_map>

#include "starboard/configuration.h"
#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/localized_strings.h"
#include "starboard/shared/uwp/analog_thumbstick_input_thread.h"
#include "starboard/types.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace uwp {

// Including <agile.h>, will eventually include <windows.h>, which includes
// C:\Program Files (x86)\Windows Kits\10\Include\10.0.15063.0\um\processenv.h,
// line 164 in processenv.h redefines GetCommandLine to GetCommandLineW if
// UNICODE is defined.
// This function was added so that it could be used as a work around when
// GetCommandLine() needed to be called.
const starboard::CommandLine*
GetCommandLinePointer(starboard::Application* app);

class ApplicationUwp : public shared::starboard::Application,
                       private AnalogThumbstickThread::Callback {
 public:
  ApplicationUwp();
  ~ApplicationUwp() override;

  static ApplicationUwp* Get() {
    return static_cast<ApplicationUwp*>(shared::starboard::Application::Get());
  }

  SbWindow CreateWindowForUWP(const SbWindowOptions* options);

  bool DestroyWindow(SbWindow window);

  void DispatchStart() { shared::starboard::Application::DispatchStart(); }

  // public for IFrameworkView subclass
  void SetCommandLine(int argc, const char** argv) {
    shared::starboard::Application::SetCommandLine(argc, argv);
  }

  // public for IFrameworkView subclass
  bool DispatchAndDelete(Application::Event* event) {
    return shared::starboard::Application::DispatchAndDelete(event);
  }

  Platform::Agile<Windows::UI::Core::CoreWindow> GetCoreWindow() const {
    return core_window_;
  }

  Platform::Agile<Windows::UI::Core::CoreDispatcher> GetDispatcher() const {
    return dispatcher_;
  }

  Platform::Agile<Windows::Media::SystemMediaTransportControls>
      GetTransportControls() const {
    return transport_controls_;
  }

  // public for IFrameworkView subclass
  void SetCoreWindow(Windows::UI::Core::CoreWindow^ window) {
    core_window_ = window;

    dispatcher_ = window->Dispatcher;
    transport_controls_ =
        Windows::Media::SystemMediaTransportControls::GetForCurrentView();
  }

  void OnKeyEvent(Windows::UI::Core::CoreWindow^ sender,
                  Windows::UI::Core::KeyEventArgs^ args,
                  bool up);

  void Inject(Event* event) override;

  void InjectKeypress(SbKey key);

  void SetStartLink(const char* link) {
    shared::starboard::Application::SetStartLink(link);
  }

  Platform::String^ GetString(const char* id, const char* fallback) const;

  bool IsHdcpOn();
  // Returns true on success.
  bool TurnOnHdcp();
  // Returns true on success.
  bool TurnOffHdcp();

  Microsoft::WRL::ComPtr<ID3D12Device> GetD3D12Device() { return d3d12device_; }

  void SetD3D12Device(const Microsoft::WRL::ComPtr<ID3D12Device>& device) {
    d3d12device_ = device;
  }

 private:
  // --- Application overrides ---
  bool IsStartImmediate() override { return false; }
  void Initialize() override;
  void Teardown() override;
  Event* GetNextEvent() override;
  bool DispatchNextEvent() override;
  void InjectTimedEvent(TimedEvent* timed_event) override;
  void CancelTimedEvent(SbEventId event_id) override;
  TimedEvent* GetNextDueTimedEvent() override;
  SbTimeMonotonic GetNextTimedEventTargetTime() override;

  int device_id() const { return device_id_; }
  void OnJoystickUpdate(SbKey key, SbInputVector value) override;

  // These two functions should only be called while holding
  // |hdcp_session_mutex_|.
  Windows::Media::Protection::HdcpSession^ GetHdcpSession();
  void ResetHdcpSession();

  // The single open window, if any.
  SbWindow window_;
  Platform::Agile<Windows::UI::Core::CoreWindow> core_window_;

  Platform::Agile<Windows::UI::Core::CoreDispatcher> dispatcher_;
  Platform::Agile<Windows::Media::SystemMediaTransportControls>
      transport_controls_;

  shared::starboard::LocalizedStrings localized_strings_;

  Mutex mutex_;
  // |timer_event_map_| is locked by |mutex_|.
  std::unordered_map<SbEventId, Windows::System::Threading::ThreadPoolTimer^>
      timer_event_map_;

  Microsoft::WRL::ComPtr<ID3D12Device> d3d12device_;
  int device_id_;

  // |hdcp_session_| is locked by |hdcp_session_mutex_|.
  Mutex hdcp_session_mutex_;
  Windows::Media::Protection::HdcpSession^ hdcp_session_;

  scoped_ptr<AnalogThumbstickThread> analog_thumbstick_thread_;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_APPLICATION_UWP_H_
