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

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "cobalt/account/account_event.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/localized_strings.h"
#include "cobalt/browser/switches.h"
#include "cobalt/deprecated/platform_delegate.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

namespace {

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

std::string GetInitialURL() {
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  // Allow the user to override the default initial URL via a command line
  // parameter.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInitialURL)) {
    return command_line->GetSwitchValueASCII(switches::kInitialURL);
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  static const char kDefaultInitialURL[] = "https://youtube.com/tv";
  return std::string(kDefaultInitialURL);
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

  GURL url = GURL(GetInitialURL());
  if (!url.is_valid()) {
    DLOG(INFO) << "Initial URL is not valid, using empty URL.";
    url = GURL();
  }
  DLOG(INFO) << "Initial URL: " << url;

  // Get the system language and initialize our localized strings.
  std::string language =
      cobalt::deprecated::PlatformDelegate::Get()->GetSystemLanguage();
  base::LocalizedStrings::GetInstance()->Initialize(language);

  // Create the main components of our browser.
  BrowserModule::Options options;
  options.web_module_options.name = "MainWebModule";
  options.language = language;
  options.network_module_options.preferred_language = language;

#if !defined(COBALT_FORCE_HTTPS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAllowHttp)) {
    DLOG(INFO) << "Allowing insecure HTTP connections";
    options.network_module_options.require_https = false;
  }
#endif  // !defined(COBALT_FORCE_HTTPS)

  // User can specify an extra search path entry for files loaded via file://.
  options.web_module_options.extra_web_file_dir = GetExtraWebFileDir();
  system_window_ = system_window::CreateSystemWindow(&event_dispatcher_);
  browser_module_.reset(new BrowserModule(url, system_window_.get(), options));
  DLOG(INFO) << "User Agent: " << browser_module_->GetUserAgent();

  // Register our callback for events from the account manager.
  account_event_callback_ =
      base::Bind(&Application::OnAccountEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(account::AccountEvent::TypeId(),
                                     account_event_callback_);

#if defined(ENABLE_WEBDRIVER)
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableWebDriver)) {
    int webdriver_port = GetWebDriverPort();
    web_driver_module_.reset(new webdriver::WebDriverModule(
        webdriver_port, base::Bind(&BrowserModule::CreateSessionDriver,
                                   base::Unretained(browser_module_.get())),
        base::Bind(&BrowserModule::RequestScreenshotToBuffer,
                   base::Unretained(browser_module_.get())),
        base::Bind(&Application::Quit, base::Unretained(this))));
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES
#endif  // ENABLE_WEBDRIVER
}

Application::~Application() {
  DCHECK(!message_loop_.is_running());

  // Unregister our callback for account manager events.
  event_dispatcher_.RemoveEventCallback(account::AccountEvent::TypeId(),
                                        account_event_callback_);
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
    DLOG(INFO) << "Got signed in event!";
  } else if (account_event->type() == account::AccountEvent::kSignedOut) {
    DLOG(INFO) << "Got signed out event!";
    browser_module_->Navigate(GURL("h5vcc://signed-out"));
  }
}

}  // namespace browser
}  // namespace cobalt
