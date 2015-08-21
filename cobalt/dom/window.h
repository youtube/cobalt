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

#ifndef DOM_WINDOW_H_
#define DOM_WINDOW_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/callback.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/debug/debug_hub.h"
#include "cobalt/dom/animation_frame_request_callback_list.h"
#include "cobalt/dom/crypto.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/parser.h"
#include "cobalt/dom/window_timers.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_runner.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

class Console;
class Document;
class HTMLElementContext;
class LocalStorageDatabase;
class Location;
class Navigator;
class Performance;
class Storage;
class WindowTimers;

// The window object represents a window containing a DOM document.
//   http://www.w3.org/TR/html5/browsers.html#the-window-object
//
// TODO(***REMOVED***): Properly handle viewport resolution change event.
class Window : public EventTarget {
 public:
  typedef AnimationFrameRequestCallbackList::FrameRequestCallback
      FrameRequestCallback;
  typedef WindowTimers::TimerCallback TimerCallback;
  typedef base::Callback<scoped_ptr<loader::Decoder>(
      HTMLElementContext*, const scoped_refptr<Document>&,
      const base::SourceLocation&, const base::Closure&,
      const base::Callback<void(const std::string&)>&)>
      HTMLDecoderCreatorCallback;

  Window(int width, int height, cssom::CSSParser* css_parser,
         Parser* dom_parser, loader::FetcherFactory* fetcher_factory,
         LocalStorageDatabase* local_storage_database,
         media::WebMediaPlayerFactory* web_media_player_factory,
         script::ScriptRunner* script_runner, const GURL& url,
         const std::string& user_agent,
         const base::Callback<void(const std::string&)>& error_callback);

  // Web API: Window
  //
  scoped_refptr<Window> window() { return this; }
  const scoped_refptr<Document>& document() const;
  scoped_refptr<Location> location() const;
  const scoped_refptr<Navigator>& navigator() const;

  // Web API: Timing control for script-based animations (partial interface)
  //   http://www.w3.org/TR/animation-timing/#Window-interface-extensions
  int32 RequestAnimationFrame(const scoped_refptr<FrameRequestCallback>&);
  void CancelAnimationFrame(int32 handle);

  // Web API: CSSOM View Module (partial interface)
  //   http://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-window-interface
  int inner_width() const { return width_; }
  int inner_height() const { return height_; }

  // Web API: GlobalCrypto (implements)
  //   http://www.w3.org/TR/WebCryptoAPI/#crypto-interface
  scoped_refptr<Crypto> crypto() const;

  // Web API: WindowTimers (implements)
  //   http://www.w3.org/TR/html5/webappapis.html#timers
  //
  int SetTimeout(const scoped_refptr<TimerCallback>& handler) {
    return SetTimeout(handler, 0);
  }

  int SetTimeout(const scoped_refptr<TimerCallback>& handler, int timeout);

  void ClearTimeout(int handle);

  // Web API: Storage (implements)
  scoped_refptr<Storage> local_storage() const;
  scoped_refptr<Storage> session_storage() const;

  // Access to the Performance API (partial interface)
  //   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-window.performance-attribute
  const scoped_refptr<Performance>& performance() const;

  // Custom, not in any spec.
  //
  HTMLElementContext* html_element_context() const;

  const scoped_refptr<Console>& console() const;

  // Will fire the animation frame callbacks and reset the animation frame
  // request callback list.
  void RunAnimationFrameCallbacks();

  // Handles debug communication with the main and debug console windows.
  const scoped_refptr<debug::DebugHub>& debug_hub() const;
  void set_debug_hub(const scoped_refptr<debug::DebugHub>& debug_hub);

  DEFINE_WRAPPABLE_TYPE(Window);

 private:
  class RelayLoadEvent;

  ~Window() OVERRIDE;

  int width_;
  int height_;

  scoped_ptr<HTMLElementContext> html_element_context_;
  scoped_refptr<Document> document_;
  scoped_ptr<loader::Loader> document_loader_;
  scoped_refptr<Navigator> navigator_;
  scoped_refptr<Performance> performance_;
  scoped_ptr<RelayLoadEvent> relay_on_load_event_;
  scoped_refptr<Console> console_;
  scoped_ptr<WindowTimers> window_timers_;
  scoped_ptr<AnimationFrameRequestCallbackList>
      animation_frame_request_callback_list_;

  scoped_refptr<Crypto> crypto_;

  scoped_refptr<Storage> local_storage_;
  scoped_refptr<Storage> session_storage_;

  scoped_refptr<debug::DebugHub> debug_hub_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_WINDOW_H_
