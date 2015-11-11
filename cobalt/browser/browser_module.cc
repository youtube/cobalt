/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/browser_module.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cobalt/browser/switches.h"
#include "cobalt/dom/event_names.h"
#include "cobalt/dom/keycode.h"
#include "cobalt/input/input_device_manager_fuzzer.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace browser {
namespace {

// TODO(***REMOVED***): Request viewport size from graphics pipeline and subscribe to
// viewport size changes.
const int kInitialWidth = 1920;
const int kInitialHeight = 1080;

// Files for the debug console web page are bundled with the executable.
const char kInitialDebugConsoleUrl[] =
    "file:///cobalt/browser/debug_console/debug_console.html";

// Name of the channel to listen for trace commands from the debug console.
const char kTraceCommandChannel[] = "trace";

// Help strings for the trace command channel.
const char kTraceCommandShortHelp[] = "Starts/stops execution tracing.";
const char kTraceCommandLongHelp[] =
    "Starts/stops execution tracing.\n"
    "If a trace is currently running, stops it and saves the result; "
    "otherwise starts a new trace.";

// Command to navigate to a URL.
const char kNavigateCommand[] = "navigate";

// Help strings for the navigate command.
const char kNavigateCommandShortHelp[] = "Navigates to URL.";
const char kNavigateCommandLongHelp[] =
    "Destroys the current document and window and creates a new one in its "
    "place that displays the specified URL.";

// Command to reload the current URL.
const char kReloadCommand[] = "reload";

// Help strings for the navigate command.
const char kReloadCommandShortHelp[] = "Reloads the current URL.";
const char kReloadCommandLongHelp[] =
    "Destroys the current document and window and creates a new one in its "
    "place that displays the current URL.";

#if defined(ENABLE_DEBUG_CONSOLE)
// Local storage key for the debug console mode.
const char kDebugConsoleModeKey[] = "debugConsole.mode";
#endif  // defined(ENABLE_DEBUG_CONSOLE)

void OnNavigateMessage(BrowserModule* browser_module,
                       const std::string& message) {
  GURL url(message);
  DLOG_IF(WARNING, !url.is_valid()) << "Invalid URL: " << message;
  if (url.is_valid()) {
    browser_module->Navigate(url, base::Closure());
  }
}

void OnReloadMessage(BrowserModule* browser_module,
                     const std::string& message) {
  UNREFERENCED_PARAMETER(message);
  browser_module->Reload();
}
}  // namespace

BrowserModule::BrowserModule(const GURL& url,
                             system_window::SystemWindow* system_window,
                             const Options& options)
    : storage_manager_(options.storage_manager_options),
      renderer_module_(system_window, options.renderer_module_options),
      media_module_(media::MediaModule::Create(
          renderer_module_.pipeline()->GetResourceProvider())),
      network_module_(&storage_manager_, system_window->event_dispatcher(),
                      options.language),
      render_tree_combiner_(renderer_module_.pipeline()),
#if defined(ENABLE_DEBUG_CONSOLE)
      ALLOW_THIS_IN_INITIALIZER_LIST(debug_hub_(new debug::DebugHub(
          base::Bind(&BrowserModule::ExecuteJavascript, base::Unretained(this)),
          base::Bind(&BrowserModule::CreateDebugServer,
                     base::Unretained(this))))),
#endif  // ENABLE_DEBUG_CONSOLE
      ALLOW_THIS_IN_INITIALIZER_LIST(debug_console_(
          GURL(kInitialDebugConsoleUrl),
          base::Bind(&BrowserModule::OnDebugConsoleRenderTreeProduced,
                     base::Unretained(this)),
          base::Bind(&BrowserModule::OnError, base::Unretained(this)),
          media_module_.get(), &network_module_,
          math::Size(kInitialWidth, kInitialHeight),
          renderer_module_.pipeline()->GetResourceProvider(),
          renderer_module_.pipeline()->refresh_rate(),
          WebModule::Options("DebugConsoleWebModule", debug_hub_))),
      self_message_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(trace_command_handler_(
          kTraceCommandChannel,
          base::Bind(&BrowserModule::OnTraceMessage, base::Unretained(this)),
          kTraceCommandShortHelp, kTraceCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(navigate_command_handler_(
          kNavigateCommand,
          base::Bind(&OnNavigateMessage, base::Unretained(this)),
          kNavigateCommandShortHelp, kNavigateCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(reload_command_handler_(
          kReloadCommand, base::Bind(&OnReloadMessage, base::Unretained(this)),
          kReloadCommandShortHelp, kReloadCommandLongHelp)),
      web_module_options_(options.web_module_options) {
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  input::KeyboardEventCallback keyboard_event_callback =
      base::Bind(&BrowserModule::OnKeyEventProduced, base::Unretained(this));

  bool use_input_fuzzer = false;

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(switches::kInputFuzzer)) {
    use_input_fuzzer = true;
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  // If the user has asked to activate the input fuzzer, then we wire up the
  // input fuzzer key generator to our keyboard event callback.  Otherwise, we
  // create and connect the platform-specific input event generator.
  if (use_input_fuzzer) {
    input_device_manager_ = scoped_ptr<input::InputDeviceManager>(
        new input::InputDeviceManagerFuzzer(keyboard_event_callback));
  } else {
    input_device_manager_ = input::InputDeviceManager::CreateFromWindow(
        keyboard_event_callback, system_window);
  }

  // Debug console defaults to Off if not specified.
  // Stored preference overrides this, if it exists.
  // Command line setting overrides this, if specified.
  debug_hub_->SetDebugConsoleMode(debug::DebugHub::kDebugConsoleOff);
  LoadDebugConsoleMode();
  SetDebugConsoleModeFromCommandLine();

  // Always render the debug console. It will draw nothing if disabled.
  // This setting is ignored if ENABLE_DEBUG_CONSOLE is not defined.
  // TODO(***REMOVED***) Render tree combiner should probably be refactored.
  render_tree_combiner_.set_render_debug_console(true);

  NavigateInternal(url, base::Closure());
}

void BrowserModule::Navigate(const GURL& url,
                             const base::Closure& loaded_callback) {
  // Always post this as a task in case this is being called from the WebModule.
  self_message_loop_->PostTask(
      FROM_HERE, base::Bind(&BrowserModule::NavigateInternal,
                            base::Unretained(this), url, loaded_callback));
}

void BrowserModule::NavigateInternal(const GURL& url,
                                     const base::Closure& loaded_callback) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);

  // Reset it explicitly first, so we don't get a memory high-watermark after
  // the second WebModule's construtor runs, but before scoped_ptr::reset() is
  // run.
  web_module_.reset(NULL);

  WebModule::Options options(web_module_options_);
  if (!loaded_callback.is_null()) {
    options.loaded_callbacks.push_back(loaded_callback);
  }

  web_module_.reset(new WebModule(
      url,
      base::Bind(&BrowserModule::OnRenderTreeProduced, base::Unretained(this)),
      base::Bind(&BrowserModule::OnError, base::Unretained(this)),
      media_module_.get(), &network_module_,
      math::Size(kInitialWidth, kInitialHeight),
      renderer_module_.pipeline()->GetResourceProvider(),
      renderer_module_.pipeline()->refresh_rate(), options));

  h5vcc::H5vcc::Settings h5vcc_settings;
  h5vcc_settings.network_module = &network_module_;
  web_module_->window()->set_h5vcc(new h5vcc::H5vcc(h5vcc_settings));
}

void BrowserModule::Reload() {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
      FROM_HERE, base::Bind(&BrowserModule::Reload, base::Unretained(this)));
    return;
  }
  Navigate(web_module_->url(), base::Closure());
}

BrowserModule::~BrowserModule() {}

void BrowserModule::OnRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnRenderTreeProduced()");
  render_tree_combiner_.UpdateMainRenderTree(renderer::Pipeline::Submission(
      layout_results.render_tree, layout_results.animations,
      layout_results.layout_time));
}

void BrowserModule::OnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnDebugConsoleRenderTreeProduced()");
  render_tree_combiner_.UpdateDebugConsoleRenderTree(
      renderer::Pipeline::Submission(layout_results.render_tree,
                                     layout_results.animations,
                                     layout_results.layout_time));
}

void BrowserModule::OnKeyEventProduced(
    const scoped_refptr<dom::KeyboardEvent>& event) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(FROM_HERE,
                                 base::Bind(&BrowserModule::OnKeyEventProduced,
                                            base::Unretained(this), event));
    return;
  }

  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnKeyEventProduced()");

  // Filter the key event and inject into the web module if it wasn't
  // processed anywhere else.
  if (FilterKeyEvent(event)) {
    web_module_->InjectEvent(event);
  }
}

bool BrowserModule::FilterKeyEvent(
    const scoped_refptr<dom::KeyboardEvent>& event) {
  // Check for hotkeys first. If it is a hotkey, no more processing is needed.
  if (!FilterKeyEventForHotkeys(event)) {
    return false;
  }

  // If the debug console is fully visible, it gets the next chance to handle
  // key events.
  if (debug_hub_->GetDebugConsoleMode() >= debug::DebugHub::kDebugConsoleOn) {
    if (!debug_console_.FilterKeyEvent(event)) {
      return false;
    }
  }

  return true;
}

bool BrowserModule::FilterKeyEventForHotkeys(
    const scoped_refptr<dom::KeyboardEvent>& event) {
#if !defined(ENABLE_DEBUG_CONSOLE)
  UNREFERENCED_PARAMETER(event);
#else
  if (event->key_code() == dom::keycode::kF1 ||
      (event->ctrl_key() && event->key_code() == dom::keycode::kO)) {
    if (event->type() == dom::EventNames::GetInstance()->keydown()) {
      // Ctrl+O toggles the debug console display.
      debug_hub_->CycleDebugConsoleMode();
      // Persist the new debug console mode to web local storage.
      SaveDebugConsoleMode();
    }
    return false;
  }

  if (event->key_code() == dom::keycode::kF3) {
    if (event->type() == dom::EventNames::GetInstance()->keydown()) {
      StartOrStopTrace();
    }
    return false;
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  return true;
}

void BrowserModule::OnTraceMessage(const std::string& message) {
  UNREFERENCED_PARAMETER(message);
  if (MessageLoop::current() == self_message_loop_) {
    StartOrStopTrace();
  } else {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::StartOrStopTrace, base::Unretained(this)));
  }
}

void BrowserModule::StartOrStopTrace() {
  bool timed_trace_active = false;

#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTimedTrace)) {
    timed_trace_active = true;
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  if (timed_trace_active) {
    DLOG(WARNING)
        << "Cannot manually trigger a trace when timed_trace is active.";
  } else {
    static const char* kOutputTraceFilename = "triggered_trace.json";
    if (trace_to_file_) {
      DLOG(INFO) << "Ending trace.";
      DLOG(INFO) << "Trace results in file \"" << kOutputTraceFilename << "\"";
      trace_to_file_.reset();
    } else {
      DLOG(INFO) << "Starting trace...";
      trace_to_file_.reset(
          new trace_event::ScopedTraceToFile(FilePath(kOutputTraceFilename)));
    }
  }
}

void BrowserModule::SaveDebugConsoleMode() {
#if defined(ENABLE_DEBUG_CONSOLE)
  const std::string mode_string = debug_hub_->GetDebugConsoleModeAsString();
  debug_console_.web_module().SetItemInLocalStorage(kDebugConsoleModeKey,
                                                    mode_string);
#endif  // defined(ENABLE_DEBUG_CONSOLE)
}

void BrowserModule::LoadDebugConsoleMode() {
#if defined(ENABLE_DEBUG_CONSOLE)
  const std::string mode_string =
      debug_console_.web_module().GetItemInLocalStorage(kDebugConsoleModeKey);
  debug_hub_->SetDebugConsoleModeAsString(mode_string);
#endif  // defined(ENABLE_DEBUG_CONSOLE)
}

bool BrowserModule::SetDebugConsoleModeFromCommandLine() {
#if defined(ENABLE_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDebugConsoleMode)) {
    const std::string debug_console_mode_string =
        command_line->GetSwitchValueASCII(switches::kDebugConsoleMode);
    debug_hub_->SetDebugConsoleModeAsString(debug_console_mode_string);
    return true;
  }
#endif  // ENABLE_COMMAND_LINE_SWITCHES

  return false;
}

#if defined(ENABLE_WEBDRIVER)
scoped_ptr<webdriver::SessionDriver> BrowserModule::CreateSessionDriver(
    const webdriver::protocol::SessionId& session_id) {
  return make_scoped_ptr(new webdriver::SessionDriver(session_id,
      base::Bind(&BrowserModule::Navigate, base::Unretained(this)),
      base::Bind(&BrowserModule::CreateWindowDriver, base::Unretained(this))));
}
#endif

}  // namespace browser
}  // namespace cobalt
