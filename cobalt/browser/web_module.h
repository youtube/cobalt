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

#ifndef COBALT_BROWSER_WEB_MODULE_H_
#define COBALT_BROWSER_WEB_MODULE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/console_commands.h"
#include "cobalt/base/source_location.h"
#include "cobalt/css_parser/parser.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/render_overlay.h"
#endif  // ENABLE_DEBUG_CONSOLE
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/math/size.h"
#include "cobalt/media/media_module.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/webdriver/session_driver.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

// WebModule hosts all components of Cobalt that deal with or implement the
// WebAPI.  This includes the ability to fetch resources given a URL, parse
// various web formats like HTML and CSS, host a DOM tree, manage a JavaScript
// engine, and lay out a web page.  Ultimately, interaction with WebModule is
// done through calls to InjectEvent() such as when dealing with external input
// (e.g. keyboards and gamepads), and handling render tree output from WebModule
// when it calls the on_render_tree_produced_ callback (provided upon
// construction).
// It is expected that all components internal to WebModule will operate on
// the same thread.  While it is not currently the case, it is expected that in
// the future, each WebModule will create and host its own private thread upon
// which all subcomponents will run.  Therefore all public methods must be
// thread-safe and callable from any thread.  This necessarily implies that
// details contained within WebModule, such as the DOM, are intentionally kept
// private, since these structures expect to be accessed from only one thread.
class WebModule {
 public:
  // TODO(***REMOVED***): These numbers should be adjusted according to the size of
  // client memory.
  static const uint32 kImageCacheCapacity = 64U * 1024 * 1024;
  static const uint32 kRemoteFontCacheCapacity = 5U * 1024 * 1024;

  struct Options {
    typedef base::Callback<scoped_refptr<script::Wrappable>()>
        CreateObjectFunction;
    typedef base::hash_map<std::string, CreateObjectFunction>
        InjectedWindowAttributes;

    // All optional parameters defined in this structure should have their
    // values
    // initialized in the default constructor to useful defaults.
    Options()
        : name("WebModule"),
          layout_trigger(layout::LayoutManager::kOnDocumentMutation),
          image_cache_capacity(kImageCacheCapacity),
          csp_enforcement_mode(dom::kCspEnforcementEnable) {}

    // The name of the WebModule.  This is useful for debugging purposes as in
    // the case where multiple WebModule objects exist, it can be used to
    // differentiate which objects belong to which WebModule.  It is used
    // to name some CVals.
    std::string name;

    // The LayoutTrigger parameter dictates when a layout should be triggered.
    // Tests will often set this up so that layouts are only performed when
    // we specifically request them to be.
    layout::LayoutManager::LayoutTrigger layout_trigger;

    // Optional directory to add to the search path for web files (file://).
    FilePath extra_web_file_dir;

    // The navigation_callback functor will be called when JavaScript internal
    // to the WebModule requests a page navigation, e.g. by modifying
    // 'window.location.href'.
    base::Callback<void(const GURL&)> navigation_callback;

    // A list of callbacks to be called once the web page finishes loading.
    std::vector<base::Closure> loaded_callbacks;

    // injected_window_attributes contains a map of attributes to be injected
    // into the WebModule's window object upon construction.  This provides
    // a mechanism to inject custom APIs into the WebModule object.
    InjectedWindowAttributes injected_window_attributes;

    // Options to customize DOMSettings.
    dom::DOMSettings::Options dom_settings_options;

    // Location policy to enforce, formatted as a Content Security Policy
    // directive, e.g. "h5vcc-location-src 'self'"
    // This is used to implement a navigation jail, so that location
    // can't be changed from the whitelisted origins.
    std::string location_policy;

    // Image cache capaticy in bytes.
    uint32 image_cache_capacity;

    // Content Security Policy enforcement mode for this web module.
    dom::CspEnforcementType csp_enforcement_mode;
  };

  typedef layout::LayoutManager::LayoutResults LayoutResults;
  typedef base::Callback<void(const LayoutResults&)>
      OnRenderTreeProducedCallback;

  WebModule(const GURL& initial_url,
            const OnRenderTreeProducedCallback& render_tree_produced_callback,
            const base::Callback<void(const GURL&, const std::string&)>&
                error_callback,
            media::MediaModule* media_module,
            network::NetworkModule* network_module,
            const math::Size& window_dimensions,
            render_tree::ResourceProvider* resource_provider,
            float layout_refresh_rate, const Options& options = Options());
  ~WebModule();

  // Call this to inject a keyboard event into the web module.
  void InjectKeyboardEvent(const dom::KeyboardEvent::Data& event);

  // Call this to execute Javascript code in this web module.  The calling
  // thread will block until the JavaScript has executed and the output results
  // are available.
  std::string ExecuteJavascript(const std::string& script_utf8,
                                const base::SourceLocation& script_location);

#if defined(ENABLE_WEBDRIVER)
  // Creates a new webdriver::WindowDriver that interacts with the Window that
  // is owned by this WebModule instance.
  scoped_ptr<webdriver::WindowDriver> CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id);
#endif

#if defined(ENABLE_DEBUG_CONSOLE)
  // Gets a reference to the debug server that interacts with this web module.
  // The debug server is owned by this web module, and is lazily created by this
  // function if necessary.
  debug::DebugServer* GetDebugServer();
#endif  // ENABLE_DEBUG_CONSOLE

 private:
  class DocumentLoadedObserver;

  // Injects a list of custom window attributes into the WebModule's window
  // object.
  void InjectCustomWindowAttributes(
      const Options::InjectedWindowAttributes& attributes);

  // Called by ExecuteJavascript, if that method is called from a different
  // message loop to the one this WebModule is running on. Sets the result
  // output parameter and signals got_result.
  void ExecuteJavascriptInternal(const std::string& script_utf8,
                                 const base::SourceLocation& script_location,
                                 base::WaitableEvent* got_result,
                                 std::string* result);

  // Called by |layout_mananger_| when it produces a render tree. May modify
  // the render tree (e.g. to add a debug overlay), then runs the callback
  // specified in the constructor, |render_tree_produced_callback_|.
  void OnRenderTreeProduced(const LayoutResults& layout_results);

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  void OnPartialLayoutConsoleCommandReceived(const std::string& message);
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  void OnCspPolicyChanged();

  scoped_refptr<script::GlobalObjectProxy> global_object_proxy() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return global_object_proxy_;
  }

  void OnError(const std::string& error) {
    error_callback_.Run(window_->location()->url(), error);
  }

  std::string name_;

  // Thread checker ensures all calls to the WebModule are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;

  // The message loop that this WebModule is running on. If a class method
  // is called from a different message loop, repost it to this one.
  MessageLoop* const self_message_loop_;

  // The error callback passed in from constructor.
  const base::Callback<void(const GURL&, const std::string&)>& error_callback_;

  // CSS parser.
  scoped_ptr<css_parser::Parser> css_parser_;

  // DOM (HTML / XML) parser.
  scoped_ptr<dom_parser::Parser> dom_parser_;

  // FetcherFactory that is used to create a fetcher according to URL.
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;

  // ImageCache that is used to manage image cache logic.
  scoped_ptr<loader::image::ImageCache> image_cache_;

  // RemoteFontCache that is used to manage loading and caching fonts from URLs.
  scoped_ptr<loader::font::RemoteFontCache> remote_font_cache_;

  // Interface between LocalStorage and the Storage Manager.
  dom::LocalStorageDatabase local_storage_database_;

  // JavaScript engine for the browser.
  scoped_ptr<script::JavaScriptEngine> javascript_engine_;

  // JavaScript Global Object for the browser. There should be one per window,
  // but since there is only one window, we can have one per browser.
  scoped_refptr<script::GlobalObjectProxy> global_object_proxy_;

  // Used by |Console| to obtain a JavaScript stack trace.
  scoped_ptr<script::ExecutionState> execution_state_;

  // Interface for the document to execute JavaScript code.
  scoped_ptr<script::ScriptRunner> script_runner_;

  // Object to register and retrieve MediaSource object with a string key.
  dom::MediaSource::Registry media_source_registry_;

  // The Window object wraps all DOM-related components.
  scoped_refptr<dom::Window> window_;

  // Cache a WeakPtr in the WebModule that is bound to the Window's message loop
  // so we can ensure that all subsequently created WeakPtr's are also bound to
  // the same loop.
  // See the documentation in base/memory/weak_ptr.h for details.
  base::WeakPtr<dom::Window> window_weak_;

  // Environment Settings object
  scoped_ptr<dom::DOMSettings> environment_settings_;

  // Called by |OnRenderTreeProduced|.
  OnRenderTreeProducedCallback render_tree_produced_callback_;

  // Triggers layout whenever the document changes.
  layout::LayoutManager layout_manager_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Allows the debugger to add render components to the web module.
  // Used for DOM node highlighting and overlay messages.
  debug::RenderOverlay debug_overlay_;
  render_tree::ResourceProvider* resource_provider_;

  // The core of the debugging system, described here:
  // https://docs.google.com/document/d/1lZhrBTusQZJsacpt21J3kPgnkj7pyQObhFqYktvm40Y
  // Created lazily when accessed via |GetDebugServer|.
  scoped_ptr<debug::DebugServer> debug_server_;
#endif  // ENABLE_DEBUG_CONSOLE

  // DocumentObserver that observes the loading document.
  scoped_ptr<DocumentLoadedObserver> document_load_observer_;

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  // Handles the 'partial_layout' command.
  scoped_ptr<base::ConsoleCommandManager::CommandHandler>
      partial_layout_command_handler_;
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_WEB_MODULE_H_
