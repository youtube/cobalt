// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_BROWSER_MODULE_H_
#define COBALT_BROWSER_BROWSER_MODULE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/account/account_manager.h"
#include "cobalt/base/message_queue.h"
#include "cobalt/browser/h5vcc_url_handler.h"
#include "cobalt/browser/render_tree_combiner.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/splash_screen.h"
#include "cobalt/browser/url_handler.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/input/input_device_manager.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/network/network_module.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/webdriver/session_driver.h"
#include "googleurl/src/gurl.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/base/console_commands.h"
#include "cobalt/browser/debug_console.h"
#include "cobalt/browser/trace_manager.h"
#include "cobalt/debug/debug_server.h"
#endif  // ENABLE_DEBUG_CONSOLE
#include "starboard/configuration.h"

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
    explicit Options(const WebModule::Options& web_options)
        : web_module_options(web_options) {}
    network::NetworkModule::Options network_module_options;
    renderer::RendererModule::Options renderer_module_options;
    storage::StorageManager::Options storage_manager_options;
    WebModule::Options web_module_options;
    media::MediaModule::Options media_module_options;
    std::string language;
    std::string initial_deep_link;
    base::Closure web_module_recreated_callback;
  };

  // Type for a collection of URL handler callbacks that can potentially handle
  // a URL before using it to initialize a new WebModule.
  typedef std::vector<URLHandler::URLHandlerCallback> URLHandlerCollection;

  BrowserModule(const GURL& url, system_window::SystemWindow* system_window,
                account::AccountManager* account_manager,
                const Options& options);
  ~BrowserModule();

  const std::string& GetUserAgent() { return network_module_.GetUserAgent(); }

  // Recreates web module with the given URL.
  void Navigate(const GURL& url);
  // Reloads web module.
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
  debug::DebugServer* GetDebugServer();
  void GetDebugServerInternal(debug::DebugServer** out_debug_server);
#endif  // ENABLE_DEBUG_CONSOLE

  // Change the network proxy settings while the application is running.
  void SetProxy(const std::string& proxy_rules);

  // Suspends the browser module from activity, and releases all graphical
  // resources, placing the application into a low-memory state.
  void Suspend();

  // Undoes the call to Suspend(), returning to normal functionality.
  void Resume();

 private:
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
  static void CoreDumpHandler(void* browser_module_as_void);
  int on_error_triggered_count_;
#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  int recovery_mechanism_triggered_count_;
  int timeout_response_trigger_count_;
#endif  // defined(COBALT_CHECK_RENDER_TIMEOUT)
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)

  // Recreates web module with the given URL.
  void NavigateInternal(const GURL& url);

  // Called when the WebModule's Window.onload event is fired.
  void OnLoad();

  // Wait for the onload event to be fired with the specified timeout. If the
  // webmodule is not currently loading the document, this will return
  // immediately.
  bool WaitForLoad(const base::TimeDelta& timeout);

  // Glue function to deal with the production of the main render tree,
  // and will manage handing it off to the renderer.
  void QueueOnRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);
  void OnRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  // Saves/loads the debug console mode to/from local storage so we can
  // persist the user's preference.
  void SaveDebugConsoleMode();

  // Glue function to deal with the production of an input event from the
  // input device, and manage handing it off to the web module for
  // interpretation.
  void OnKeyEventProduced(const dom::KeyboardEvent::Data& event);

  // Injects a key event directly into the main web module, useful for setting
  // up an input fuzzer whose input should be sent directly to the main
  // web module and not filtered into the debug console.
  void InjectKeyEventToMainWebModule(const dom::KeyboardEvent::Data& event);

  // Error callback for any error that stops the program.
  void OnError(const GURL& url, const std::string& error);

  // Filters a key event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEvent(const dom::KeyboardEvent::Data& event);

  // Filters a key event for hotkeys.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEventForHotkeys(const dom::KeyboardEvent::Data& event);

  // Tries all registered URL handlers for a URL. Returns true if one of the
  // handlers handled the URL, false if otherwise.
  bool TryURLHandlers(const GURL& url);

  // Destroys the splash screen, if currently displayed.
  void DestroySplashScreen();

  // Called when web module has received window.close().
  void OnWindowClose();

  // Called when web module has received window.minimize().
  void OnWindowMinimize();

#if defined(ENABLE_DEBUG_CONSOLE)
  // Toggles the input fuzzer on/off.  Ignores the parameter.
  void OnFuzzerToggle(const std::string&);

  // Use the config in the form of '<string name>=<int value>' to call
  // MediaModule::SetConfiguration().
  void OnSetMediaConfig(const std::string& config);

  // Glue function to deal with the production of the debug console render tree,
  // and will manage handing it off to the renderer.
  void QueueOnDebugConsoleRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);
  void OnDebugConsoleRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);
#endif  // defined(ENABLE_DEBUG_CONSOLE)

#if defined(ENABLE_WEBDRIVER)
  scoped_ptr<webdriver::WindowDriver> CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id);

  void CreateWindowDriverInternal(
      const webdriver::protocol::WindowId& window_id,
      scoped_ptr<webdriver::WindowDriver>* out_window_driver);
#endif

#if defined(OS_STARBOARD)
  // Called when a renderer submission has been rasterized. Used to hide the
  // system splash screen after the first render has completed.
  void OnRendererSubmissionRasterized();
#endif  // OS_STARBOARD

  // Process all messages queued into the |render_tree_submission_queue_|.
  void ProcessRenderTreeSubmissionQueue();

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  // Poll for render timeout. Called from timeout_polling_thread_.
  void OnPollForRenderTimeout(const GURL& url);
#endif

  // TODO:
  //     WeakPtr usage here can be avoided if BrowserModule has a thread to
  //     own where it can ensure that its tasks are all resolved when it is
  //     destructed.
  //
  // BrowserModule provides weak pointers to itself to prevent tasks generated
  // by its components when attempting to communicate to other components (e.g.
  // input events being generated and passed into WebModule).  By passing
  // references to BrowserModule as WeakPtrs, we do not access invalid memory
  // if they are processed after the BrowserModule is destroyed.
  base::WeakPtrFactory<BrowserModule> weak_ptr_factory_;
  // After constructing |weak_ptr_factory_| we immediately construct a WeakPtr
  // in order to bind the BrowserModule WeakPtr object to its thread.  When we
  // need a WeakPtr, we copy construct this, which is safe to do from any
  // thread according to weak_ptr.h (versus calling
  // |weak_ptr_factory_.GetWeakPtr() which is not).
  base::WeakPtr<BrowserModule> weak_this_;

  // The browser module runs on this message loop.
  MessageLoop* const self_message_loop_;

  // Collection of URL handlers that can potentially handle a URL before
  // using it to initialize a new WebModule.
  URLHandlerCollection url_handlers_;

  storage::StorageManager storage_manager_;

#if defined(OS_STARBOARD)
  // Whether the browser module has yet rendered anything. On the very first
  // render, we hide the system splash screen.
  bool is_rendered_;
#endif  // OS_STARBOARD

  // Wraps input device and produces input events that can be passed into
  // the web module.
  scoped_ptr<input::InputDeviceManager> input_device_manager_;

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  renderer::RendererModule renderer_module_;

  // Optional memory allocator used by ArrayBuffer.
  scoped_ptr<dom::ArrayBuffer::Allocator> array_buffer_allocator_;

  // Optional cache used by ArrayBuffer.
  scoped_ptr<dom::ArrayBuffer::Cache> array_buffer_cache_;

  // Controls all media playback related objects/resources.
  scoped_ptr<media::MediaModule> media_module_;

  // Sets up the network component for requesting internet resources.
  network::NetworkModule network_module_;

  // Manages the two render trees, combines and renders them.
  RenderTreeCombiner render_tree_combiner_;

#if defined(ENABLE_SCREENSHOT)
  // Helper object to create screen shots of the last layout tree.
  scoped_ptr<ScreenShotWriter> screen_shot_writer_;
#endif  // defined(ENABLE_SCREENSHOT)

  // Keeps track of all messages containing render tree submissions that will
  // ultimately reference the |render_tree_combiner_| and the
  // |renderer_module_|.  It is important that this is destroyed before the
  // above mentioned references are.  It must however outlive all WebModules
  // that may be producing render trees.
  base::MessageQueue render_tree_submission_queue_;

  // Sets up everything to do with web page management, from loading and
  // parsing the web page and all referenced files to laying it out.  The
  // web module will ultimately produce a render tree that can be passed
  // into the renderer module.
  scoped_ptr<WebModule> web_module_;

  // Will be signalled when the WebModule's Window.onload event is fired.
  base::WaitableEvent web_module_loaded_;

  // This will be called after the WebModule has been destroyed and recreated,
  // which could occur on navigation.
  base::Closure web_module_recreated_callback_;

  // The time when a URL navigation starts. This is recorded after the previous
  // WebModule is destroyed.
  base::CVal<int64> navigate_time_;

  // The time when the WebModule's Window.onload event is fired.
  base::CVal<int64> on_load_event_time_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Possibly null, but if not, will contain a reference to an instance of
  // a debug fuzzer input device manager.
  scoped_ptr<input::InputDeviceManager> input_device_manager_fuzzer_;

  // Manages a second web module to implement the debug console.
  scoped_ptr<DebugConsole> debug_console_;

  TraceManager trace_manager_;

  // Command handler object for toggling the input fuzzer on/off.
  base::ConsoleCommandManager::CommandHandler fuzzer_toggle_command_handler_;

  // Command handler object for setting media module config.
  base::ConsoleCommandManager::CommandHandler set_media_config_command_handler_;

#if defined(ENABLE_SCREENSHOT)
  // Command handler object for screenshot command from the debug console.
  base::ConsoleCommandManager::CommandHandler screenshot_command_handler_;
#endif  // defined(ENABLE_SCREENSHOT)
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  // Handler object for h5vcc URLs.
  H5vccURLHandler h5vcc_url_handler_;

  // WebModule options.
  WebModule::Options web_module_options_;

  // The splash screen. The pointer wrapped here should be non-NULL iff
  // the splash screen is currently displayed.
  scoped_ptr<SplashScreen> splash_screen_;

  // Reset when the browser is paused, signalled to resume.
  base::WaitableEvent has_resumed_;

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  base::Thread timeout_polling_thread_;

  // Counts the number of continuous render timeout expirations. This value is
  // updated and used from OnPollForRenderTimeout.
  int render_timeout_count_;
#endif

  // Set when the application is about to quit. May be set from a thread other
  // than the one hosting this object, and read from another.
  bool will_quit_;

  // The |will_quit_| flag may be set from one thread (e.g. not the one hosting
  // this object) and read from another. This lock is used to
  // ensure synchronous access.
  base::Lock quit_lock_;

  bool suspended_;

  system_window::SystemWindow* system_window_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_BROWSER_MODULE_H_
