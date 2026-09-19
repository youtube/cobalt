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

#include "starboard/shared/uwp/application_uwp.h"

#include <D3D11.h>
#include <D3D11_1.h>
#include <WinSock2.h>
#include <collection.h>
#include <mfapi.h>
#include <ppltasks.h>
#include <windows.graphics.display.core.h>
#include <windows.h>
#include <windows.system.display.h>
#include <windows.applicationmodel.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <fstream>

#include "starboard/common/device_type.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/queue.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/string.h"
#include "starboard/common/system_property.h"
#include "starboard/common/thread.h"
#include "starboard/configuration_constants.h"
#include "starboard/event.h"
#include "starboard/input.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "starboard/shared/starboard/net_args.h"
#include "starboard/shared/starboard/net_log.h"
#include "starboard/shared/uwp/analog_thumbstick_input_thread.h"
#include "starboard/shared/uwp/app_accessors.h"
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/uwp/extended_resources_manager.h"
#include "starboard/shared/uwp/log_file_impl.h"
#include "starboard/shared/uwp/watchdog_log.h"
#include "starboard/shared/uwp/window_internal.h"
#include "starboard/shared/win32/thread_private.h"
#include "starboard/shared/win32/video_decoder.h"
#include "starboard/shared/win32/wchar_utils.h"
#include "starboard/shared/win32/directory_internal.h"
#include "starboard/system.h"

namespace starboard {

using Microsoft::WRL::ComPtr;
using shared::starboard::Application;
using shared::starboard::CommandLine;
using shared::starboard::kNetArgsCommandSwitchWait;
using shared::starboard::kNetLogCommandSwitchWait;
using shared::starboard::NetArgsWaitForPayload;
using shared::starboard::NetLogFlushThenClose;
using shared::starboard::NetLogWaitForClientConnected;
using shared::uwp::ApplicationUwp;
using shared::uwp::RunInMainThreadAsync;
using shared::uwp::WaitForResult;
using shared::win32::platformStringToString;
using shared::win32::stringToPlatformString;
using shared::win32::wchar_tToUTF8;
using ::starboard::shared::starboard::media::KeySystemSupportabilityCache;
using ::starboard::shared::starboard::media::MimeSupportabilityCache;
using Windows::ApplicationModel::SuspendingDeferral;
using Windows::ApplicationModel::SuspendingEventArgs;
using Windows::ApplicationModel::Activation::ActivationKind;
using Windows::ApplicationModel::Activation::DialReceiverActivatedEventArgs;
using Windows::ApplicationModel::Activation::IActivatedEventArgs;
using Windows::ApplicationModel::Activation::IProtocolActivatedEventArgs;
using Windows::ApplicationModel::Core::CoreApplication;
using Windows::ApplicationModel::Core::CoreApplicationView;
using Windows::ApplicationModel::Core::IFrameworkView;
using Windows::ApplicationModel::Core::IFrameworkViewSource;
using Windows::ApplicationModel::ExtendedExecution::ExtendedExecutionReason;
using Windows::ApplicationModel::ExtendedExecution::ExtendedExecutionResult;
using Windows::ApplicationModel::ExtendedExecution::
    ExtendedExecutionRevokedEventArgs;
using Windows::ApplicationModel::ExtendedExecution::ExtendedExecutionSession;
using Windows::ApplicationModel::Package;
using Windows::ApplicationModel::PackageVersion;
using Windows::Devices::Enumeration::DeviceInformation;
using Windows::Devices::Enumeration::DeviceInformationUpdate;
using Windows::Devices::Enumeration::DeviceWatcher;
using Windows::Devices::Enumeration::DeviceWatcherStatus;
using Windows::Foundation::EventHandler;
using Windows::Foundation::IAsyncOperation;
using Windows::Foundation::TimeSpan;
using Windows::Foundation::TypedEventHandler;
using Windows::Foundation::Uri;
using Windows::Foundation::Collections::IVectorView;
using Windows::Foundation::Metadata::ApiInformation;
using Windows::Globalization::Calendar;
using Windows::Graphics::Display::AdvancedColorInfo;
using Windows::Graphics::Display::AdvancedColorKind;
using Windows::Graphics::Display::DisplayInformation;
using Windows::Graphics::Display::HdrMetadataFormat;
using Windows::Graphics::Display::Core::HdmiDisplayColorSpace;
using Windows::Graphics::Display::Core::HdmiDisplayHdr2086Metadata;
using Windows::Graphics::Display::Core::HdmiDisplayHdrOption;
using Windows::Graphics::Display::Core::HdmiDisplayInformation;
using Windows::Graphics::Display::Core::HdmiDisplayMode;
using Windows::Media::Protection::HdcpProtection;
using Windows::Media::Protection::HdcpSession;
using Windows::Media::Protection::HdcpSetProtectionResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestResult;
using Windows::Security::Authentication::Web::Core::WebTokenRequestStatus;
using Windows::Security::Credentials::WebAccountProvider;
using Windows::Storage::ApplicationData;
using Windows::Storage::FileAttributes;
using Windows::Storage::KnownFolders;
using Windows::Storage::StorageFile;
using Windows::Storage::StorageFolder;
using Windows::System::UserAuthenticationStatus;
using Windows::System::Threading::ThreadPoolTimer;
using Windows::System::Threading::TimerElapsedHandler;
using Windows::UI::Core::CoreDispatcherPriority;
using Windows::UI::Core::CoreProcessEventsOption;
using Windows::UI::Core::CoreWindow;
using Windows::UI::Core::DispatchedHandler;
using Windows::UI::Core::KeyEventArgs;
using Windows::UI::ViewManagement::ApplicationView;
using Windows::UI::ViewManagement::ApplicationViewScaling;

namespace {

const Platform::String ^ kGenericPnpMonitorAqs = ref new Platform::String(
    L"System.Devices.InterfaceClassGuid:=\"{e6f07b5f-ee97-4a90-b076-"
    L"33f57bf4eaa7}\" AND "
    L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#"
    L"True");

const uint32_t kYuv420BitsPerPixelForHdr10Mode = 24;
const uint32_t kHdr4kRefreshRateMaximum = 60;
const uint32_t k4kResolutionWidth = 3840;
const uint32_t k4kResolutionHeight = 2160;

// The number of seconds to wait after requesting application stop. This is used
// to exit the app when a suspend has been requested. This is a temporary
// behavior that should be removed when not needed anymore.
const SbTime kAppExitWaitTime = kSbTimeSecond * 30;

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

int main_return_value = 0;

// IDisplayRequest is both "non-agile" and apparently
// incompatible with Platform::Agile (it doesn't fully implement
// a thread marshaller). We must neither use ComPtr or Platform::Agile
// here. We manually create, access release on the main app thread only.
ABI::Windows::System::Display::IDisplayRequest* display_request = nullptr;

// If an argv[0] is required, fill it in with the result of
// GetModuleFileName()
std::string GetArgvZero() {
  const size_t kMaxModuleNameSize = kSbFileMaxName;
  std::vector<wchar_t> buffer(kMaxModuleNameSize);
  DWORD result = GetModuleFileName(NULL, buffer.data(), buffer.size());
  std::string arg;
  if (result == 0) {
    arg = "unknown";
  } else {
    arg = wchar_tToUTF8(buffer.data(), result).c_str();
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
                              std::vector<std::string>* result) {
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

void AddArgumentsFromFile(const char* path, std::vector<std::string>* args) {
  ScopedFile file(path, kSbFileOpenOnly | kSbFileRead);
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

void TryAddCommandArgsFromStarboardFile(std::vector<std::string>* args) {
  std::vector<char> content_directory(kSbFileMaxName);
  content_directory[0] = '\0';

  if (!SbSystemGetPath(kSbSystemPathContentDirectory, content_directory.data(),
                       content_directory.size())) {
    return;
  }

  std::string arguments_file_path(static_cast<char*>(content_directory.data()));
  arguments_file_path += kSbFileSepString;
  arguments_file_path += kStarboardArgumentsPath;

  AddArgumentsFromFile(arguments_file_path.c_str(), args);
}

void AddCommandArgsFromNetArgs(SbTime timeout, std::vector<std::string>* args) {
  // Detect if NetArgs is enabled for this run. If so then receive and
  // then merge the arguments into this run.
  SB_LOG(INFO) << "Waiting for net args...";
  std::vector<std::string> net_args = NetArgsWaitForPayload(timeout);
  if (!net_args.empty()) {
    std::stringstream ss;
    ss << "Found Net Args:\n";
    for (const std::string& s : net_args) {
      ss << "  " << s << "\n";
    }
    SB_LOG(INFO) << ss.str();
  }
  // Merge command arguments.
  args->insert(args->end(), net_args.begin(), net_args.end());
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
  starboard::strlcpy(deep_link, uri_string.c_str(), kBufferSize);

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
  std::string::size_type index = full_binary_path.rfind(kSbFileSepChar);
  if (index == std::string::npos) {
    return full_binary_path;
  }

  return full_binary_path.substr(index + 1);
}

void OnDeviceAdded(DeviceWatcher ^, DeviceInformation ^) {
  SB_LOG(INFO) << "DisplayStatusWatcher::OnDeviceAdded";
  // We need delay to give time for the display initializing after connect.
  SbThreadSleep(15 * kSbTimeMillisecond);

  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();

  ApplicationUwp::Get()->Inject(
      new ApplicationUwp::Event(kSbEventTypeUnfreeze, NULL, NULL));
  ApplicationUwp::Get()->Inject(
      new ApplicationUwp::Event(kSbEventTypeReveal, NULL, NULL));
  ApplicationUwp::Get()->Inject(
      new ApplicationUwp::Event(kSbEventTypeFocus, NULL, NULL));
}

void OnDeviceRemoved(DeviceWatcher ^, DeviceInformationUpdate ^) {
  // Without signing on OnDeviceRemoved, callback OnDeviceAdded doesn't work.
  SB_LOG(INFO) << "DisplayStatusWatcher::OnDeviceRemoved";

  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();

  ApplicationUwp::Get()->Inject(
      new ApplicationUwp::Event(kSbEventTypeBlur, NULL, NULL));
  ApplicationUwp::Get()->Inject(
      new ApplicationUwp::Event(kSbEventTypeConceal, NULL, NULL));
  ApplicationUwp::Get()->Inject(
      new ApplicationUwp::Event(kSbEventTypeFreeze, NULL, NULL));
}

}  // namespace

namespace shared {
namespace win32 {
// Called into drm_system_playready.cc
extern void DrmSystemOnUwpResume();
}  // namespace win32
}  // namespace shared

ref class App sealed : public IFrameworkView {
 public:
  App(SbTimeMonotonic start_time)
      : application_start_time_{start_time},
        previously_activated_(false),
#if SB_API_VERSION >= 15
        application_(SbEventHandle),
#endif  // SB_API_VERSION >= 15
        is_online_(true) {
  }

  // IFrameworkView methods.
  virtual void Initialize(CoreApplicationView ^ application_view) {
    // The following incantation creates a DisplayRequest and obtains
    // its underlying COM interface.
    ComPtr<IInspectable> inspectable = reinterpret_cast<IInspectable*>(
        ref new Windows::System::Display::DisplayRequest());
    ComPtr<ABI::Windows::System::Display::IDisplayRequest> dr;
    inspectable.As(&dr);
    display_request = dr.Detach();

    SbAudioSinkPrivate::Initialize();
    Windows::Networking::Connectivity::NetworkInformation::
        NetworkStatusChanged += ref new Windows::Networking::Connectivity::
            NetworkStatusChangedEventHandler(this,
                                             &App::OnNetworkStatusChanged);
    CoreApplication::Suspending +=
        ref new EventHandler<SuspendingEventArgs ^>(this, &App::OnSuspending);
    CoreApplication::Resuming +=
        ref new EventHandler<Object ^>(this, &App::OnResuming);
    application_view->Activated +=
        ref new TypedEventHandler<CoreApplicationView ^, IActivatedEventArgs ^>(
            this, &App::OnActivated);

    MimeSupportabilityCache::GetInstance()->SetCacheEnabled(true);
    KeySystemSupportabilityCache::GetInstance()->SetCacheEnabled(true);
  }

  virtual void SetWindow(CoreWindow ^ window) {
    ApplicationUwp::Get()->SetCoreWindow(window);
    window->KeyUp += ref new TypedEventHandler<CoreWindow ^, KeyEventArgs ^>(
        this, &App::OnKeyUp);
    window->KeyDown += ref new TypedEventHandler<CoreWindow ^, KeyEventArgs ^>(
        this, &App::OnKeyDown);
  }

  virtual void Load(Platform::String ^ entry_point) {
    entry_point_ = wchar_tToUTF8(entry_point->Data());
  }

  virtual void Run() {
#if SB_API_VERSION >= 15
    main_return_value =
        SbRunStarboardMain(static_cast<int>(argv_.size()),
                           const_cast<char**>(argv_.data()), SbEventHandle);
#else
    main_return_value = application_.Run(static_cast<int>(argv_.size()),
                                         const_cast<char**>(argv_.data()));
#endif  // SB_API_VERSION >= 15
  }
  virtual void Uninitialize() {
    SbAudioSinkPrivate::TearDown();
    display_request->Release();
    display_request = nullptr;
  }

  void CompleteSuspendDeferral() {
    if (suspend_deferral_ != nullptr) {
      // Completing the deferral results in the app being suspended by the OS.
      SB_LOG(INFO) << "App is ready to be suspended.";
      suspend_deferral_->Complete();
      suspend_deferral_ = nullptr;
    }
  }

  void ExtendedExecutionSessionRevoked(Object ^ sender,
                                       ExtendedExecutionRevokedEventArgs ^
                                           args) {
    CompleteSuspendDeferral();
  }

  void ForceQuit() {
    SB_LOG(ERROR) << "Application is not safe to suspend, forcing exit.";
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeStop, NULL, NULL));
    auto extended_resources_manager =
        shared::uwp::ExtendedResourcesManager::GetInstance();
    if (extended_resources_manager) {
      extended_resources_manager->Quit();
    }
  }

  void OnNetworkStatusChanged(Object ^ sender) {
    auto connection_profile = Windows::Networking::Connectivity::
        NetworkInformation::GetInternetConnectionProfile();
    bool is_online =
        connection_profile &&
        connection_profile->GetNetworkConnectivityLevel() !=
            Windows::Networking::Connectivity::NetworkConnectivityLevel::None;

    // Only inject event if the online status changed.
    if (is_online != is_online_) {
      auto* application = starboard::shared::starboard::Application::Get();

      // Verify we have an application and dispatcher before injecting events.
      if (application == nullptr) {
        SB_LOG(ERROR) << "OnNetworkStatusChanged has no application.";
        SB_DCHECK(false);
        return;
      }
      if (starboard::shared::uwp::GetDispatcher().Get() == nullptr) {
        SB_LOG(ERROR) << "OnNetworkStatusChanged has no dispatcher.";
        SB_DCHECK(false);
        return;
      }

      if (is_online) {
        application->InjectOsNetworkConnectedEvent();
      } else {
        application->InjectOsNetworkDisconnectedEvent();
      }
      is_online_ = is_online;
    }
  }

  void OnSuspending(Platform::Object ^ sender, SuspendingEventArgs ^ args) {
    // Request deferral of the suspending operation. This ensures that the app
    // does not immediately get suspended when this function returns.
    suspend_deferral_ = args->SuspendingOperation->GetDeferral();

    auto extended_resources_manager =
        shared::uwp::ExtendedResourcesManager::GetInstance();
    bool is_safe_to_suspend = extended_resources_manager->IsSafeToSuspend();

    // Request extended execution during which we will suspend the app and
    // release extended resources.
    auto session = ref new ExtendedExecutionSession();
    session->Reason = ExtendedExecutionReason::SavingData;
    session->Description = "Suspending...";

    // If the extended execution session gets revoked, we have to complete
    // the deferral.
    Windows::Foundation::EventRegistrationToken revoked_token =
        session->Revoked +=
        ref new TypedEventHandler<Object ^,
                                  ExtendedExecutionRevokedEventArgs ^>(
            this, &App::ExtendedExecutionSessionRevoked);
    Concurrency::create_task(session->RequestExtensionAsync())
        .then([this, is_safe_to_suspend,
               extended_resources_manager](ExtendedExecutionResult result) {
          // Suspend the app and release extended resources during the extended
          // session.

          // Note if we dispatch "suspend" here before pause, application.cc
          // will inject the "pause" which will cause us to go async which
          // will cause us to not have completed the suspend operation before
          // returning, which UWP requires.
          ApplicationUwp::Get()->DispatchAndDelete(
              new ApplicationUwp::Event(kSbEventTypeBlur, NULL, NULL));
          ApplicationUwp::Get()->DispatchAndDelete(
              new ApplicationUwp::Event(kSbEventTypeConceal, NULL, NULL));
          ApplicationUwp::Get()->DispatchAndDelete(
              new ApplicationUwp::Event(kSbEventTypeFreeze, NULL, NULL));
          extended_resources_manager->ReleaseExtendedResources();
          if (!extended_resources_manager->IsSafeToSuspend()) {
            ForceQuit();
          }
        })
        .then([this, is_safe_to_suspend, session, revoked_token]() {
          // The extended session has completed, we are ready to be suspended
          // now.
          session->Revoked -= revoked_token;
          delete session;

          CompleteSuspendDeferral();
        });
    if (!is_safe_to_suspend) {
      ForceQuit();
    }
  }

  void OnResuming(Platform::Object ^ sender, Platform::Object ^ args) {
    SB_LOG(INFO) << "Resuming application.";
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeUnfreeze, NULL, NULL));
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeReveal, NULL, NULL));
    ApplicationUwp::Get()->DispatchAndDelete(
        new ApplicationUwp::Event(kSbEventTypeFocus, NULL, NULL));
    shared::win32::DrmSystemOnUwpResume();
    shared::uwp::ExtendedResourcesManager::GetInstance()
        ->AcquireExtendedResources();
  }

  void OnKeyUp(CoreWindow ^ sender, KeyEventArgs ^ args) {
    ApplicationUwp::Get()->OnKeyEvent(sender, args, true);
  }

  void OnKeyDown(CoreWindow ^ sender, KeyEventArgs ^ args) {
    ApplicationUwp::Get()->OnKeyEvent(sender, args, false);
  }

  void OnActivated(CoreApplicationView ^ application_view,
                   IActivatedEventArgs ^ args) {
    SB_LOG(INFO) << "OnActivated";
    shared::uwp::ExtendedResourcesManager::GetInstance()
        ->AcquireExtendedResources();

    std::string start_url = entry_point_;
    bool command_line_set = false;

    // Please see application lifecycle description:
    // https://docs.microsoft.com/en-us/windows/uwp/launch-resume/app-lifecycle
    // Note that this document was written for Xaml apps not core apps,
    // so for us the precise API is a little different.
    // The substance is that, while OnActivated is definitely called the
    // first time the application is started, it may additionally called
    // in other cases while the process is already running. Starboard
    // applications cannot fully restart in a process lifecycle,
    // so we interpret the first activation and the subsequent ones differently.
    if (args->Kind == ActivationKind::Protocol) {
      Uri ^ uri = dynamic_cast<IProtocolActivatedEventArgs ^>(args)->Uri;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      // The starboard: scheme provides commandline arguments, but that's
      // only allowed during a process's first activation.
      std::string scheme = platformStringToString(uri->SchemeName);
      if (!previously_activated_ && ends_with(scheme, "-starboard")) {
        std::string uri_string = wchar_tToUTF8(uri->RawUri->Data());
        // args_ is a vector of std::string, but argv_ is a vector of
        // char* into args_ so as to compose a char**.
        args_ = ParseStarboardUri(uri_string);
        for (const std::string& arg : args_) {
          argv_.push_back(arg.c_str());
        }

        ApplicationUwp::Get()->SetCommandLine(static_cast<int>(argv_.size()),
                                              argv_.data());
        command_line_set = true;
      }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      if (uri->SchemeName->Equals("youtube") ||
          uri->SchemeName->Equals("youtube-tv") ||
          uri->SchemeName->Equals("ms-xbl-07459769")) {
        std::string uri_string = platformStringToString(uri->RawUri);

        // Strip the protocol from the uri.
        size_t index = uri_string.find(':');
        if (index != std::string::npos) {
          uri_string = uri_string.substr(index + 1);
        }

        ProcessDeepLinkUri(&uri_string);
      }
    } else if (args->Kind == ActivationKind::DialReceiver) {
      DialReceiverActivatedEventArgs ^ dial_args =
          dynamic_cast<DialReceiverActivatedEventArgs ^>(args);
      SB_CHECK(dial_args);
      Platform::String ^ arguments = dial_args->Arguments;
      if (previously_activated_) {
        std::string uri_string =
            kDialParamPrefix + platformStringToString(arguments);
        ProcessDeepLinkUri(&uri_string);
      } else {
        std::string activation_args = "--url=";
        activation_args.append(start_url);
        activation_args.append("?");
        activation_args.append(platformStringToString(arguments));
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
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
        TryAddCommandArgsFromStarboardFile(&args_);
        CommandLine cmd_line(args_);
        if (cmd_line.HasSwitch(kNetArgsCommandSwitchWait)) {
          // Wait for net args is flaky and needs extended wait time on Xbox.
          SbTime timeout = kSbTimeSecond * 30;
          std::string val = cmd_line.GetSwitchValue(kNetArgsCommandSwitchWait);
          if (!val.empty()) {
            timeout = atoi(val.c_str());
          }
          AddCommandArgsFromNetArgs(timeout, &args_);
        }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

        if (!CommandLine(args_).HasSwitch("url")) {
          args_.push_back(GetArgvZero());
          std::string start_url_arg = "--url=";
          start_url_arg.append(start_url);
          args_.push_back(start_url_arg);
          std::string partition_arg = "--local_storage_partition_url=";
          partition_arg.append(entry_point_);
          args_.push_back(partition_arg);
        }

        for (auto& arg : args_) {
          argv_.push_back(arg.c_str());
        }

        ApplicationUwp::Get()->SetCommandLine(static_cast<int>(argv_.size()),
                                              argv_.data());
      }

      ApplicationUwp* application_uwp = ApplicationUwp::Get();
      const CommandLine* command_line = application_uwp->GetCommandLine();

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
      if (command_line->HasSwitch(kWatchDogLog)) {
        // Launch a thread.
        std::string switch_val = command_line->GetSwitchValue(kWatchDogLog);
        auto uwp_dir =
            Windows::Storage::ApplicationData::Current->LocalCacheFolder;
        std::stringstream ss;
        ss << platformStringToString(uwp_dir->Path) << "/" << switch_val;
        shared::uwp::StartWatchdogLog(ss.str());
      }

      if (command_line->HasSwitch(kNetLogCommandSwitchWait)) {
        SbTime timeout = kSbTimeSecond;
        std::string val =
            command_line->GetSwitchValue(kNetLogCommandSwitchWait);
        if (!val.empty()) {
          timeout = atoi(val.c_str());
        }
        NetLogWaitForClientConnected(timeout);
      }

      if (command_line->HasSwitch(kLogPathSwitch)) {
        std::stringstream ss;
        ss << platformStringToString(
            Windows::Storage::ApplicationData::Current->LocalCacheFolder->Path);
        ss << "\\"
           << "" << command_line->GetSwitchValue(kLogPathSwitch);
        std::string full_path_log_file = ss.str();
        shared::uwp::OpenLogFileWin32(full_path_log_file.c_str());
      } else {
#if !defined(COBALT_BUILD_TYPE_GOLD)
        // Log to a file on the last removable device available (probably the
        // most recently added removable device).
        if (KnownFolders::RemovableDevices != nullptr) {
          concurrency::create_task(
              KnownFolders::RemovableDevices->GetFoldersAsync())
              .then(
                  [](concurrency::task<IVectorView<StorageFolder ^> ^> result) {
                    IVectorView<StorageFolder ^> ^ results;
                    try {
                      results = result.get();
                    } catch (Platform::Exception ^) {
                      SB_LOG(ERROR)
                          << "Unable to open log file in RemovableDevices";
                      return;
                    }

                    if (results->Size == 0) {
                      return;
                    }

                    StorageFolder ^ folder = results->GetAt(results->Size - 1);
                    Calendar ^ now = ref new Calendar();
                    char filename[128];
                    SbStringFormatF(filename, sizeof(filename),
                                    "cobalt_log_%04d%02d%02d_%02d%02d%02d.txt",
                                    now->Year, now->Month, now->Day,
                                    now->Hour + now->FirstHourInThisPeriod,
                                    now->Minute, now->Second);
                    shared::uwp::OpenLogFileUWP(folder, filename);
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
        ApplicationUwp::Get()->DispatchStart(application_start_time_);
      });
    }
    previously_activated_ = true;
  }

 private:
  void ProcessDeepLinkUri(std::string* uri_string) {
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
  bool is_online_;
  bool previously_activated_;
  // Only valid if previously_activated_ is true
  ActivationKind previous_activation_kind_;
  std::vector<std::string> args_;
  std::vector<const char*> argv_;

  shared::uwp::ApplicationUwp application_;
  SuspendingDeferral ^ suspend_deferral_ = nullptr;
  SbTimeMonotonic application_start_time_;
};

ref class Direct3DApplicationSource sealed : IFrameworkViewSource {
 public:
  Direct3DApplicationSource(SbTimeMonotonic start_time)
      : application_start_time_{start_time} {}
  virtual IFrameworkView ^ CreateView() {
    return ref new App(application_start_time_);
  } private : SbTimeMonotonic application_start_time_;
};

namespace shared {
namespace uwp {

void DisplayStatusWatcher::CreateWatcher() {
  Platform::Collections::Vector<Platform::String ^> ^ requestedProperties =
      ref new Platform::Collections::Vector<Platform::String ^>();
  requestedProperties->Append("System.Devices.InterfaceClassGuid");
  requestedProperties->Append("System.ItemNameDisplay");
  watcher_ = DeviceInformation::CreateWatcher(
      const_cast<Platform::String ^>(kGenericPnpMonitorAqs),
      requestedProperties);
  SB_CHECK(watcher_);
  watcher_->Added +=
      ref new TypedEventHandler<DeviceWatcher ^, DeviceInformation ^>(
          &OnDeviceAdded);
  watcher_->Removed +=
      ref new TypedEventHandler<DeviceWatcher ^, DeviceInformationUpdate ^>(
          &OnDeviceRemoved);
  watcher_->Start();
}

bool DisplayStatusWatcher::IsWatcherStarted() {
  SB_CHECK(watcher_);
  DeviceWatcherStatus status = watcher_->Status;
  return (status == DeviceWatcherStatus::Started) ||
         (status == DeviceWatcherStatus::EnumerationCompleted);
}

void DisplayStatusWatcher::StopWatcher() {
  SB_CHECK(watcher_);
  if (IsWatcherStarted()) {
    watcher_->Stop();
  }
}

#if SB_API_VERSION >= 15
ApplicationUwp::ApplicationUwp(SbEventHandleCallback sb_event_handle_callback)
    : shared::starboard::Application(sb_event_handle_callback),
      window_(kSbWindowInvalid),
#else
ApplicationUwp::ApplicationUwp()
    : window_(kSbWindowInvalid),
#endif  // SB_API_VERSION >= 15
      localized_strings_(SbSystemGetLocaleId()),
      device_id_(MakeDeviceId()) {
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  window_size_ = options.size;
  analog_thumbstick_thread_.reset(new AnalogThumbstickThread(this));
  // We don't check result of operation because if something went wrong
  // when we try to delete local cache folder, we can do nothing with it.
  ClearLocalCacheIfNeeded();
}

ApplicationUwp::~ApplicationUwp() {
  SB_CHECK(watcher_);
  {
    ScopedLock lock(time_event_mutex_);
    timer_event_map_.clear();
  }
  watcher_->StopWatcher();
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

  if (!watcher_) {
    watcher_ = ref new shared::uwp::DisplayStatusWatcher();
    watcher_->CreateWatcher();
  }

  window_size_ = GetPreferredWindowSize();

  window_ = new SbWindowPrivate(window_size_.width, window_size_.height);
  return window_;
}

SbWindowSize ApplicationUwp::GetVisibleAreaSize() {
  SbWindowSize size{};
  if (SbWindowGetSize(window_, &size)) {
    return size;
  }
  return window_size_;
}

SbWindowSize ApplicationUwp::GetPreferredWindowSize() {
  bool scale_enabled = true;
  if (ApiInformation::IsApiContractPresent(
          "Windows.UI.ViewManagement.ViewManagementViewScalingContract", 1)) {
    // Use unscaled layout where possible
    scale_enabled = !ApplicationViewScaling::TrySetDisableLayoutScaling(true);
  }

  auto app_view = ApplicationView::GetForCurrentView();
  bool is_fullscreen = app_view->IsFullScreenMode;

  if (!is_fullscreen && scale_enabled &&
      ApiInformation::IsApiContractPresent(
          "Windows.Xbox.ApplicationResourcesContract", 1)) {
    // Always use full screen mode for XBox
    is_fullscreen = app_view->TryEnterFullScreenMode();
  }

  UpdateDisplayPreferredMode();
  int width = 0;
  int height = 0;
  auto display_mode = ApplicationUwp::Get()->GetPreferredModeHdr();
  if (!display_mode) {
    display_mode = ApplicationUwp::Get()->GetPreferredModeHdmi();
  }
  if (display_mode) {
    width = display_mode->ResolutionWidthInRawPixels;
    height = display_mode->ResolutionHeightInRawPixels;
  } else {
    auto bounds = app_view->VisibleBounds;
    auto scaleFactor =
        DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;
    width = bounds.Width * scaleFactor;
    height = bounds.Height * scaleFactor;
  }
  SB_LOG(INFO) << "Preferred window size is " << width << " x " << height
               << "\nScaling is " << (scale_enabled ? "enabled" : "disabled")
               << "\nFull Screen mode is " << (is_fullscreen ? "ON" : "OFF");

  return SbWindowSize{width, height};
}

bool ApplicationUwp::DestroyWindow(SbWindow window) {
  // TODO: Determine why SB_DCHECK(IsCurrentThread()) fails in nplb, fix it,
  // and add back this check.

  if (!SbWindowIsValid(window)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid context.";
    return false;
  }

  auto app_view = ApplicationView::GetForCurrentView();
  if (app_view->IsFullScreenMode &&
      ApiInformation::IsApiContractPresent(
          "Windows.Xbox.ApplicationResourcesContract", 1)) {
    app_view->ExitFullScreenMode();
  }

  SB_DCHECK(window_ == window);
  delete window;
  window_ = kSbWindowInvalid;

  return true;
}

void ApplicationUwp::UpdateDisplayPreferredMode() {
  ScopedLock lock(preferred_display_mode_mutex_);
  preferred_display_mode_hdmi_ = nullptr;
  preferred_display_mode_hdr_ = nullptr;
  if (!ApiInformation::IsTypePresent(
          "Windows.Graphics.Display.Core.HdmiDisplayInformation")) {
    return;
  }

  auto hdmi_display_info = HdmiDisplayInformation::GetForCurrentView();
  if (!hdmi_display_info) {
    return;
  }

  preferred_display_mode_hdmi_ = hdmi_display_info->GetCurrentDisplayMode();
  for (auto mode : hdmi_display_info->GetSupportedDisplayModes()) {
    // Check that resolution matches the preferred display mode.
    if (mode->ResolutionWidthInRawPixels !=
            preferred_display_mode_hdmi_->ResolutionWidthInRawPixels ||
        mode->ResolutionHeightInRawPixels !=
            preferred_display_mode_hdmi_->ResolutionHeightInRawPixels) {
      continue;
    }
    // Verify HDR metadata and transfer function are supported.
    if (!mode->Is2086MetadataSupported || !mode->IsSmpte2084Supported) {
      continue;
    }
    // Verify we have enough bits per pixel and the correct color space for HDR.
    if (mode->BitsPerPixel < kYuv420BitsPerPixelForHdr10Mode ||
        mode->ColorSpace != HdmiDisplayColorSpace::BT2020) {
      continue;
    }
    // We don't serve 4k HDR videos over 60fps, skipping display modes that will
    // consume more power than needed.
    if (mode->ResolutionWidthInRawPixels >= k4kResolutionWidth &&
        mode->ResolutionHeightInRawPixels >= k4kResolutionHeight &&
        mode->RefreshRate > kHdr4kRefreshRateMaximum) {
      continue;
    }
    if (!preferred_display_mode_hdr_ ||
        preferred_display_mode_hdr_->RefreshRate < mode->RefreshRate) {
      preferred_display_mode_hdr_ = mode;
    }
  }
}

bool ApplicationUwp::DispatchNextEvent() {
  core_window_->Activate();
  core_window_->Dispatcher->ProcessEvents(
      CoreProcessEventsOption::ProcessUntilQuit);
  return false;
}

Windows::Media::Protection::HdcpSession ^ ApplicationUwp::GetHdcpSession() {
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

bool ApplicationUwp::ClearLocalCacheIfNeeded() {
  // We compare saved app version (previous launch) and current version.
  // If they are different, it means that this is the very first launch
  // after install or after update.
  // In both cases we clear LocalCacheFolder to ensure that the old saved
  // data can't impact on new app version.
  PackageVersion v = Package::Current->Id->Version;
  std::string version =
      std::to_string(v.Major) + "." + std::to_string(v.Minor) + "." +
      std::to_string(v.Build) + "." + std::to_string(v.Revision);
  SB_LOG(INFO) << "Current package version is " << version;

  auto localFolder = ApplicationData::Current->LocalFolder;
  std::wstring path = localFolder->Path->Data();
  path += L"\\app_version.txt";

  std::string prev_version;
  std::fstream version_file(path.c_str(), std::ios_base::in);
  if (version_file.is_open()) {
    version_file >> prev_version;
    version_file.close();
  }
  if (version.compare(prev_version) != 0) {
    SB_LOG(INFO) << "Previous version is " << prev_version
                 << ". Trying to clear local cache.";
    if (ClearLocalCacheFolder()) {
      // Save current version only in case of success.
      // If something went wrong, we leave current version unchanged and
      // this check will be repeated again in next app launch.
      version_file.open(path.c_str(),
                        std::ios_base::trunc | std::ios_base::out);
      if (version_file.is_open()) {
        version_file << version;
        version_file.close();
        return true;
      } else {
        SB_LOG(ERROR) << "Unable to open version file for writing.";
        return false;
      }
    }
  }
  return false;
}

bool ApplicationUwp::ClearLocalCacheFolder() {
  auto cacheFolder = ApplicationData::Current->LocalCacheFolder;
  std::wstring wpath = cacheFolder->Path->Data();
  return ::starboard::shared::win32::ClearOrDeleteDirectory(wpath, false);
}

void ApplicationUwp::Inject(Application::Event* event) {
  RunInMainThreadAsync([this, event]() {
    bool result = DispatchAndDelete(event);
    if (!result) {
      NetLogFlushThenClose();
      CoreApplication::Exit();
    }
  });
}

void ApplicationUwp::InjectKeypress(SbKey key) {
  if (!SbWindowIsValid(window_)) {
    return;
  }
  std::unique_ptr<SbInputData> press_data(new SbInputData());
  std::unique_ptr<SbInputData> unpress_data(new SbInputData());

  memset(press_data.get(), 0, sizeof(*press_data));
  press_data->window = window_;
  press_data->device_type = kSbInputDeviceTypeKeyboard;
  press_data->device_id = device_id();
  press_data->key = key;
  press_data->type = kSbInputEventTypePress;

  *unpress_data = *press_data;
  unpress_data->type = kSbInputEventTypeUnpress;

  Inject(new Event(kSbEventTypeInput, press_data.release(),
                   &Application::DeleteDestructor<SbInputData>));
  Inject(new Event(kSbEventTypeInput, unpress_data.release(),
                   &Application::DeleteDestructor<SbInputData>));
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

  ScopedLock lock(time_event_mutex_);
  ThreadPoolTimer ^ timer = ThreadPoolTimer::CreateTimer(
      ref new TimerElapsedHandler([this, timed_event](ThreadPoolTimer ^ timer) {
        RunInMainThreadAsync([this, timed_event]() {
          // Even if the event is canceled, the callback can still fire.
          // Thus, the existence of event in timer_event_map_ is used
          // as a source of truth.
          std::size_t number_erased = 0;
          {
            ScopedLock lock(time_event_mutex_);
            number_erased = timer_event_map_.erase(timed_event->id);
          }
          if (number_erased > 0) {
            timed_event->callback(timed_event->context);
          }
        });
      }),
      timespan);
  timer_event_map_.emplace(timed_event->id, timer);
}

void ApplicationUwp::CancelTimedEvent(SbEventId event_id) {
  ScopedLock lock(time_event_mutex_);
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
  if (!SbWindowIsValid(window_)) {
    return;
  }
  scoped_ptr<SbInputData> data(new SbInputData());
  memset(data.get(), 0, sizeof(*data));
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

Platform::String ^
    ApplicationUwp::GetString(const char* id, const char* fallback) const {
  return stringToPlatformString(localized_strings_.GetString(id, fallback));
}

bool ApplicationUwp::IsHdcpOn() {
  ScopedLock lock(hdcp_session_mutex_);

  return GetHdcpSession()->IsEffectiveProtectionAtLeast(kHDCPProtectionMode);
}

bool ApplicationUwp::TurnOnHdcp() {
  HdcpSetProtectionResult protection_result;
  {
    ScopedLock lock(hdcp_session_mutex_);

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
    ScopedLock lock(hdcp_session_mutex_);
    ResetHdcpSession();
  }
  bool success = !IsHdcpOn();
  return success;
}

// TODO: Consolidate this function with TurnOfHdcp() and TurnOffHdcp().
bool ApplicationUwp::SetOutputProtection(bool should_enable_dhcp) {
  bool is_hdcp_on = IsHdcpOn();

  if (is_hdcp_on == should_enable_dhcp) {
    return true;
  }

  SB_LOG(INFO) << "Attempting to "
               << (should_enable_dhcp ? "enable" : "disable")
               << " output protection.  Current status: "
               << (is_hdcp_on ? "enabled" : "disabled");
  SbTimeMonotonic tick = SbTimeGetMonotonicNow();

  bool hdcp_success = false;
  if (should_enable_dhcp) {
    hdcp_success = TurnOnHdcp();
  } else {
    hdcp_success = TurnOffHdcp();
  }

  is_hdcp_on = (hdcp_success ? should_enable_dhcp : !should_enable_dhcp);

  SbTimeMonotonic tock = SbTimeGetMonotonicNow();
  SB_LOG(INFO) << "Output protection is "
               << (is_hdcp_on ? "enabled" : "disabled")
               << ".  Toggling HDCP took " << (tock - tick) / kSbTimeMillisecond
               << " milliseconds.";
  return hdcp_success;
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

void InjectKeypress(SbKey key) {
  ApplicationUwp::Get()->InjectKeypress(key);
}

namespace {

// Calls CoreApplication::Run() on a new thread to free up the main thread.
// This allows all extended resources related operations to be run on the main
// thread.
class CoreApplicationThread : public ::starboard::Thread {
 public:
  explicit CoreApplicationThread(SbTimeMonotonic start_time)
      : application_start_time_{start_time}, Thread("core_app") {}
  void Run() override {
    CoreApplication::Run(
        ref new Direct3DApplicationSource(application_start_time_));
    ExtendedResourcesManager::GetInstance()->Quit();
  }

 private:
  SbTimeMonotonic application_start_time_;
};

}  // namespace

int InternalMain() {
  volatile auto start_time = SbTimeGetMonotonicNow();
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

#if defined(COBALT_BUILD_TYPE_GOLD)
  // Early exit for gold builds on desktop as a security measure.
#if SB_API_VERSION < 15
  if (SbSystemGetDeviceType() == kSbSystemDeviceTypeDesktopPC) {
    return main_return_value;
  }
#else
  if (GetSystemPropertyString(kSbSystemPropertyDeviceType) ==
      kSystemDeviceTypeDesktopPC) {
    return main_return_value;
  }
#endif
#endif  // defined(COBALT_BUILD_TYPE_GOLD)

  shared::win32::RegisterMainThread();

  ExtendedResourcesManager extended_resources_manager;
  CoreApplicationThread thread(start_time);

  thread.Start();
  extended_resources_manager.Run();

  NetLogFlushThenClose();
  CoreApplication::Exit();

  MFShutdown();
  WSACleanup();

  return main_return_value;
}

#if SB_API_VERSION >= 15
extern "C" int SbRunStarboardMain(int argc,
                                  char** argv,
                                  SbEventHandleCallback callback) {
  return ApplicationUwp::Get()->Run(argc, argv);
}
#endif  // SB_API_VERSION >= 15

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

[Platform::MTAThread] int main(Platform::Array<Platform::String ^> ^ args) {
  return starboard::shared::uwp::InternalMain();
}
