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
#include "cobalt/input/input_device_manager_fuzzer.h"

namespace cobalt {
namespace browser {
namespace {

// TODO(***REMOVED***): Request viewport size from graphics pipeline and subscribe to
// viewport size changes.
const int kInitialWidth = 1920;
const int kInitialHeight = 1080;

// Files for the debug console web page are bundled with the executable.
const char* kInitialDebugConsoleUrl =
    "file:///cobalt/browser/debug_console/debug_console.html";

}  // namespace

BrowserModule::BrowserModule(const Options& options)
    : storage_manager_(options.storage_manager_options),
      renderer_module_(options.renderer_module_options),
      media_module_(media::MediaModule::Create(
          renderer_module_.pipeline()->GetResourceProvider())),
      network_module_(&storage_manager_),
      ALLOW_THIS_IN_INITIALIZER_LIST(debug_console_(
          base::Bind(&BrowserModule::OnDebugConsoleRenderTreeProduced,
                     base::Unretained(this)),
          base::Bind(&BrowserModule::OnError, base::Unretained(this)),
          media_module_.get(), &network_module_,
          math::Size(kInitialWidth, kInitialHeight),
          renderer_module_.pipeline()->GetResourceProvider(),
          renderer_module_.pipeline()->refresh_rate(),
          WebModule::Options(GURL(kInitialDebugConsoleUrl)))),
      render_tree_combiner_(renderer_module_.pipeline()),
      ALLOW_THIS_IN_INITIALIZER_LIST(web_module_(
          base::Bind(&BrowserModule::OnRenderTreeProduced,
                     base::Unretained(this)),
          base::Bind(&BrowserModule::OnError, base::Unretained(this)),
          media_module_.get(), &network_module_,
          math::Size(kInitialWidth, kInitialHeight),
          renderer_module_.pipeline()->GetResourceProvider(),
          renderer_module_.pipeline()->refresh_rate(),
          options.web_module_options)),
      browser_module_message_loop_(MessageLoop::current()) {
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
    input_device_manager_ =
        input::InputDeviceManager::Create(keyboard_event_callback);
  }

  // Set the initial debug console mode according to the command-line args
  DebugConsole::DebugConsoleMode debug_console_mode =
      DebugConsole::GetDebugConsoleModeFromCommandLine();
  render_tree_combiner_.SetDebugConsoleMode(debug_console_mode);
}

BrowserModule::~BrowserModule() {}

void BrowserModule::OnRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnRenderTreeProduced()");
  render_tree_combiner_.UpdateMainRenderTree(render_tree, node_animations_map);
}

void BrowserModule::OnDebugConsoleRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnDebugConsoleRenderTreeProduced()");
  render_tree_combiner_.UpdateDebugConsoleRenderTree(render_tree,
                                                     node_animations_map);
}

void BrowserModule::OnKeyEventProduced(
    const scoped_refptr<dom::KeyboardEvent>& event) {
  if (MessageLoop::current() != browser_module_message_loop_) {
    browser_module_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnKeyEventProduced,
                              base::Unretained(this), event));
    return;
  }

  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnKeyEventProduced()");
  web_module_.InjectEvent(event);
}

}  // namespace browser
}  // namespace cobalt
