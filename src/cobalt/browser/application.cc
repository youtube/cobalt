/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/browser/application.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/time.h"
#include "build/build_config.h"
#include "cobalt/account/account_event.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/deep_link_event.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/base/localized_strings.h"
#include "cobalt/base/user_log.h"
#include "cobalt/browser/switches.h"
#include "cobalt/deprecated/platform_delegate.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/math/size.h"
#include "cobalt/network/network_event.h"
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "cobalt/storage/savegame_fake.h"
#endif
#include "cobalt/system_window/application_event.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "googleurl/src/gurl.h"
#if defined(__LB_SHELL__)
#if !defined(__LB_SHELL__FOR_RELEASE__)
#include "lbshell/src/lb_memory_manager.h"
#endif  // defined(__LB_SHELL__FOR_RELEASE__)
#include "lbshell/src/lb_memory_pages.h"
#endif  // defined(__LB_SHELL__)

namespace cobalt {
namespace browser {

namespace {
const int kStatUpdatePeriodMs = 1000;

const char kDefaultURL[] = "https://www.youtube.com/tv";

#if defined(ENABLE_REMOTE_DEBUGGING)
int GetRemoteDebuggingPort() {
  const int kDefaultRemoteDebuggingPort = 9222;
  int remote_debugging_port = kDefaultRemoteDebuggingPort;
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kRemoteDebuggingPort)) {
    const std::string switchValue =
        command_line->GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (!base::StringToInt(switchValue, &remote_debugging_port)) {
      DLOG(ERROR) << "Invalid port specified for remote debug server: "
                  << switchValue
                  << ". Using default port: " << kDefaultRemoteDebuggingPort;
      remote_debugging_port = kDefaultRemoteDebuggingPort;
    }
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
  return remote_debugging_port;
}
#endif  // ENABLE_REMOTE_DEBUGGING

#if defined(ENABLE_WEBDRIVER)
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
int GetWebDriverPort() {
  // The default port on which the webdriver server should listen for incoming
  // connections.
  const int kDefaultWebDriverPort = 9515;
  int webdriver_port = kDefaultWebDriverPort;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWebDriverPort)) {
    if (!base::StringToInt(
            command_line->GetSwitchValueASCII(switches::kWebDriverPort),
            &webdriver_port)) {
      DLOG(ERROR) << "Invalid port specified for WebDriver server: "
                  << command_line->GetSwitchValueASCII(switches::kWebDriverPort)
                  << ". Using default port: " << kDefaultWebDriverPort;
      webdriver_port = kDefaultWebDriverPort;
    }
  }
  return webdriver_port;
}
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
#endif  // ENABLE_WEBDRIVER

GURL GetInitialURL() {
  // Allow the user to override the default URL via a command line parameter.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInitialURL)) {
    GURL url = GURL(command_line->GetSwitchValueASCII(switches::kInitialURL));
    if (url.is_valid()) {
      return url;
    } else {
      DLOG(INFO) << "URL from parameter is not valid, using default URL.";
    }
  }

  return GURL(kDefaultURL);
}

base::TimeDelta GetTimedTraceDuration() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kTimedTrace) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kTimedTrace),
          &duration_in_seconds)) {
    return base::TimeDelta::FromSeconds(duration_in_seconds);
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return base::TimeDelta();
}

FilePath GetExtraWebFileDir() {
  // Default is empty, command line can override.
  FilePath result;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kExtraWebFileDir)) {
    result =
        FilePath(command_line->GetSwitchValueASCII(switches::kExtraWebFileDir));
    if (!result.IsAbsolute()) {
      // Non-absolute paths are relative to the executable directory.
      FilePath content_path;
      PathService::Get(base::DIR_EXE, &content_path);
      result = content_path.DirName().DirName().Append(result);
    }
    DLOG(INFO) << "Extra web file dir: " << result.value();
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return result;
}

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
void EnableUsingStubImageDecoderIfRequired() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kStubImageDecoder)) {
    loader::image::ImageDecoder::UseStubImageDecoder();
  }
}
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

void SetIntegerIfSwitchIsSet(const char* switch_name, int* output) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    int32 out;
    if (base::StringToInt32(
            CommandLine::ForCurrentProcess()->GetSwitchValueNative(switch_name),
            &out)) {
      LOG(INFO) << "Command line switch '" << switch_name << "': Modifying "
                << *output << " -> " << out;
      *output = out;
    } else {
      LOG(ERROR) << "Invalid value for command line setting: " << switch_name;
    }
  }
}

void ApplyCommandLineSettingsToRendererOptions(
    renderer::RendererModule::Options* options) {
  SetIntegerIfSwitchIsSet(browser::switches::kSurfaceCacheSizeInBytes,
                          &options->surface_cache_size_in_bytes);
  SetIntegerIfSwitchIsSet(browser::switches::kScratchSurfaceCacheSizeInBytes,
                          &options->scratch_surface_cache_size_in_bytes);
  SetIntegerIfSwitchIsSet(browser::switches::kSkiaCacheSizeInBytes,
                          &options->skia_cache_size_in_bytes);
}

void ApplyCommandLineSettingsToWebModuleOptions(WebModule::Options* options) {
  SetIntegerIfSwitchIsSet(browser::switches::kImageCacheSizeInBytes,
                          &options->image_cache_capacity);
  SetIntegerIfSwitchIsSet(browser::switches::kRemoteTypefaceCacheSizeInBytes,
                          &options->remote_typeface_cache_capacity);
}

// Restrict navigation to a couple of whitelisted URLs by default.
const char kYouTubeTvLocationPolicy[] =
    "h5vcc-location-src "
    "https://www.youtube.com/tv "
    "https://web-release-qa.youtube.com/tv "
#if defined(ENABLE_ABOUT_SCHEME)
    "about: "
#endif
    "h5vcc:";

#if !defined(COBALT_FORCE_CSP)
dom::CspEnforcementType StringToCspMode(const std::string& mode) {
  if (mode == "disable") {
    return dom::kCspEnforcementDisable;
  } else if (mode == "enable") {
    return dom::kCspEnforcementEnable;
  } else {
    DLOG(INFO) << "Invalid CSP mode: " << mode << ": use [disable|enable]";
    return dom::kCspEnforcementEnable;
  }
}
#endif  // !defined(COBALT_FORCE_CSP)

struct NonTrivialStaticFields {
  NonTrivialStaticFields()
      : system_language(
            cobalt::deprecated::PlatformDelegate::Get()->GetSystemLanguage()) {}

  const std::string system_language;
  std::string user_agent;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

// |non_trivial_static_fields| will be lazily created on the first time it's
// accessed.
base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Static user logs
ssize_t Application::available_memory_ = 0;
int64 Application::lifetime_in_ms_ = 0;

Application::AppStatus Application::app_status_ =
    Application::kUninitializedAppStatus;
int Application::app_pause_count_ = 0;
int Application::app_unpause_count_ = 0;
int Application::app_suspend_count_ = 0;
int Application::app_resume_count_ = 0;

Application::NetworkStatus Application::network_status_ =
    Application::kDisconnectedNetworkStatus;
int Application::network_connect_count_ = 0;
int Application::network_disconnect_count_ = 0;

Application::Application(const base::Closure& quit_closure)
    : message_loop_(MessageLoop::current()),
      quit_closure_(quit_closure),
      start_time_(base::TimeTicks::Now()),
      stats_update_timer_(true, true) {
  DCHECK(MessageLoop::current());
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

  network_event_thread_checker_.DetachFromThread();
  application_event_thread_checker_.DetachFromThread();

  RegisterUserLogs();

  stats_update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kStatUpdatePeriodMs),
      base::Bind(&Application::UpdatePeriodicStats, base::Unretained(this)));

  // Check to see if a timed_trace has been set, indicating that we should
  // begin a timed trace upon startup.
  base::TimeDelta trace_duration = GetTimedTraceDuration();
  if (trace_duration != base::TimeDelta()) {
    trace_event::TraceToFileForDuration(
        FilePath(FILE_PATH_LITERAL("timed_trace.json")), trace_duration);
  }

  // Get the initial URL.
  GURL initial_url = GetInitialURL();
  DLOG(INFO) << "Initial URL: " << initial_url;

  // Get the system language and initialize our localized strings.
  std::string language =
      cobalt::deprecated::PlatformDelegate::Get()->GetSystemLanguage();
  base::LocalizedStrings::GetInstance()->Initialize(language);

  // Create the main components of our browser.
  BrowserModule::Options options;
  options.web_module_options.name = "MainWebModule";
  options.language = language;
  options.initial_deep_link = GetInitialDeepLink();
  options.network_module_options.preferred_language = language;

  ApplyCommandLineSettingsToRendererOptions(&options.renderer_module_options);
  ApplyCommandLineSettingsToWebModuleOptions(&options.web_module_options);

  CommandLine* command_line = CommandLine::ForCurrentProcess();

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(browser::switches::kNullSavegame)) {
    options.storage_manager_options.savegame_options.factory =
        &storage::SavegameFake::Create;
  }
#endif

  // User can specify an extra search path entry for files loaded via file://.
  options.web_module_options.extra_web_file_dir = GetExtraWebFileDir();
  options.web_module_options.location_policy = kYouTubeTvLocationPolicy;
  // Set callback to be notified when a navigation occurs that destroys the
  // underlying WebModule.
  options.web_module_recreated_callback =
      base::Bind(&Application::WebModuleRecreated, base::Unretained(this));

  // The main web module's stat tracker tracks event stats.
  options.web_module_options.track_event_stats = true;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(browser::switches::kProxy)) {
    options.network_module_options.custom_proxy =
        command_line->GetSwitchValueASCII(browser::switches::kProxy);
  }

#if !defined(COBALT_FORCE_CSP)
  if (command_line->HasSwitch(browser::switches::kCspMode)) {
    options.web_module_options.csp_enforcement_mode = StringToCspMode(
        command_line->GetSwitchValueASCII(browser::switches::kCspMode));
  }
  if (options.web_module_options.csp_enforcement_mode !=
      dom::kCspEnforcementEnable) {
    options.web_module_options.location_policy = "h5vcc-location-src *";
  }
#endif  // !defined(COBALT_FORCE_CSP)

#if defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)
  if (command_line->HasSwitch(browser::switches::kIgnoreCertificateErrors)) {
    options.network_module_options.ignore_certificate_errors = true;
  }
#endif  // defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)

#if !defined(COBALT_FORCE_HTTPS)
  if (command_line->HasSwitch(switches::kAllowHttp)) {
    DLOG(INFO) << "Allowing insecure HTTP connections";
    options.network_module_options.require_https = false;
  }
#endif  // !defined(COBALT_FORCE_HTTPS)

  EnableUsingStubImageDecoderIfRequired();

  if (command_line->HasSwitch(browser::switches::kDisableWebmVp9)) {
    DLOG(INFO) << "Webm/Vp9 disabled";
    options.media_module_options.disable_webm_vp9 = true;
  }
  if (command_line->HasSwitch(switches::kAudioDecoderStub)) {
    DLOG(INFO) << "Use ShellRawAudioDecoderStub";
    options.media_module_options.use_audio_decoder_stub = true;
  }
  if (command_line->HasSwitch(switches::kNullAudioStreamer)) {
    DLOG(INFO) << "Use null audio";
    options.media_module_options.use_null_audio_streamer = true;
  }
  if (command_line->HasSwitch(switches::kVideoContainerSizeOverride)) {
    std::string size_override = command_line->GetSwitchValueASCII(
        browser::switches::kVideoContainerSizeOverride);
    DLOG(INFO) << "Set video container size override from command line to "
               << size_override;
    deprecated::PlatformDelegate::Get()->SetVideoContainerSizeOverride(
        size_override);
  }
  if (command_line->HasSwitch(switches::kVideoDecoderStub)) {
    DLOG(INFO) << "Use ShellRawVideoDecoderStub";
    options.media_module_options.use_video_decoder_stub = true;
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  base::optional<math::Size> viewport_size;
  if (command_line->HasSwitch(browser::switches::kViewport)) {
    const std::string switchValue =
        command_line->GetSwitchValueASCII(browser::switches::kViewport);
    std::vector<std::string> lengths;
    base::SplitString(switchValue, 'x', &lengths);
    if (lengths.size() >= 1) {
      int width = -1;
      if (base::StringToInt(lengths[0], &width) && width >= 1) {
        int height = -1;
        if (lengths.size() < 2) {
          // Allow shorthand specification of the viewport by only giving the
          // width. This calculates the height at 4:3 aspect ratio for smaller
          // viewport widths, and 16:9 for viewports 1280 pixels wide or larger.
          if (width >= 1280) {
            viewport_size.emplace(width, 9 * width / 16);
          } else {
            viewport_size.emplace(width, 3 * width / 4);
          }
        } else if (base::StringToInt(lengths[1], &height) && height >= 1) {
          viewport_size.emplace(width, height);
        } else {
          DLOG(ERROR) << "Invalid value specified for viewport height: "
                      << switchValue << ". Using default viewport size.";
        }
      } else {
        DLOG(ERROR) << "Invalid value specified for viewport width: "
                    << switchValue << ". Using default viewport size.";
      }
    }
  }

  system_window_ =
      system_window::CreateSystemWindow(&event_dispatcher_, viewport_size);
  account_manager_ = account::AccountManager::Create(&event_dispatcher_);
  browser_module_.reset(new BrowserModule(initial_url, system_window_.get(),
                                          account_manager_.get(), options));
  UpdateAndMaybeRegisterUserAgent();

  app_status_ = kRunningAppStatus;

  // Register event callbacks.
  account_event_callback_ =
      base::Bind(&Application::OnAccountEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(account::AccountEvent::TypeId(),
                                     account_event_callback_);
  network_event_callback_ =
      base::Bind(&Application::OnNetworkEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(network::NetworkEvent::TypeId(),
                                     network_event_callback_);
  application_event_callback_ =
      base::Bind(&Application::OnApplicationEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(system_window::ApplicationEvent::TypeId(),
                                     application_event_callback_);
  deep_link_event_callback_ =
      base::Bind(&Application::OnDeepLinkEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(base::DeepLinkEvent::TypeId(),
                                     deep_link_event_callback_);

#if defined(ENABLE_WEBDRIVER)
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(switches::kEnableWebDriver)) {
    int webdriver_port = GetWebDriverPort();
    web_driver_module_.reset(new webdriver::WebDriverModule(
        webdriver_port, base::Bind(&BrowserModule::CreateSessionDriver,
                                   base::Unretained(browser_module_.get())),
        base::Bind(&BrowserModule::RequestScreenshotToBuffer,
                   base::Unretained(browser_module_.get())),
        base::Bind(&BrowserModule::SetProxy,
                   base::Unretained(browser_module_.get())),
        base::Bind(&Application::Quit, base::Unretained(this))));
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
#endif  // ENABLE_WEBDRIVER

#if defined(ENABLE_REMOTE_DEBUGGING)
  int remote_debugging_port = GetRemoteDebuggingPort();
  debug_web_server_.reset(new debug::DebugWebServer(
      remote_debugging_port,
      base::Bind(&BrowserModule::GetDebugServer,
                 base::Unretained(browser_module_.get()))));
#endif  // ENABLE_REMOTE_DEBUGGING

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kShutdownAfter) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kShutdownAfter),
          &duration_in_seconds)) {
    // If the "shutdown_after" command line option is specified, setup a delayed
    // message to quit the application after the specified number of seconds
    // have passed.
    message_loop_->PostDelayedTask(
        FROM_HERE, quit_closure_,
        base::TimeDelta::FromSeconds(duration_in_seconds));
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
}

Application::~Application() {
  // Unregister event callbacks.
  event_dispatcher_.RemoveEventCallback(account::AccountEvent::TypeId(),
                                        account_event_callback_);
  event_dispatcher_.RemoveEventCallback(network::NetworkEvent::TypeId(),
                                        network_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      system_window::ApplicationEvent::TypeId(), application_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::DeepLinkEvent::TypeId(), deep_link_event_callback_);

  app_status_ = kShutDownAppStatus;
}

void Application::Quit() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&Application::Quit, base::Unretained(this)));
    return;
  }

  DCHECK(!quit_closure_.is_null());
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
  }

  app_status_ = kQuitAppStatus;
}

void Application::OnAccountEvent(const base::Event* event) {
  const account::AccountEvent* account_event =
      base::polymorphic_downcast<const account::AccountEvent*>(event);
  if (account_event->type() == account::AccountEvent::kSignedIn) {
    DLOG(INFO) << "Got signed in event, checking for age restriction.";
    if (account_manager_->IsAgeRestricted()) {
      browser_module_->Navigate(GURL("h5vcc://age-restricted"));
    }
  } else if (account_event->type() == account::AccountEvent::kSignedOut) {
    DLOG(INFO) << "Got signed out event.";
    browser_module_->Navigate(GURL("h5vcc://signed-out"));
  }
}

void Application::OnNetworkEvent(const base::Event* event) {
  DCHECK(network_event_thread_checker_.CalledOnValidThread());
  const network::NetworkEvent* network_event =
      base::polymorphic_downcast<const network::NetworkEvent*>(event);
  if (network_event->type() == network::NetworkEvent::kDisconnection) {
    network_status_ = kDisconnectedNetworkStatus;
    ++network_disconnect_count_;
    browser_module_->Navigate(GURL("h5vcc://network-failure"));
  } else if (network_event->type() == network::NetworkEvent::kConnection) {
    network_status_ = kConnectedNetworkStatus;
    ++network_connect_count_;
    if (network_disconnect_count_ > 0) {
      DLOG(INFO) << "Got network connection event, reloading browser.";
      browser_module_->Reload();
    } else {
      DLOG(INFO) << "Got network connection event, NOT reloading browser.";
    }
  }
}

void Application::OnApplicationEvent(const base::Event* event) {
  DCHECK(application_event_thread_checker_.CalledOnValidThread());
  const system_window::ApplicationEvent* app_event =
      base::polymorphic_downcast<const system_window::ApplicationEvent*>(event);
  if (app_event->type() == system_window::ApplicationEvent::kQuit) {
    DLOG(INFO) << "Got quit event.";
    app_status_ = kWillQuitAppStatus;
    Quit();
  } else if (app_event->type() == system_window::ApplicationEvent::kPause) {
    DLOG(INFO) << "Got pause event.";
    app_status_ = kPausedAppStatus;
    ++app_pause_count_;
  } else if (app_event->type() == system_window::ApplicationEvent::kUnpause) {
    DLOG(INFO) << "Got unpause event.";
    app_status_ = kRunningAppStatus;
    ++app_unpause_count_;
  } else if (app_event->type() == system_window::ApplicationEvent::kSuspend) {
    DLOG(INFO) << "Got suspend event.";
    app_status_ = kSuspendedAppStatus;
    ++app_suspend_count_;
    browser_module_->Suspend();
    DLOG(INFO) << "Finished suspending.";
  } else if (app_event->type() == system_window::ApplicationEvent::kResume) {
    DLOG(INFO) << "Got resume event.";
    app_status_ = kPausedAppStatus;
    ++app_resume_count_;
    browser_module_->Resume();
    DLOG(INFO) << "Finished resuming.";
  }
}

void Application::OnDeepLinkEvent(const base::Event* event) {
  const base::DeepLinkEvent* deep_link_event =
      base::polymorphic_downcast<const base::DeepLinkEvent*>(event);
  // TODO: Remove this when terminal application states are properly handled.
  if (deep_link_event->IsH5vccLink()) {
    browser_module_->Navigate(GURL(deep_link_event->link()));
  }
}

void Application::WebModuleRecreated() {
#if defined(ENABLE_WEBDRIVER)
  if (web_driver_module_) {
    web_driver_module_->OnWindowRecreated();
  }
#endif
}

Application::CValStats::CValStats()
    : free_cpu_memory("Memory.CPU.Free", 0,
                      "Total free application CPU memory remaining."),
      used_cpu_memory("Memory.CPU.Used", 0,
                      "Total CPU memory allocated via the app's allocators."),
#if !defined(__LB_SHELL__FOR_RELEASE__)
      exe_memory("Memory.CPU.Exe", 0,
                 "Total memory occupied by the size of the executable."),
#endif
      app_lifetime("Cobalt.Lifetime", base::TimeDelta(),
                   "Application lifetime.") {
#if defined(OS_STARBOARD)
  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    free_gpu_memory.emplace("Memory.GPU.Free", 0,
                            "Total free application GPU memory remaining.");
    used_gpu_memory.emplace("Memory.GPU.Used", 0,
                            "Total GPU memory allocated by the application.");
  }
#endif  // defined(OS_STARBOARD)
}

void Application::RegisterUserLogs() {
  if (base::UserLog::IsRegistrationSupported()) {
    base::UserLog::Register(
        base::UserLog::kSystemLanguageStringIndex, "SystemLanguage",
        non_trivial_static_fields.Get().system_language.c_str(),
        non_trivial_static_fields.Get().system_language.size());

    base::UserLog::Register(base::UserLog::kAvailableMemoryIndex,
                            "AvailableMemory", &available_memory_,
                            sizeof(available_memory_));
    base::UserLog::Register(base::UserLog::kAppLifetimeIndex, "Lifetime(ms)",
                            &lifetime_in_ms_, sizeof(lifetime_in_ms_));
    base::UserLog::Register(base::UserLog::kAppStatusIndex, "AppStatus",
                            &app_status_, sizeof(app_status_));
    base::UserLog::Register(base::UserLog::kAppPauseCountIndex, "PauseCnt",
                            &app_pause_count_, sizeof(app_pause_count_));
    base::UserLog::Register(base::UserLog::kAppUnpauseCountIndex, "UnpauseCnt",
                            &app_unpause_count_, sizeof(app_unpause_count_));
    base::UserLog::Register(base::UserLog::kAppSuspendCountIndex, "SuspendCnt",
                            &app_suspend_count_, sizeof(app_suspend_count_));
    base::UserLog::Register(base::UserLog::kAppResumeCountIndex, "ResumeCnt",
                            &app_resume_count_, sizeof(app_resume_count_));
    base::UserLog::Register(base::UserLog::kNetworkStatusIndex,
                            "NetworkStatus", &network_status_,
                            sizeof(network_status_));
    base::UserLog::Register(base::UserLog::kNetworkConnectCountIndex,
                            "ConnectCnt", &network_connect_count_,
                            sizeof(network_connect_count_));
    base::UserLog::Register(base::UserLog::kNetworkDisconnectCountIndex,
                            "DisconnectCnt", &network_disconnect_count_,
                            sizeof(network_disconnect_count_));
  }
}

// NOTE: UserAgent registration is handled separately, as the value is not
// available when the app is first being constructed. Registration must happen
// each time the user agent is modified, because the string may be pointing
// to a new location on the heap.
void Application::UpdateAndMaybeRegisterUserAgent() {
  non_trivial_static_fields.Get().user_agent = browser_module_->GetUserAgent();
  DLOG(INFO) << "User Agent: " << non_trivial_static_fields.Get().user_agent;
  if (base::UserLog::IsRegistrationSupported()) {
    base::UserLog::Register(base::UserLog::kUserAgentStringIndex, "UserAgent",
                            non_trivial_static_fields.Get().user_agent.c_str(),
                            non_trivial_static_fields.Get().user_agent.size());
  }
}

void Application::UpdatePeriodicStats() {
#if defined(__LB_SHELL__)
  bool memory_stats_updated = false;
#if !defined(__LB_SHELL__FOR_RELEASE__)
  if (LB::Memory::IsCountEnabled()) {
    memory_stats_updated = true;

    LB::Memory::Info memory_info;
    lb_memory_get_info(&memory_info);

    available_memory_ = memory_info.free_memory;
    c_val_stats_.free_cpu_memory =
        static_cast<size_t>(memory_info.free_memory);
    c_val_stats_.used_cpu_memory =
        static_cast<size_t>(memory_info.application_memory);
    c_val_stats_.exe_memory = static_cast<size_t>(memory_info.executable_size);
  }
#endif  // defined(__LB_SHELL__FOR_RELEASE__)
  // If the memory stats have not been updated yet, then simply use the
  // unallocated memory as the available memory.
  if (!memory_stats_updated) {
    available_memory_ = lb_get_unallocated_memory();
    c_val_stats_.free_cpu_memory = static_cast<size_t>(available_memory_);
    c_val_stats_.used_cpu_memory =
        lb_get_total_system_memory() - lb_get_unallocated_memory();
  }
#elif defined(OS_STARBOARD)
  int64_t used_cpu_memory = SbSystemGetUsedCPUMemory();
  available_memory_ = SbSystemGetTotalCPUMemory() - used_cpu_memory;
  c_val_stats_.free_cpu_memory = available_memory_;
  c_val_stats_.used_cpu_memory = used_cpu_memory;

  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    int64_t used_gpu_memory = SbSystemGetUsedGPUMemory();
    *c_val_stats_.free_gpu_memory =
        SbSystemGetTotalGPUMemory() - used_gpu_memory;
    *c_val_stats_.used_gpu_memory = used_gpu_memory;
  }
#endif

  c_val_stats_.app_lifetime = base::TimeTicks::Now() - start_time_;
}

}  // namespace browser
}  // namespace cobalt
