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
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "cobalt/account/account_event.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/localized_strings.h"
#include "cobalt/browser/switches.h"
#include "cobalt/deprecated/platform_delegate.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/math/size.h"
#include "cobalt/network/network_event.h"
#include "cobalt/system_window/application_event.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

namespace {
const int kDefaultViewportWidth = 1920;
const int kDefaultViewportHeight = 1080;

const char kDefaultURL[] = "https://www.youtube.com/tv";

#if defined(ENABLE_REMOTE_DEBUGGING)
int GetRemoteDebuggingPort() {
  const int kDefaultRemoteDebuggingPort = 9222;
  int remote_debugging_port = kDefaultRemoteDebuggingPort;
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
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
#endif  // ENABLE_COMMAND_LINE_SWITCHES
  return remote_debugging_port;
}
#endif  // ENABLE_REMOTE_DEBUGGING

#if defined(ENABLE_WEBDRIVER)
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
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
#endif  // ENABLE_COMMAND_LINE_SWITCHES
#endif  // ENABLE_WEBDRIVER

GURL GetInitialURL() {
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
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
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  return GURL(kDefaultURL);
}

base::TimeDelta GetTimedTraceDuration() {
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kTimedTrace) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kTimedTrace),
          &duration_in_seconds)) {
    return base::TimeDelta::FromSeconds(duration_in_seconds);
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  return base::TimeDelta();
}

FilePath GetExtraWebFileDir() {
  // Default is empty, command line can override.
  FilePath result;

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
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
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  return result;
}

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
void EnableUsingStubImageDecoderIfRequired() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kStubImageDecoder)) {
    loader::image::ImageDecoder::UseStubImageDecoder();
  }
}
#endif  // ENABLE_COMMAND_LINE_SWITCHES

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

}  // namespace

Application::Application()
    : message_loop_(MessageLoop::TYPE_UI) {
  base::PlatformThread::SetName("Main");
  message_loop_.set_thread_name("Main");

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
  options.network_module_options.preferred_language = language;
  // User can specify an extra search path entry for files loaded via file://.
  options.web_module_options.extra_web_file_dir = GetExtraWebFileDir();
  options.web_module_options.location_policy = kYouTubeTvLocationPolicy;
  // Set callback to be notified when a navigation occurs that destroys the
  // underlying WebModule.
  options.web_module_recreated_callback =
      base::Bind(&Application::WebModuleRecreated, base::Unretained(this));

  math::Size viewport_size(kDefaultViewportWidth, kDefaultViewportHeight);

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
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

  if (command_line->HasSwitch(browser::switches::kViewport)) {
    const std::string switchValue =
        command_line->GetSwitchValueASCII(browser::switches::kViewport);
    std::vector<std::string> lengths;
    base::SplitString(switchValue, 'x', &lengths);
    if (lengths.size() >= 1) {
      int width;
      if (!base::StringToInt(lengths[0], &width) || width < 1) {
        DLOG(ERROR) << "Invalid value specified for viewport width: "
                    << switchValue
                    << ". Using default viewport: " << viewport_size.width()
                    << "x" << viewport_size.height();
      } else {
        viewport_size.set_width(width);
        if (lengths.size() >= 2) {
          int height;
          if (!base::StringToInt(lengths[1], &height) || height < 1) {
            DLOG(ERROR) << "Invalid value specified for viewport height: "
                        << switchValue << ". Using default viewport height: "
                        << viewport_size.width() << "x"
                        << viewport_size.height();
          } else {
            viewport_size.set_height(height);
          }
        } else {
          // Allow shorthand specification of the viewport by only giving the
          // width. This calculates the height at 4:3 aspect ratio for smaller
          // viewport widths, and 16:9 for viewports 1280 pixels wide or larger.
          if (viewport_size.width() >= 1280) {
            viewport_size.set_height(9 * viewport_size.width() / 16);
          } else {
            viewport_size.set_height(3 * viewport_size.width() / 4);
          }
        }
      }
    }
  }
  if (command_line->HasSwitch(switches::kAudioDecoderStub)) {
    DLOG(INFO) << "Use ShellRawAudioDecoderStub";
    options.media_module_options.use_audio_decoder_stub = true;
  }
  if (command_line->HasSwitch(switches::kNullAudioStreamer)) {
    DLOG(INFO) << "Use null audio";
    options.media_module_options.use_null_audio_streamer = true;
  }
  if (command_line->HasSwitch(switches::kVideoDecoderStub)) {
    DLOG(INFO) << "Use ShellRawVideoDecoderStub";
    options.media_module_options.use_video_decoder_stub = true;
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  system_window_ =
      system_window::CreateSystemWindow(&event_dispatcher_, viewport_size);
  account_manager_ = account::AccountManager::Create(&event_dispatcher_);
  browser_module_.reset(new BrowserModule(initial_url, system_window_.get(),
                                          account_manager_.get(), options));
  DLOG(INFO) << "User Agent: " << browser_module_->GetUserAgent();

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

#if defined(ENABLE_WEBDRIVER)
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
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
#endif  // ENABLE_COMMAND_LINE_SWITCHES
#endif  // ENABLE_WEBDRIVER

#if defined(ENABLE_REMOTE_DEBUGGING)
  int remote_debugging_port = GetRemoteDebuggingPort();
  debug_web_server_.reset(new debug::DebugWebServer(
      remote_debugging_port,
      base::Bind(&BrowserModule::CreateDebugServer,
                 base::Unretained(browser_module_.get()))));
#endif  // ENABLE_REMOTE_DEBUGGING
}

Application::~Application() {
  DCHECK(!message_loop_.is_running());

  // Unregister event callbacks.
  event_dispatcher_.RemoveEventCallback(account::AccountEvent::TypeId(),
                                        account_event_callback_);
  event_dispatcher_.RemoveEventCallback(network::NetworkEvent::TypeId(),
                                        network_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      system_window::ApplicationEvent::TypeId(), application_event_callback_);
}

void Application::Quit() {
  if (MessageLoop::current() == &message_loop_) {
    if (!quit_closure_.is_null()) {
      quit_closure_.Run();
    }
  } else {
    message_loop_.PostTask(FROM_HERE, quit_closure_);
  }
}

void Application::Run() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kShutdownAfter) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kShutdownAfter),
          &duration_in_seconds)) {
    // If the "shutdown_after" command line option is specified, setup a delayed
    // message to quit the application after the specified number of seconds
    // have passed.
    message_loop_.PostDelayedTask(
        FROM_HERE, quit_closure_,
        base::TimeDelta::FromSeconds(duration_in_seconds));
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  run_loop.Run();
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
  const network::NetworkEvent* network_event =
      base::polymorphic_downcast<const network::NetworkEvent*>(event);
  if (network_event->type() == network::NetworkEvent::kDisconnection) {
    browser_module_->Navigate(GURL("h5vcc://network-failure"));
  }
}

void Application::OnApplicationEvent(const base::Event* event) {
  const system_window::ApplicationEvent* app_event =
      base::polymorphic_downcast<const system_window::ApplicationEvent*>(event);
  if (app_event->type() == system_window::ApplicationEvent::kQuit) {
    DLOG(INFO) << "Got quit event.";
    browser_module_->SetWillQuit();
    browser_module_->SetPaused(false);
    Quit();
  } else if (app_event->type() == system_window::ApplicationEvent::kSuspend) {
    DLOG(INFO) << "Got suspend event.";
    browser_module_->SetPaused(true);
  } else if (app_event->type() == system_window::ApplicationEvent::kResume) {
    DLOG(INFO) << "Got resume event.";
    browser_module_->SetPaused(false);
  }
}

void Application::WebModuleRecreated() {
#if defined(ENABLE_WEBDRIVER)
  if (web_driver_module_) {
    web_driver_module_->OnWindowRecreated();
  }
#endif
}
}  // namespace browser
}  // namespace cobalt
