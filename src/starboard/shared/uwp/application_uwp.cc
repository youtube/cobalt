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

#include <D3D11.h>
#include <WinSock2.h>
#include <mfapi.h>
#include <ppltasks.h>
#include <windows.h>
#include <windows.system.display.h>

#include <memory>
#include <string>
#include <vector>

#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/input.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/uwp/analog_thumbstick_input_thread.h"
#include "starboard/shared/uwp/app_accessors.h"
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/uwp/log_file_impl.h"
#include "starboard/shared/uwp/watchdog_log.h"
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
using starboard::shared::uwp::RunInMainThreadAsync;
using starboard::shared::uwp::WaitForResult;
using starboard::shared::win32::stringToPlatformString;
using starboard::shared::win32::wchar_tToUTF8;
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
using Windows::Foundation::Metadata::ApiInformation;
using Windows::Foundation::TimeSpan;
using Windows::Foundation::TypedEventHandler;
using Windows::Foundation::Uri;
using Windows::Graphics::Display::Core::HdmiDisplayInformation;
using Windows::Graphics::Display::DisplayInformation;
using Windows::Globalization::Calendar;
using Windows::Media::Protection::HdcpProtection;
using Windows::Media::Protection::HdcpSession;
using Windows::Media::Protection::HdcpSetProtectionResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestStatus;
using Windows::Security::Credentials::WebAccountProvider;
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
using Windows::UI::ViewManagement::ApplicationView;

namespace {

// Per Microsoft, HdcpProtection::On means HDCP 1.x required.
const HdcpProtection kHDCPProtectionMode = HdcpProtection::On;

const int kWinSockVersionMajor = 2;
const int kWinSockVersionMinor = 2;

const char kDialParamPrefix[] = "cobalt-dial:?";
const char kLogPathSwitch[] = "xb1_log_file";
// A special log that the app will periodically write to. This allows
// tests to determine if the app is still alive.
const char kWatchDogLog[] = "xb1_watchdog_log";
const char kStarboardArgumentsPath[] = "arguments\\starboard_arguments.txt";
const int64_t kMaxArgumentFileSizeBytes = 4 * 1024 * 1024;

// Filename, placed in the cache dir, whose presence indicates
// that the first run has happened.
const char kFirstRunFilename[] = "first_run";
// The entry point used by the main app.
const char kMainAppDefaultEntryPoint[] = "https://www.youtube.com/tv";
// The URL to be used for the main app on the first run.
const char kMainAppFirstRunUrl[]
    = "https://www.youtube.com/api/xbox/cobalt_bootstrap";

int main_return_value = 0;

// IDisplayRequest is both "non-agile" and apparently
// incompatible with Platform::Agile (it doesn't fully implement
// a thread marshaller). We must neither use ComPtr or Platform::Agile
// here. We manually create, access release on the main app thread only.
ABI::Windows::System::Display::IDisplayRequest* display_request = nullptr;

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

void SplitArgumentsIntoVector(std::string* args,
                              std::vector<std::string> *result) {
  SB_DCHECK(args);
  SB_DCHECK(result);
  while (!args->empty()) {
    size_t next = args->find(';');
    result->push_back(args->substr(0, next));
    if (next == std::string::npos) {
      return;
    }
    *args = args->substr(next + 1);
  }
}

// Parses a starboard: URI scheme by splitting args at ';' boundaries.
std::vector<std::string> ParseStarboardUri(const std::string& uri) {
  std::vector<std::string> result;
  result.push_back(GetArgvZero());

  size_t index = uri.find(':');
  if (index == std::string::npos) {
    return result;
  }

  std::string args = uri.substr(index + 1);
  SplitArgumentsIntoVector(&args, &result);

  return result;
}

void AddArgumentsFromFile(const char* path,
  std::vector<std::string>* args) {
  starboard::ScopedFile file(path, kSbFileOpenOnly | kSbFileRead);
  if (!file.IsValid()) {
    SB_LOG(INFO) << path << " is not valid for arguments.";
    return;
  }

  int64_t file_size = file.GetSize();
  if (file_size > kMaxArgumentFileSizeBytes) {
    SB_DLOG(ERROR) << "The arguments file is too big.";
    return;
  }

  if (file_size <= 0) {
    SB_DLOG(INFO) << "Arguments file is empty.";
    return;
  }

  std::string argument_string(file_size, '\0');
  int return_value = file.ReadAll(&argument_string[0], file_size);
  if (return_value < 0) {
    SB_DLOG(ERROR) << "Error while reading arguments from file.";
    return;
  }
  argument_string.resize(return_value);

  SplitArgumentsIntoVector(&argument_string, args);
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

// Returns true of this is the app's first run on a given device.
bool IsFirstRun() {
  char path[SB_FILE_MAX_PATH];
  bool success = SbSystemGetPath(kSbSystemPathCacheDirectory,
                                 path, SB_FILE_MAX_PATH);
  SB_DCHECK(success);
  std::string file_path(path);
  file_path.append(1, '/');
  file_path.append(kFirstRunFilename);

  if (SbFileExists(file_path.c_str())) {
    return false;
  }

  bool created;
  SbFile file = SbFileOpen(file_path.c_str(),
      kSbFileRead | kSbFileOpenAlways, &created, nullptr);
  SbFileClose(file);

  return true;
}
}  // namespace

ref class App sealed : public IFrameworkView {
 public:
  App() : previously_activated_(false),
          first_run_(IsFirstRun()) {}

  // IFrameworkView methods.
  virtual void Initialize(CoreApplicationView^ application_view) {
    // The following incantation creates a DisplayRequest and obtains
    // it's underlying COM interface.
    ComPtr<IInspectable> inspectable = reinterpret_cast<IInspectable*>(
        ref new Windows::System::Display::DisplayRequest());
    ComPtr<ABI::Windows::System::Display::IDisplayRequest> dr;
    inspectable.As(&dr);
    display_request = dr.Detach();

    SbAudioSinkPrivate::Initialize();
    CoreApplication::Suspending +=
        ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);
    CoreApplication::Resuming +=
        ref new EventHandler<Object^>(this, &App::OnResuming);
    application_view->Activated +=
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(
            this, &App::OnActivated);
  }

  virtual void SetWindow(CoreWindow^ window) {
    ApplicationUwp::Get()->SetCoreWindow(window);
    window->KeyUp += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(
      this, &App::OnKeyUp);
    window->KeyDown += ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(
      this, &App::OnKeyDown);
  }

  virtual void Load(Platform::String^ entry_point) {
    entry_point_ = wchar_tToUTF8(entry_point->Data());
  }

  virtual void Run() {
    main_return_value = application_.Run(
        static_cast<int>(argv_.size()), const_cast<char**>(argv_.data()));
  }
  virtual void Uninitialize() {
    SbAudioSinkPrivate::TearDown();
    display_request->Release();
    display_request = nullptr;
  }

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

#pragma warning(push)
#pragma warning(disable: 4451)  // False-positive Platform::Agile warning
  void OnActivated(
      CoreApplicationView^ application_view, IActivatedEventArgs^ args) {
    SB_LOG(INFO) << "OnActivated first run:" << first_run_;
    if (!first_run_) {
      FinishActivated(application_view, args, entry_point_);
    } else {
      sbuwp::TryToFetchSsoToken(kMainAppFirstRunUrl).then(
          [this, application_view, args](
            concurrency::task<WebTokenRequestResult^> previous_task) {
        bool success = false;
        try {
          WebTokenRequestResult^ result = previous_task.get();
          success = result &&
              (result->ResponseStatus == WebTokenRequestStatus::Success);
        } catch(Platform::Exception^) {
          SB_LOG(INFO) << "Exception during RequestTokenAsync";
        }
        // It seems like it should be able to specify a task_contination_context
        // such that we don't need to additionally do RunInMainThreadAsync here,
        // however obvious solutions seem to cause this to run in background
        // threads.
        SB_LOG(INFO) << "TryToFetchSsoToken success:" << success;
        RunInMainThreadAsync([this, application_view, args, success]() {
          std::string start_url = entry_point_;
          if (success && start_url == kMainAppDefaultEntryPoint) {
            SB_LOG(INFO) << "Using special main app first-run entry point.";
            start_url = kMainAppFirstRunUrl;
          }
          FinishActivated(application_view, args, start_url);
        });
      });
    }
  }
#pragma warning(pop)

 private:
  void FinishActivated(
      CoreApplicationView^ application_view, IActivatedEventArgs^ args,
      const std::string& start_url) {
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
        std::string activation_args = "--url=";
        activation_args.append(start_url);
        activation_args.append("?");
        activation_args.append(sbwin32::platformStringToString(arguments));
        SB_DLOG(INFO) << "Dial Activation url: " << activation_args;
        args_.push_back(GetArgvZero());
        args_.push_back(activation_args);
        // Set partition URL in case start_url is the main app first run
        // special case.
        std::string partition_arg = "--local_storage_partition_url=";
        partition_arg.append(entry_point_);
        args_.push_back(partition_arg);
        for (const std::string& arg : args_) {
          argv_.push_back(arg.c_str());
        }
        ApplicationUwp::Get()->SetCommandLine(static_cast<int>(argv_.size()),
          argv_.data());
        command_line_set = true;
      }
    }
    previous_activation_kind_ = args->Kind;

    if (!previously_activated_) {
      if (!command_line_set) {
        args_.push_back(GetArgvZero());
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
        char content_directory[SB_FILE_MAX_NAME];
        content_directory[0] = '\0';
        if (SbSystemGetPath(kSbSystemPathContentDirectory,
                            content_directory,
                            SB_ARRAY_SIZE_INT(content_directory))) {
          std::string arguments_file_path = content_directory;
          arguments_file_path += SB_FILE_SEP_STRING;
          arguments_file_path += kStarboardArgumentsPath;
          AddArgumentsFromFile(arguments_file_path.c_str(), &args_);
        }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

        std::string start_url_arg = "--url=";
        start_url_arg.append(start_url);
        args_.push_back(start_url_arg);
        std::string partition_arg = "--local_storage_partition_url=";
        partition_arg.append(entry_point_);
        args_.push_back(partition_arg);
        for (auto& arg : args_) {
          argv_.push_back(arg.c_str());
        }

        ApplicationUwp::Get()->SetCommandLine(
            static_cast<int>(argv_.size()), argv_.data());
      }

      ApplicationUwp* application_uwp = ApplicationUwp::Get();
      const CommandLine* command_line =
          ::starboard::shared::uwp::GetCommandLinePointer(application_uwp);

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      if (command_line->HasSwitch(kWatchDogLog)) {
        // Launch a thread.
        std::string switch_val = command_line->GetSwitchValue(kWatchDogLog);
        auto uwp_dir =
            Windows::Storage::ApplicationData::Current->LocalCacheFolder;
        std::stringstream ss;
        ss << sbwin32::platformStringToString(uwp_dir->Path) << "/"
           << switch_val;
        sbuwp::StartWatchdogLog(ss.str());
      }

      if (command_line->HasSwitch(kLogPathSwitch)) {
        std::string switch_val = command_line->GetSwitchValue(kLogPathSwitch);
        sbuwp::OpenLogFile(
            Windows::Storage::ApplicationData::Current->LocalCacheFolder,
            switch_val.c_str());
      } else {
#if !defined(COBALT_BUILD_TYPE_GOLD)
        // Log to a file on the last removable device available (probably the
        // most recently added removable device).
        if (KnownFolders::RemovableDevices != nullptr) {
          concurrency::create_task(
              KnownFolders::RemovableDevices->GetFoldersAsync()).then(
              [](concurrency::task<IVectorView<StorageFolder^>^> result) {
                IVectorView<StorageFolder^>^ results;
                try {
                  results = result.get();
                } catch(Platform::Exception^) {
                  SB_LOG(ERROR) <<
                      "Unable to open log file in RemovableDevices";
                  return;
                }

                if (results->Size == 0) {
                  return;
                }

                StorageFolder^ folder = results->GetAt(results->Size - 1);
                Calendar^ now = ref new Calendar();
                char filename[128];
                SbStringFormatF(filename, sizeof(filename),
                    "cobalt_log_%04d%02d%02d_%02d%02d%02d.txt",
                    now->Year, now->Month, now->Day,
                    now->Hour + now->FirstHourInThisPeriod,
                    now->Minute, now->Second);
                sbuwp::OpenLogFile(folder, filename);
              });
        }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
      }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      SB_LOG(INFO) << "Starting " << GetBinaryName();

      CoreWindow::GetForCurrentThread()->Activate();
      // Call DispatchStart async so the UWP system thinks we're activated.
      // Some tools seem to want the application to be activated before
      // interacting with them, some things are disallowed during activation
      // (such as exiting), and DispatchStart (for example) runs
      // automated tests synchronously.

      RunInMainThreadAsync([this]() {
        ApplicationUwp::Get()->DispatchStart();
      });
    }
    previously_activated_ = true;
  }

  void ProcessDeepLinkUri(std::string *uri_string) {
    SB_DCHECK(uri_string);
    if (previously_activated_) {
      std::unique_ptr<Application::Event> event =
        MakeDeepLinkEvent(*uri_string);
      SB_DCHECK(event);
      ApplicationUwp::Get()->Inject(event.release());
    } else {
      ApplicationUwp::Get()->SetStartLink(uri_string->c_str());
    }
  }

  std::string entry_point_;
  bool previously_activated_;
  bool first_run_;
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

void ApplicationUwp::Teardown() {
  CloseWatchdogLog();
}

Application::Event* ApplicationUwp::GetNextEvent() {
  SB_NOTREACHED();
  return nullptr;
}

SbWindow ApplicationUwp::CreateWindowForUWP(const SbWindowOptions*) {
  // TODO: Determine why SB_DCHECK(IsCurrentThread()) fails in nplb, fix it,
  // and add back this check.

  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }

  // Get the logical resolution in pixels. See section "Bounds" in
  // https://docs.microsoft.com/en-us/uwp/api/windows.ui.core.corewindow.
  Windows::Foundation::Rect bounds_in_dips =
      CoreWindow::GetForCurrentThread()->Bounds;
  float dpi = DisplayInformation::GetForCurrentView()->LogicalDpi;
  int width = static_cast<int>(bounds_in_dips.Width * dpi / 96.0f);
  int height = static_cast<int>(bounds_in_dips.Height * dpi / 96.0f);

  // For UWP on XB1, the logical resolution is always 1080p, regardless of the
  // actual output resolution -- section "Scale factor and adaptive layout" in
  // "https://docs.microsoft.com/en-us/windows/uwp/input-and-devices/
  // designing-for-tv". However, if the swap chain uses a special surface
  // format (R10G10B10A2), it can be passed to the output without scaling.
  bool supports_hdmi_api = ApiInformation::IsApiContractPresent(
      "Windows.Foundation.UniversalApiContract", 4);
  bool is_fullscreen = ApplicationView::GetForCurrentView()->IsFullScreenMode;
  if (supports_hdmi_api && is_fullscreen) {
    // This reports the actual output resolution.
    auto display_info = HdmiDisplayInformation::GetForCurrentView();
    auto current_mode = display_info->GetCurrentDisplayMode();
    width = static_cast<int>(current_mode->ResolutionWidthInRawPixels);
    height = static_cast<int>(current_mode->ResolutionHeightInRawPixels);
  }

  SB_LOG(INFO) << "Window resolution is " << width << " x " << height;

  window_ = new SbWindowPrivate(width, height);
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
      break;
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

Platform::Agile<Windows::UI::Core::CoreDispatcher> GetDispatcher() {
  return ApplicationUwp::Get()->GetDispatcher();
}

Platform::Agile<Windows::Media::SystemMediaTransportControls>
    GetTransportControls() {
  return ApplicationUwp::Get()->GetTransportControls();
}

void DisplayRequestActive() {
  RunInMainThreadAsync([]() {
    if (display_request != nullptr) {
      display_request->RequestActive();
    }
  });
}

void DisplayRequestRelease() {
  RunInMainThreadAsync([]() {
    if (display_request != nullptr) {
      display_request->RequestRelease();
    }
  });
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
