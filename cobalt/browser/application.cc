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
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "cobalt/browser/switches.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

namespace {

std::string GetInitialURL() {
  // Allow the user to override the default initial URL via a command line
  // parameter.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInitialURL)) {
    return command_line->GetSwitchValueASCII(switches::kInitialURL);
  }

  static const char kDefaultInitialURL[] =
      "file:///cobalt/browser/testdata/performance-spike/index.html";
  return std::string(kDefaultInitialURL);
}

base::TimeDelta GetTimedTraceDuration() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kTimedTrace) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kTimedTrace),
          &duration_in_seconds)) {
    return base::TimeDelta::FromSeconds(duration_in_seconds);
  } else {
    return base::TimeDelta();
  }
}

}  // namespace

Application::Application()
    : ui_message_loop_(MessageLoop::TYPE_UI) {
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

  // Create the main window for our application.
  main_system_window_ = system_window::CreateSystemWindow();

  // Create the main components of our browser.
  BrowserModule::Options options;
  options.web_module_options.url = url;
  options.renderer_module_options.system_window = main_system_window_.get();
  browser_module_.reset(new BrowserModule(options));
  DLOG(INFO) << "User Agent: " << browser_module_->GetUserAgent();
}

Application::~Application() { DCHECK(!ui_message_loop_.is_running()); }

void Application::Quit() {
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
  }
}

void Application::Run() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kShutdownAfter) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kShutdownAfter),
          &duration_in_seconds)) {
    // If the "shutdown_after" command line option is specified, setup a delayed
    // message to quit the application after the specified number of seconds
    // have passed.
    ui_message_loop_.PostDelayedTask(
        FROM_HERE, quit_closure_,
        base::TimeDelta::FromSeconds(duration_in_seconds));
  }

  run_loop.Run();
}

}  // namespace browser
}  // namespace cobalt
