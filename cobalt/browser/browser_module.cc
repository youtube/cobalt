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
#include "cobalt/dom/keyboard_code.h"
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

}  // namespace

BrowserModule::BrowserModule(const GURL& url, const Options& options)
    : main_system_window_(system_window::CreateSystemWindow()),
      storage_manager_(options.storage_manager_options),
      renderer_module_(main_system_window_.get(),
                       options.renderer_module_options),
      media_module_(media::MediaModule::Create(
          renderer_module_.pipeline()->GetResourceProvider())),
      network_module_(&storage_manager_, options.language),
      render_tree_combiner_(renderer_module_.pipeline()),
      ALLOW_THIS_IN_INITIALIZER_LIST(web_module_(
          url, base::Bind(&BrowserModule::OnRenderTreeProduced,
                          base::Unretained(this)),
          base::Bind(&BrowserModule::OnError, base::Unretained(this)),
          media_module_.get(), &network_module_,
          math::Size(kInitialWidth, kInitialHeight),
          renderer_module_.pipeline()->GetResourceProvider(),
          renderer_module_.pipeline()->refresh_rate(),
          options.web_module_options)),
      debug_hub_(new debug::DebugHub(base::Bind(
          &WebModule::ExecuteJavascript, base::Unretained(&web_module_)))),
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
      self_message_loop_(MessageLoop::current()) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  input::KeyboardEventCallback keyboard_event_callback =
      base::Bind(&BrowserModule::OnKeyEventProduced, base::Unretained(this));

  // If the user has asked to activate the input fuzzer, then we wire up the
  // input fuzzer key generator to our keyboard event callback.  Otherwise, we
  // create and connect the platform-specific input event generator.
  if (command_line->HasSwitch(switches::kInputFuzzer)) {
    input_device_manager_ = scoped_ptr<input::InputDeviceManager>(
        new input::InputDeviceManagerFuzzer(keyboard_event_callback));
  } else {
    input_device_manager_ = input::InputDeviceManager::CreateFromWindow(
        keyboard_event_callback, main_system_window_.get());
  }

  // Set the initial debug console mode according to the command-line args
  debug_hub_->SetDebugConsoleMode(GetDebugConsoleModeFromCommandLine());

  // Always render the debug console. It will draw nothing if disabled.
  // This setting is ignored if ENABLE_DEBUG_CONSOLE is not defined.
  // TODO(***REMOVED***) Render tree combiner should probably be refactored.
  render_tree_combiner_.set_render_debug_console(true);
}

BrowserModule::~BrowserModule() {}

void BrowserModule::OnRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map,
    base::TimeDelta time_produced) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnRenderTreeProduced()");
  render_tree_combiner_.UpdateMainRenderTree(renderer::Pipeline::Submission(
      render_tree, node_animations_map, time_produced));
}

void BrowserModule::OnDebugConsoleRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map,
    base::TimeDelta time_produced) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnDebugConsoleRenderTreeProduced()");
  render_tree_combiner_.UpdateDebugConsoleRenderTree(
      renderer::Pipeline::Submission(render_tree, node_animations_map,
                                     time_produced));
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
    web_module_.InjectEvent(event);
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
#if defined(ENABLE_DEBUG_CONSOLE)
  if (event->ctrl_key() && event->key_code() == dom::kO) {
    if (event->type() == dom::EventNames::GetInstance()->keydown()) {
      // Ctrl+O toggles the debug console display.
      debug_hub_->CycleDebugConsoleMode();
    }
    return false;
  }

  if (event->key_code() == dom::kF3) {
    if (event->type() == dom::EventNames::GetInstance()->keydown()) {
      CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(switches::kTimedTrace)) {
        DLOG(WARNING)
            << "Cannot manually trigger a trace when timed_trace is active.";
      } else {
        static const char* kOutputTraceFilename = "triggered_trace.json";
        if (trace_to_file_) {
          DLOG(INFO) << "Ending trace.";
          DLOG(INFO) << "Trace results in file \"" << kOutputTraceFilename
                     << "\"";
          trace_to_file_.reset();
        } else {
          DLOG(INFO) << "Starting trace...";
          trace_to_file_.reset(new trace_event::ScopedTraceToFile(
              FilePath(kOutputTraceFilename)));
        }
      }
    }
    return false;
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  return true;
}

// Static
int BrowserModule::GetDebugConsoleModeFromCommandLine() {
  int debug_console_mode = debug::DebugHub::kDebugConsoleHud;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDebugConsoleMode)) {
    const std::string debug_console_mode_string =
        command_line->GetSwitchValueASCII(switches::kDebugConsoleMode);
    if (debug_console_mode_string == "off") {
      debug_console_mode = debug::DebugHub::kDebugConsoleOff;
    } else if (debug_console_mode_string == "hud") {
      debug_console_mode = debug::DebugHub::kDebugConsoleHud;
    } else if (debug_console_mode_string == "on") {
      debug_console_mode = debug::DebugHub::kDebugConsoleOn;
    }
  }

  return debug_console_mode;
}

}  // namespace browser
}  // namespace cobalt
