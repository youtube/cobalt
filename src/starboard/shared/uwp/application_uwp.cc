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

#include "starboard/shared/uwp/application_uwp.h"

#include <WinSock2.h>
#include <mfapi.h>
#include <ppltasks.h>
#include <windows.h>
#include <D3D11.h>

#include <memory>
#include <string>
#include <vector>

#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/shared/uwp/analog_thumbstick_input_thread.h"
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/uwp/log_file_impl.h"
#include "starboard/shared/uwp/window_internal.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace sbuwp = starboard::shared::uwp;
namespace sbwin32 = starboard::shared::win32;

using Microsoft::WRL::ComPtr;
using starboard::shared::starboard::Application;
using starboard::shared::starboard::CommandLine;
using starboard::shared::uwp::ApplicationUwp;
using starboard::shared::uwp::GetArgvZero;
using starboard::shared::uwp::WaitForResult;
using starboard::shared::win32::stringToPlatformString;
using starboard::shared::win32::wchar_tToUTF8;
using starboard::shared::starboard::player::VideoFrame;
using Windows::ApplicationModel::Activation::ActivationKind;
using Windows::ApplicationModel::Activation::DialReceiverActivatedEventArgs;
using Windows::ApplicationModel::Activation::IActivatedEventArgs;
using Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs;
using Windows::ApplicationModel::Core::CoreApplication;
using Windows::ApplicationModel::Core::CoreApplicationView;
using Windows::ApplicationModel::Core::IFrameworkView;
using Windows::ApplicationModel::Core::IFrameworkViewSource;
using Windows::ApplicationModel::SuspendingEventArgs;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::EventHandler;
using Windows::Foundation::IAsyncOperation;
using Windows::Foundation::TimeSpan;
using Windows::Foundation::TypedEventHandler;
using Windows::Foundation::Uri;
using Windows::Globalization::Calendar;
using Windows::Media::Protection::HdcpProtection;
using Windows::Media::Protection::HdcpSession;
using Windows::Media::Protection::HdcpSetProtectionResult;
using Windows::Networking::Connectivity::ConnectionProfile;
using Windows::Networking::Connectivity::NetworkConnectivityLevel;
using Windows::Networking::Connectivity::NetworkInformation;
using Windows::Networking::Connectivity::NetworkStatusChangedEventHandler;
using Windows::Storage::KnownFolders;
using Windows::Storage::StorageFolder;
using Windows::System::Threading::ThreadPoolTimer;
using Windows::System::Threading::TimerElapsedHandler;
using Windows::System::UserAuthenticationStatus;
using Windows::UI::Core::CoreDispatcherPriority;
using Windows::UI::Core::CoreProcessEventsOption;
using Windows::UI::Core::CoreWindow;
using Windows::UI::Core::DispatchedHandler;
using Windows::UI::Core::KeyEventArgs;
using Windows::UI::Popups::IUICommand;
using Windows::UI::Popups::MessageDialog;
using Windows::UI::Popups::UICommand;
using Windows::UI::Popups::UICommandInvokedHandler;

namespace {

// Per Microsoft, HdcpProtection::On means HDCP 1.x required.
const HdcpProtection kHDCPProtectionMode = HdcpProtection::On;

const int kWinSockVersionMajor = 2;
const int kWinSockVersionMinor = 2;

const char kDialParamPrefix[] = "cobalt-dial:?";
const char kLogPathSwitch[] = "xb1_log_file";

int main_return_value = 0;

int MakeDeviceId() {
  // TODO: Devices MIGHT have colliding hashcodes. Some other unique int
  // ID generation tool would be better.
  using Windows::Security::ExchangeActiveSyncProvisioning::
      EasClientDeviceInformation;
  auto device_information = ref new EasClientDeviceInformation();
  Platform::String ^ device_id_string = device_information->Id.ToString();
  return device_id_string->GetHashCode();
}

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

// Parses a starboard: URI scheme by splitting args at ';' boundaries.
std::vector<std::string> ParseStarboardUri(const std::string& uri) {
  std::vector<std::string> result;
  result.push_back(GetArgvZero());

  size_t index = uri.find(':');
  if (index == std::string::npos) {
    return result;
  }

  std::string args = uri.substr(index + 1);

  while (!args.empty()) {
    size_t next = args.find(';');
    result.push_back(args.substr(0, next));
    if (next == std::string::npos) {
      return result;
    }
    args = args.substr(next + 1);
  }
  return result;
}

#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

std::unique_ptr<Application::Event> MakeDeepLinkEvent(
  const std::string& uri_string) {
  SB_LOG(INFO) << "Navigate to: [" << uri_string << "]";
  const size_t kMaxDeepLinkSize = 128 * 1024;
  const std::size_t uri_size = uri_string.size();
  if (uri_size > kMaxDeepLinkSize) {
    SB_NOTREACHED() << "App launch data too big: " << uri_size;
    return nullptr;
  }

  const int kBufferSize = static_cast<int>(uri_string.size()) + 1;
  char* deep_link = new char[kBufferSize];
  SB_DCHECK(deep_link);
  SbStringCopy(deep_link, uri_string.c_str(), kBufferSize);

  return std::unique_ptr<Application::Event>(
      new Application::Event(kSbEventTypeLink, deep_link,
                             Application::DeleteArrayDestructor<const char*>));
}

// Returns if |full_string| ends with |substring|.
bool ends_with(const std::string& full_string, const std::string& substring) {
  if (substring.length() > full_string.length()) {
    return false;
  }
  return std::equal(substring.rbegin(), substring.rend(), full_string.rbegin());
}

std::string GetBinaryName() {
  std::string full_binary_path = GetArgvZero();
  std::string::size_type index = full_binary_path.rfind(SB_FILE_SEP_CHAR);
  if (index == std::string::npos) {
    return full_binary_path;
  }

  return full_binary_path.substr(index + 1);
}

}  // namespace

// Note that this is a "struct" and not a "class" because
// that's how it's defined in starboard/system.h
struct SbSystemPlatformErrorPrivate {
  SbSystemPlatformErrorPrivate(const SbSystemPlatformErrorPrivate&) = delete;
  SbSystemPlatformErrorPrivate& operator=(
      const SbSystemPlatformErrorPrivate&) = delete;

  SbSystemPlatformErrorPrivate(
      SbSystemPlatformErrorType type,
      SbSystemPlatformErrorCallback callback,
      void* user_data)
      : callback_(callback), user_data_(user_data) {
    SB_DCHECK(type == kSbSystemPlatformErrorTypeConnectionError);

    ApplicationUwp* app = ApplicationUwp::Get();
    app->RunInMainThreadAsync([this, callback, user_data, app]() {
      MessageDialog^ dialog = ref new MessageDialog(
          app->GetString("UNABLE_TO_CONTACT_YOUTUBE_1",
              "Sorry, could not connect to YouTube."));
      dialog->Commands->Append(
          MakeUICommand(
              "OFFLINE_MESSAGE_TRY_AGAIN", "Try again",
              kSbSystemPlatformErrorResponsePositive));
      dialog->Commands->Append(
          MakeUICommand(
              "EXIT_BUTTON", "Exit",
              kSbSystemPlatformErrorResponseCancel));
      dialog->DefaultCommandIndex = 0;
      dialog->CancelCommandIndex = 1;
      IAsyncOperation<IUICommand^>^ operation = dialog->ShowAsync();
      dialog_operation_ = operation;
      concurrency::create_task(operation).then([this](IUICommand^ command) {
        delete this;
      });
    });
  }

  UICommand^ MakeUICommand(
      const char* id,
      const char* fallback,
      SbSystemPlatformErrorResponse response) {
    ApplicationUwp* app = ApplicationUwp::Get();
    Platform::String^ label = app->GetString(id, fallback);

    return ref new UICommand(label,
      ref new UICommandInvokedHandler(
        [this, response](IUICommand^ command) {
          callback_(response, user_data_);
        }));
  }

  void Clear() {
    ApplicationUwp::Get()->RunInMainThreadAsync([this]() {
      dialog_operation_->Cancel();
    });
  }

 private:
  SbSystemPlatformErrorCallback callback_;
  void* user_data_;
  Platform::Agile<IAsyncOperation<IUICommand^>> dialog_operation_;
};

ref class App sealed : public IFrameworkView {
 public:
  App() : previously_activated_(false), has_internet_access_(false) {}

  // IFrameworkView methods.
  virtual void Initialize(CoreApplicationView^ applicationView) {
    SbAudioSinkPrivate::Initialize();
    CoreApplication::Suspending +=
        ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);
    CoreApplication::Resuming +=
        ref new EventHandler<Object^>(this, &App::OnResuming);
    applicationView->Activated +=
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(
            this, &App::OnActivated);

    has_internet_access_ = HasInternetAccess();

    SB_LOG(INFO) << "Has internet access? " << std::boolalpha
                 << has_internet_access_;

    NetworkInformation::NetworkStatusChanged +=
        ref new NetworkStatusChangedEventHandler(this, &App::OnNetworkChanged);
  }

  void OnNetworkChanged(Platform::Object^ sender) {
    SB_UNREFERENCED_PARAMETER(sender);
    bool has_internet_access = HasInternetAccess();

    if (has_internet_access == has_internet_access_) {
      return;
    }

    SB_LOG(INFO) << "NetworkChanged.  Has internet access? " << std::boolalpha
      << has_internet_access;

    has_internet_access_ = has_internet_access;

    const SbEventType network_event =
        (has_internet_access ? kSbEventTypeNetworkConnect
                             : kSbEventTypeNetworkDisconnect);

    ApplicationUwp::Get()->Inject(
        new ApplicationUwp::Event(network_event, nullptr, nullptr));
  }

  virtual void SetWindow(CoreWindow^ window) {
    ApplicationUwp::Get()->SetCoreWindow(window);
    window->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(
      this, &App::OnKeyUp);
    window->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(
      this, &App::OnKeyDown);
  }
  virtual void Load(Platform::String^ entryPoint) {}
  virtual void Run() {
    main_return_value = application_.Run(
        static_cast<int>(argv_.size()), const_cast<char**>(argv_.data()));
  }
  virtual void Uninitialize() { SbAudioSinkPrivate::TearDown(); }

  void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args) {
    SB_DLOG(INFO) << "Suspending application.";
    // Note if we dispatch "suspend" here before pause, application.cc
    // will inject the "pause" which will cause us to go async which
    // will cause us to not have completed the suspend operation before
    // returning, which UWP requires.
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypePause, NULL, NULL));
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeSuspend, NULL, NULL));
  }

  void OnResuming(Platform::Object^ sender, Platform::Object^ args) {
    SB_DLOG(INFO) << "Resuming";
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeResume, NULL, NULL));
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeUnpause, NULL, NULL));
  }

  void OnKeyUp(CoreWindow^ sender, KeyEventArgs^ args) {
    ApplicationUwp::Get()->OnKeyEvent(sender, args, true);
  }

  void OnKeyDown(CoreWindow^ sender, KeyEventArgs^ args) {
    ApplicationUwp::Get()->OnKeyEvent(sender, args, false);
  }

  void OnActivated(
      CoreApplicationView^ applicationView, IActivatedEventArgs^ args) {
    bool command_line_set = false;

    // Please see application lifecyle description:
    // https://docs.microsoft.com/en-us/windows/uwp/launch-resume/app-lifecycle
    // Note that this document was written for Xaml apps not core apps,
    // so for us the precise API is a little different.
    // The substance is that, while OnActiviated is definitely called the
    // first time the application is started, it may additionally called
    // in other cases while the process is already running. Starboard
    // applications cannot fully restart in a process lifecycle,
    // so we interpret the first activation and the subsequent ones differently.
    if (args->Kind == ActivationKind::Protocol) {
      Uri^ uri = dynamic_cast<IProtocolActivatedEventArgs^>(args)->Uri;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      // The starboard: scheme provides commandline arguments, but that's
      // only allowed during a process's first activation.
      std::string scheme = sbwin32::platformStringToString(uri->SchemeName);

      if (!previously_activated_ && ends_with(scheme, "-starboard")) {
        std::string uri_string = wchar_tToUTF8(uri->RawUri->Data());
        // args_ is a vector of std::string, but argv_ is a vector of
        // char* into args_ so as to compose a char**.
        args_ = ParseStarboardUri(uri_string);
        for (const std::string& arg : args_) {
          argv_.push_back(arg.c_str());
        }

        ApplicationUwp::Get()->SetCommandLine(
            static_cast<int>(argv_.size()), argv_.data());
        command_line_set = true;
      }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      if (uri->SchemeName->Equals("youtube") ||
          uri->SchemeName->Equals("ms-xbl-07459769")) {
        std::string uri_string = sbwin32::platformStringToString(uri->RawUri);

        // Strip the protocol from the uri.
        size_t index = uri_string.find(':');
        if (index != std::string::npos) {
          uri_string = uri_string.substr(index + 1);
        }

        ProcessDeepLinkUri(&uri_string);
      }
    } else if (args->Kind == ActivationKind::DialReceiver) {
      DialReceiverActivatedEventArgs^ dial_args =
          dynamic_cast<DialReceiverActivatedEventArgs^>(args);
      SB_CHECK(dial_args);
      Platform::String^ arguments = dial_args->Arguments;
      if (previously_activated_) {
        std::string uri_string =
          kDialParamPrefix + sbwin32::platformStringToString(arguments);
        ProcessDeepLinkUri(&uri_string);
      } else {
        const char kYouTubeTVurl[] = "--url=https://www.youtube.com/tv?";
        std::string activation_args =
            kYouTubeTVurl + sbwin32::platformStringToString(arguments);
        SB_DLOG(INFO) << "Dial Activation url: " << activation_args;
        args_.push_back(GetArgvZero());
        args_.push_back(activation_args);
        argv_.push_back(args_.front().c_str());
        argv_.push_back(args_.back().c_str());
        ApplicationUwp::Get()->SetCommandLine(static_cast<int>(argv_.size()),
          argv_.data());
        command_line_set = true;
      }
    }
    previous_activation_kind_ = args->Kind;

    if (!previously_activated_) {
      if (!command_line_set) {
        args_.push_back(GetArgvZero());
        argv_.push_back(args_.begin()->c_str());
        ApplicationUwp::Get()->SetCommandLine(
            static_cast<int>(argv_.size()), argv_.data());
      }

      ApplicationUwp* application_uwp = ApplicationUwp::Get();
      CommandLine* command_line =
          ::starboard::shared::uwp::GetCommandLinePointer(application_uwp);
      if (command_line->HasSwitch(kLogPathSwitch)) {
        std::string switch_val = command_line->GetSwitchValue(kLogPathSwitch);
        sbuwp::OpenLogFile(
            Windows::Storage::ApplicationData::Current->LocalCacheFolder,
            switch_val.c_str());
      } else {
#if !defined(COBALT_BUILD_TYPE_GOLD)
        // Log to a file on the last removable device available (probably the
        // most recently added removable device).
        try {
          if (KnownFolders::RemovableDevices != nullptr) {
            concurrency::create_task(
                KnownFolders::RemovableDevices->GetFoldersAsync()).then(
                [](IVectorView<StorageFolder^>^ results) {
                  if (results->Size > 0) {
                    StorageFolder^ folder = results->GetAt(results->Size - 1);
                    Calendar^ now = ref new Calendar();
                    char filename[128];
                    SbStringFormatF(filename, sizeof(filename),
                        "cobalt_log_%04d%02d%02d_%02d%02d%02d.txt",
                        now->Year, now->Month, now->Day,
                        now->Hour + now->FirstHourInThisPeriod,
                        now->Minute, now->Second);
                    sbuwp::OpenLogFile(folder, filename);
                  }
                });
          }
        } catch(Platform::Exception^) {
          SB_LOG(ERROR) << "Unable to open log file in RemovableDevices";
        }
#endif
      }
      SB_LOG(INFO) << "Starting " << GetBinaryName();

      CoreWindow::GetForCurrentThread()->Activate();
      // Call DispatchStart async so the UWP system thinks we're activated.
      // Some tools seem to want the application to be activated before
      // interacting with them, some things are disallowed during activation
      // (such as exiting), and DispatchStart (for example) runs
      // automated tests synchronously.
      ApplicationUwp::Get()->RunInMainThreadAsync([this]() {
        ApplicationUwp::Get()->DispatchStart();
      });
    }
    previously_activated_ = true;
  }
 private:
  void ProcessDeepLinkUri(std::string *uri_string) {
    SB_DCHECK(uri_string);
    if (previously_activated_) {
      std::unique_ptr<Application::Event> event =
        MakeDeepLinkEvent(*uri_string);
      SB_DCHECK(event);
      ApplicationUwp::Get()->Inject(event.release());
    } else {
      SB_DCHECK(!uri_string->empty());
      ApplicationUwp::Get()->SetStartLink(uri_string->c_str());
    }
  }

  bool HasInternetAccess() {
    ConnectionProfile^ connection_profile =
        NetworkInformation::GetInternetConnectionProfile();

    if (connection_profile == nullptr) {
      return false;
    }
    const NetworkConnectivityLevel connectivity_level =
        connection_profile->GetNetworkConnectivityLevel();

    switch (connectivity_level) {
      case NetworkConnectivityLevel::InternetAccess:
      case NetworkConnectivityLevel::ConstrainedInternetAccess:
        return true;
      case NetworkConnectivityLevel::None:
      case NetworkConnectivityLevel::LocalAccess:
        return false;
    }
    SB_NOTREACHED() << "Unknown network connectivity level found "
                    << sbwin32::platformStringToString(
                            connectivity_level.ToString());

    return false;
  }

  bool previously_activated_;
  bool has_internet_access_;
  // Only valid if previously_activated_ is true
  ActivationKind previous_activation_kind_;
  std::vector<std::string> args_;
  std::vector<const char *> argv_;

  starboard::shared::uwp::ApplicationUwp application_;
};

ref class Direct3DApplicationSource sealed : IFrameworkViewSource {
 public:
  Direct3DApplicationSource() {}
  virtual IFrameworkView^ CreateView() {
    return ref new App();
  }
};

namespace starboard {
namespace shared {
namespace uwp {

// If an argv[0] is required, fill it in with the result of
// GetModuleFileName()
std::string GetArgvZero() {
  const size_t kMaxModuleNameSize = SB_FILE_MAX_NAME;
  wchar_t buffer[kMaxModuleNameSize];
  DWORD result = GetModuleFileName(NULL, buffer, kMaxModuleNameSize);
  std::string arg;
  if (result == 0) {
    arg = "unknown";
  } else {
    arg = wchar_tToUTF8(buffer, result).c_str();
  }
  return arg;
}

ApplicationUwp::ApplicationUwp()
    : window_(kSbWindowInvalid),
      localized_strings_(SbSystemGetLocaleId()),
      device_id_(MakeDeviceId()) {
  analog_thumbstick_thread_.reset(new AnalogThumbstickThread(this));
}

ApplicationUwp::~ApplicationUwp() {
  analog_thumbstick_thread_.reset(nullptr);
}

void ApplicationUwp::Initialize() {}

void ApplicationUwp::Teardown() {}

Application::Event* ApplicationUwp::GetNextEvent() {
  SB_NOTREACHED();
  return nullptr;
}

SbWindow ApplicationUwp::CreateWindowForUWP(const SbWindowOptions* options) {
  // TODO: Determine why SB_DCHECK(IsCurrentThread()) fails in nplb, fix it,
  // and add back this check.

  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }

  window_ = new SbWindowPrivate(options);
  return window_;
}

bool ApplicationUwp::DestroyWindow(SbWindow window) {
  // TODO: Determine why SB_DCHECK(IsCurrentThread()) fails in nplb, fix it,
  // and add back this check.

  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }

  SB_DCHECK(window_ == window);
  delete window;
  window_ = kSbWindowInvalid;

  return true;
}

bool ApplicationUwp::DispatchNextEvent() {
  core_window_->Activate();
  core_window_->Dispatcher->ProcessEvents(
      CoreProcessEventsOption::ProcessUntilQuit);
  return false;
}

Windows::Media::Protection::HdcpSession^ ApplicationUwp::GetHdcpSession() {
  if (!hdcp_session_) {
    hdcp_session_ = ref new HdcpSession();
  }
  return hdcp_session_;
}

void ApplicationUwp::ResetHdcpSession() {
  // delete will call the destructor, but not free memory.
  // The destructor is called explicitly so that HDCP session can be
  // torn down immediately.
  if (hdcp_session_) {
    delete hdcp_session_;
    hdcp_session_ = nullptr;
  }
}

void ApplicationUwp::Inject(Application::Event* event) {
  RunInMainThreadAsync([this, event]() {
    bool result = DispatchAndDelete(event);
    if (!result) {
      CoreApplication::Exit();
    }
  });
}

void ApplicationUwp::InjectTimedEvent(Application::TimedEvent* timed_event) {
  SbTimeMonotonic delay_usec =
      timed_event->target_time - SbTimeGetMonotonicNow();
  if (delay_usec < 0) {
    delay_usec = 0;
  }

  // TimeSpan ticks are, like FILETIME, 100ns
  const SbTimeMonotonic kTicksPerUsec = 10;

  TimeSpan timespan;
  timespan.Duration = delay_usec * kTicksPerUsec;

  ScopedLock lock(mutex_);
  ThreadPoolTimer^ timer = ThreadPoolTimer::CreateTimer(
    ref new TimerElapsedHandler([this, timed_event](ThreadPoolTimer^ timer) {
      RunInMainThreadAsync([this, timed_event]() {
        // Even if the event is canceled, the callback can still fire.
        // Thus, the existence of event in timer_event_map_ is used
        // as a source of truth.
        std::size_t number_erased = 0;
        {
          ScopedLock lock(mutex_);
          number_erased = timer_event_map_.erase(timed_event->id);
        }
        if (number_erased > 0) {
          timed_event->callback(timed_event->context);
        }
      });
    }), timespan);
  timer_event_map_.emplace(timed_event->id, timer);
}

void ApplicationUwp::CancelTimedEvent(SbEventId event_id) {
  ScopedLock lock(mutex_);
  auto it = timer_event_map_.find(event_id);
  if (it == timer_event_map_.end()) {
    return;
  }
  it->second->Cancel();
  timer_event_map_.erase(it);
}

Application::TimedEvent* ApplicationUwp::GetNextDueTimedEvent() {
  SB_NOTIMPLEMENTED();
  return nullptr;
}

SbTimeMonotonic ApplicationUwp::GetNextTimedEventTargetTime() {
  SB_NOTIMPLEMENTED();
  return 0;
}

void ApplicationUwp::OnJoystickUpdate(SbKey key, SbInputVector input_vector) {
  scoped_ptr<SbInputData> data(new SbInputData());
  SbMemorySet(data.get(), 0, sizeof(*data));
  data->window = window_;
  data->type = kSbInputEventTypeMove;
  data->device_type = kSbInputDeviceTypeGamepad;
  data->device_id = device_id();
  data->key = key;
  data->character = 0;

  data->key_modifiers = kSbKeyModifiersNone;
  data->position = input_vector;

  SbKeyLocation key_location = kSbKeyLocationUnspecified;
  switch (key) {
    case kSbKeyGamepadLeftStickLeft:
    case kSbKeyGamepadLeftStickUp: {
      key_location = kSbKeyLocationLeft;
      break;
    }
    case kSbKeyGamepadRightStickLeft:
    case kSbKeyGamepadRightStickUp: {
      key_location = kSbKeyLocationRight;
    }
    default: {
      SB_NOTREACHED();
      break;
    }
  }

  data->key_location = key_location;
  Inject(new Event(kSbEventTypeInput, data.release(),
                   &Application::DeleteDestructor<SbInputData>));
}

SbSystemPlatformError ApplicationUwp::OnSbSystemRaisePlatformError(
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data) {
  return new SbSystemPlatformErrorPrivate(type, callback, user_data);
}

void ApplicationUwp::OnSbSystemClearPlatformError(
    SbSystemPlatformError handle) {
  if (handle == kSbSystemPlatformErrorInvalid) {
    return;
  }
  static_cast<SbSystemPlatformErrorPrivate*>(handle)->Clear();
}

Platform::String^ ApplicationUwp::GetString(
    const char* id, const char* fallback) const {
  return stringToPlatformString(localized_strings_.GetString(id, fallback));
}

bool ApplicationUwp::IsHdcpOn() {
  ::starboard::ScopedLock lock(hdcp_session_mutex_);

  return GetHdcpSession()->IsEffectiveProtectionAtLeast(kHDCPProtectionMode);
}

bool ApplicationUwp::TurnOnHdcp() {
  HdcpSetProtectionResult protection_result;
  {
    ::starboard::ScopedLock lock(hdcp_session_mutex_);

    protection_result = WaitForResult(
        GetHdcpSession()->SetDesiredMinProtectionAsync(kHDCPProtectionMode));
  }

  if (IsHdcpOn()) {
    return true;
  }

  // If the operation did not have intended result, log something.
  switch (protection_result) {
  case HdcpSetProtectionResult::Success:
    SB_LOG(INFO) << "Successfully set HDCP.";
    break;
  case HdcpSetProtectionResult::NotSupported:
    SB_LOG(INFO) << "HDCP is not supported.";
    break;
  case HdcpSetProtectionResult::TimedOut:
    SB_LOG(INFO) << "Setting HDCP timed out.";
    break;
  case HdcpSetProtectionResult::UnknownFailure:
    SB_LOG(INFO) << "Unknown failure returned while setting HDCP.";
    break;
  }

  return false;
}

bool ApplicationUwp::TurnOffHdcp() {
  {
    ::starboard::ScopedLock lock(hdcp_session_mutex_);
    ResetHdcpSession();
  }
  bool success = !SbMediaIsOutputProtected();
  return success;
}

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^ args) {
  if (!IsDebuggerPresent()) {
    // By default, a Windows application will display a dialog box
    // when it crashes. This is extremely undesirable when run offline.
    // The following configures messages to be print to the console instead.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  }

  WSAData wsaData;
  int init_result = WSAStartup(
      MAKEWORD(kWinSockVersionMajor, kWinSockVersionMajor), &wsaData);

  SB_CHECK(init_result == 0);
  // WSAStartup returns the highest version that is supported up to the version
  // we request.
  SB_CHECK(LOBYTE(wsaData.wVersion) == kWinSockVersionMajor &&
           HIBYTE(wsaData.wVersion) == kWinSockVersionMinor);

  HRESULT hr = MFStartup(MF_VERSION);
  SB_DCHECK(SUCCEEDED(hr));

  starboard::shared::win32::RegisterMainThread();

  auto direct3DApplicationSource = ref new Direct3DApplicationSource();
  CoreApplication::Run(direct3DApplicationSource);

  MFShutdown();
  WSACleanup();

  return main_return_value;
}
