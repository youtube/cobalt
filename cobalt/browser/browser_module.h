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

#ifndef BROWSER_BROWSER_MODULE_H_
#define BROWSER_BROWSER_MODULE_H_

#include <list>
#include <string>

#include "cobalt/base/console_commands.h"
#include "cobalt/browser/debug_console.h"
#include "cobalt/browser/h5vcc_url_handler.h"
#include "cobalt/browser/render_tree_combiner.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/splash_screen.h"
#include "cobalt/browser/url_handler.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/input/input_device_manager.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/network/network_module.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/script/debug_server.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "cobalt/webdriver/session_driver.h"

namespace cobalt {
namespace browser {

// BrowserModule hosts all major components of the Cobalt browser application.
// It also contains all of the glue components required to connect the
// different subsystems together.
class BrowserModule {
 public:
  // All browser subcomponent options should have default constructors that
  // setup reasonable default options.
  struct Options {
    network::NetworkModule::Options network_module_options;
    renderer::RendererModule::Options renderer_module_options;
    storage::StorageManager::Options storage_manager_options;
    WebModule::Options web_module_options;
    std::string language;
  };

  // Type for a collection of URL handler callbacks that can potentially handle
  // a URL before using it to initialize a new WebModule.
  typedef std::list<URLHandler::URLHandlerCallback> URLHandlerCollection;

  BrowserModule(const GURL& url, system_window::SystemWindow* system_window,
                const Options& options);
  ~BrowserModule();

  const std::string& GetUserAgent() { return network_module_.user_agent(); }

  // Navigation related functions.
  void Navigate(const GURL& url);
  void Reload();

  // Adds/removes a URL handler.
  void AddURLHandler(const URLHandler::URLHandlerCallback& callback);
  void RemoveURLHandler(const URLHandler::URLHandlerCallback& callback);

#if defined(ENABLE_SCREENSHOT)
  // Request a screenshot to be written to the specified path. Callback will
  // be fired after the screenshot has been written to disk.
  void RequestScreenshotToFile(const FilePath& path,
                               const base::Closure& done_cb);

  // Request a screenshot to an in-memory buffer.
  void RequestScreenshotToBuffer(
      const ScreenShotWriter::PNGEncodeCompleteCallback& screenshot_ready);
#endif

#if defined(ENABLE_WEBDRIVER)
  scoped_ptr<webdriver::SessionDriver> CreateSessionDriver(
      const webdriver::protocol::SessionId& session_id);
#endif

#if defined(ENABLE_DEBUG_CONSOLE)
  scoped_ptr<script::DebugServer> CreateDebugServer() {
    return web_module_->CreateDebugServer();
  }
#endif  // ENABLE_DEBUG_CONSOLE

 private:
  // Internal Navigation function and its internal verison. Replaces the current
  // WebModule with a new one that is displaying the specified URL. After
  // navigation, calls the callback if it is not null.
  void NavigateWithCallback(const GURL& url,
                            const base::Closure& loaded_callback);
  void NavigateWithCallbackInternal(const GURL& url,
                                    const base::Closure& loaded_callback);

  // Glue function to deal with the production of the main render tree,
  // and will manage handing it off to the renderer.
  void OnRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  // Glue function to deal with the production of the debug console render tree,
  // and will manage handing it off to the renderer.
  void OnDebugConsoleRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  // Sets the debug console mode if specified on the command-line.
  // Returns true if the debug console mode was set, false otherwise.
  bool SetDebugConsoleModeFromCommandLine();

  // Calls the ExecuteJavascript for the current WebModule.
  std::string ExecuteJavascript(const std::string& script_utf8,
                                const base::SourceLocation& script_location) {
    return web_module_->ExecuteJavascript(script_utf8, script_location);
  }

  // Saves/loads the debug console mode to/from local storage so we can
  // persist the user's preference.
  void SaveDebugConsoleMode();
  void LoadDebugConsoleMode();

  // Glue function to deal with the production of an input event from the
  // input device, and manage handing it off to the web module for
  // interpretation.
  void OnKeyEventProduced(const scoped_refptr<dom::KeyboardEvent>& event);

  // Error callback for any error that stops the program.
  void OnError(const std::string& error);

  // Filters a key event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEvent(const scoped_refptr<dom::KeyboardEvent>& event);

  // Filters a key event for hotkeys.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEventForHotkeys(const scoped_refptr<dom::KeyboardEvent>& event);

  // Message handler callback for trace messages from debug console.
  void OnTraceMessage(const std::string& message);

  void StartOrStopTrace();

  // Tries all registered URL handlers for a URL. Returns true if one of the
  // handlers handled the URL, false if otherwise.
  bool TryURLHandlers(const GURL& url);

  // Destroys the splash screen, if currently displayed.
  void DestroySplashScreen();

#if defined(ENABLE_WEBDRIVER)
  scoped_ptr<webdriver::WindowDriver> CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id) {
    return web_module_->CreateWindowDriver(window_id);
  }
#endif

  // Collection of URL handlers that can potentially handle a URL before
  // using it to initialize a new WebModule.
  URLHandlerCollection url_handlers_;

  storage::StorageManager storage_manager_;

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  renderer::RendererModule renderer_module_;

  // Controls all media playback related objects/resources.
  scoped_ptr<media::MediaModule> media_module_;

  // Sets up the network component for requesting internet resources.
  network::NetworkModule network_module_;

  // Manages the two render trees, combines and renders them.
  RenderTreeCombiner render_tree_combiner_;

  // Sets up everything to do with web page management, from loading and
  // parsing the web page and all referenced files to laying it out.  The
  // web module will ultimately produce a render tree that can be passed
  // into the renderer module.
  scoped_ptr<WebModule> web_module_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Manages debug communication with the web modules.
  scoped_refptr<debug::DebugHub> debug_hub_;

  // Manages a second web module to implement the debug console.
  DebugConsole debug_console_;
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  // The browser module runs on this message loop.
  MessageLoop* const self_message_loop_;

  // Wraps input device and produces input events that can be passed into
  // the web module.
  scoped_ptr<input::InputDeviceManager> input_device_manager_;

  // This object can be set to start a trace if a hotkey (like F3) is pressed.
  // While initialized, it means that a trace is on-going.
  scoped_ptr<trace_event::ScopedTraceToFile> trace_to_file_;

  // Command handler object for trace commands from the debug console.
  base::ConsoleCommandManager::CommandHandler trace_command_handler_;

  // Command handler object for navigate command from the debug console.
  base::ConsoleCommandManager::CommandHandler navigate_command_handler_;

  // Command handler object for reload command from the debug console.
  base::ConsoleCommandManager::CommandHandler reload_command_handler_;

#if defined(ENABLE_SCREENSHOT)
  // Command handler object for screenshot command from the debug console.
  base::ConsoleCommandManager::CommandHandler screenshot_command_handler_;

  // Helper object to create screen shots of the last layout tree.
  scoped_ptr<ScreenShotWriter> screen_shot_writer_;
#endif

  // Handler object for h5vcc URLs.
  H5vccURLHandler h5vcc_url_handler_;

  // WebModule options.
  WebModule::Options web_module_options_;

  // The splash screen. The pointer wrapped here should be non-NULL iff
  // the splash screen is currently displayed.
  scoped_ptr<SplashScreen> splash_screen_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_BROWSER_MODULE_H_
