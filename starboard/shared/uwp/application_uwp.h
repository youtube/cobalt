// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/input.h"
#include "starboard/key.h"
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

private
ref class DisplayStatusWatcher sealed {
 public:
  void CreateWatcher();
  bool IsWatcherStarted();
  void StopWatcher();

 private:
  Windows::Devices::Enumeration::DeviceWatcher ^ watcher_;
};

class ApplicationUwp : public shared::starboard::Application,
                       private AnalogThumbstickThread::Callback {
 public:
  const float kDefaultScreenRefreshRate = 60.f;

#if SB_API_VERSION >= 15
  explicit ApplicationUwp(SbEventHandleCallback sb_event_handle_callback);
#else
  ApplicationUwp();
#endif
  ~ApplicationUwp() override;

  static ApplicationUwp* Get() {
    return static_cast<ApplicationUwp*>(shared::starboard::Application::Get());
  }

  SbWindow CreateWindowForUWP(const SbWindowOptions* options);

  // Returns window size if the window_ has been instantiated, otherwise returns
  // theoretical maximum visible size, i.e. screen size for active display.
  SbWindowSize GetVisibleAreaSize();

  bool DestroyWindow(SbWindow window);

  void DispatchStart(SbTimeMonotonic timestamp) {
    shared::starboard::Application::DispatchStart(timestamp);
  }

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
  void SetCoreWindow(Windows::UI::Core::CoreWindow ^ window) {
    core_window_ = window;

    dispatcher_ = window->Dispatcher;
    transport_controls_ =
        Windows::Media::SystemMediaTransportControls::GetForCurrentView();
  }

  void OnKeyEvent(Windows::UI::Core::CoreWindow ^ sender,
                  Windows::UI::Core::KeyEventArgs ^ args,
                  bool up);

  void Inject(Event* event) override;

  void InjectKeypress(SbKey key);

  void SetStartLink(const char* link) {
    shared::starboard::Application::SetStartLink(link);
  }

  Platform::String ^ GetString(const char* id, const char* fallback) const;

  void UpdateDisplayPreferredMode();

  bool IsHdrSupported() {
    ScopedLock lock(preferred_display_mode_mutex_);
    return preferred_display_mode_hdr_ != nullptr;
  }

  float GetRefreshRate() {
    ScopedLock lock(preferred_display_mode_mutex_);
    if (preferred_display_mode_hdr_ != nullptr) {
      return preferred_display_mode_hdr_->RefreshRate;
    }
    if (preferred_display_mode_hdmi_ != nullptr) {
      return preferred_display_mode_hdmi_->RefreshRate;
    }
    // Sometimes it happens that functions are requesting refresh rate earlier
    // than Application is fully initialized and preferred_display_modes are
    // nullptr, for such cases we return default most common for typical
    // monitors refresh rate.
    return kDefaultScreenRefreshRate;
  }

  // clang-format off
  Windows::Graphics::Display::Core::HdmiDisplayMode ^
      GetPreferredModeHdr() {
        ScopedLock lock(preferred_display_mode_mutex_);
        return preferred_display_mode_hdr_;
      }

      Windows::Graphics::Display::Core::HdmiDisplayMode
      ^
      GetPreferredModeHdmi() {
        ScopedLock lock(preferred_display_mode_mutex_);
        return preferred_display_mode_hdmi_;
      }
      // clang-format on

      bool IsHdcpOn();
  // Returns true on success.
  bool TurnOnHdcp();
  // Returns true on success.
  bool TurnOffHdcp();

  bool SetOutputProtection(bool should_enable_dhcp);

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

  // Calculates the preferred size of main application window depending on
  // platform API available and the mode used to launch application. Used by
  // CreateWindowForUWP and GetVisibleAreaSize if the window_ is not yet
  // instantiated.
  SbWindowSize GetPreferredWindowSize();

  // These two functions should only be called while holding
  // |hdcp_session_mutex_|.
  Windows::Media::Protection::HdcpSession ^ GetHdcpSession();
  void ResetHdcpSession();

  bool ClearLocalCacheIfNeeded();
  bool ClearLocalCacheFolder();

  // TODO: Check if |window_| requires mutex or that it is safely accessed
  // The single open window, if any.
  SbWindow window_;
  SbWindowSize window_size_;
  Platform::Agile<Windows::UI::Core::CoreWindow> core_window_;

  Platform::Agile<Windows::UI::Core::CoreDispatcher> dispatcher_;
  Platform::Agile<Windows::Media::SystemMediaTransportControls>
      transport_controls_;

  shared::starboard::LocalizedStrings localized_strings_;

  Mutex time_event_mutex_;
  // |timer_event_map_| is locked by |mutex_|.
  std::unordered_map<SbEventId, Windows::System::Threading::ThreadPoolTimer ^>
      timer_event_map_;

  int device_id_;

  // |hdcp_session_| is locked by |hdcp_session_mutex_|.
  Mutex hdcp_session_mutex_;
  Windows::Media::Protection::HdcpSession ^ hdcp_session_;

  scoped_ptr<AnalogThumbstickThread> analog_thumbstick_thread_;
  Windows::ApplicationModel::SuspendingDeferral ^ suspend_deferral_ = nullptr;

  Mutex preferred_display_mode_mutex_;
  Windows::Graphics::Display::Core::HdmiDisplayMode ^
      preferred_display_mode_hdr_ = nullptr;
  Windows::Graphics::Display::Core::HdmiDisplayMode ^
      preferred_display_mode_hdmi_ = nullptr;
  DisplayStatusWatcher ^ watcher_ = nullptr;
};

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_APPLICATION_UWP_H_
