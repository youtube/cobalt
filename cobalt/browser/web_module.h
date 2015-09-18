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

#ifndef BROWSER_WEB_MODULE_H_
#define BROWSER_WEB_MODULE_H_

#include <string>

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/console_commands.h"
#include "cobalt/base/source_location.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/debug/debug_hub.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/local_storage_database.h"
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
// the same thread.
class WebModule {
 public:
  // All browser subcomponent options should have default constructors that
  // setup reasonable default options.
  struct Options {
    Options()
        : name("WebModule"),
          layout_trigger(layout::LayoutManager::kOnDocumentMutation) {}
    explicit Options(const std::string& name)
        : name(name),
          layout_trigger(layout::LayoutManager::kOnDocumentMutation) {}
    Options(const std::string& name,
            const scoped_refptr<debug::DebugHub>& initial_debug_hub)
        : name(name),
          layout_trigger(layout::LayoutManager::kOnDocumentMutation),
          debug_hub(initial_debug_hub) {}

    std::string name;
    layout::LayoutManager::LayoutTrigger layout_trigger;
    scoped_refptr<debug::DebugHub> debug_hub;
  };

  typedef layout::LayoutManager::LayoutResults LayoutResults;
  typedef base::Callback<void(const LayoutResults&)>
      OnRenderTreeProducedCallback;

  WebModule(const GURL& initial_url,
            const OnRenderTreeProducedCallback& render_tree_produced_callback,
            const base::Callback<void(const std::string&)>& error_callback,
            media::MediaModule* media_module,
            network::NetworkModule* network_module,
            const math::Size& window_dimensions,
            render_tree::ResourceProvider* resource_provider,
            float layout_refresh_rate, const Options& options);
  ~WebModule();

  // Call this to inject an event into the web module which will ultimately make
  // its way to the appropriate object in DOM.
  void InjectEvent(const scoped_refptr<dom::Event>& event);

  // Call this to execute Javascript code in this web module.
  std::string ExecuteJavascript(const std::string& script_utf8,
                                const base::SourceLocation& script_location);

  scoped_refptr<dom::Window> window() const { return window_; }

  // Methods to get and set items in the local storage of this web module.
  std::string GetItemInLocalStorage(const std::string& key);
  void SetItemInLocalStorage(const std::string& key, const std::string& value);

#if defined(ENABLE_WEBDRIVER)
  // Create a new webdriver::SessionDriver that interacts with this WebModule
  // instance.
  scoped_ptr<webdriver::SessionDriver> CreateSessionDriver(
      const webdriver::protocol::SessionId& session_id);
#endif

 private:
  // Called by ExecuteJavascript, if that method is called from a different
  // message loop to the one this WebModule is running on. Sets the result
  // output parameter and signals got_result.
  void ExecuteJavascriptInternal(const std::string& script_utf8,
                                 const base::SourceLocation& script_location,
                                 base::WaitableEvent* got_result,
                                 std::string* result);

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  void OnPartialLayoutConsoleCommandReceived(const std::string& message);
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  std::string name_;

  // Thread checker ensures all calls to the WebModule are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;

  // The message loop that this WebModule is running on. If a class method
  // (e.g. ExecuteJavascript) is called from a different message loop, repost
  // it to this one.
  MessageLoop* const self_message_loop_;

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

  // The Window object wraps all DOM-related components.
  scoped_refptr<dom::Window> window_;

  // Cache a WeakPtr in the WebModule that is bound to the Window's message loop
  // so we can ensure that all subsequently created WeakPtr's are also bound to
  // the same loop.
  // See the documentation in base/memory/weak_ptr.h for details.
  base::WeakPtr<dom::Window> window_weak_;

  // Environment Settings object
  scoped_ptr<dom::DOMSettings> environment_settings_;

  // Triggers layout whenever the document changes.
  layout::LayoutManager layout_manager_;

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  // Handles the 'partial_layout' command.
  scoped_ptr<base::ConsoleCommandManager::CommandHandler>
      partial_layout_command_handler_;
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_WEB_MODULE_H_
