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
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_runner.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

class Console;
class Document;
class DocumentBuilder;
class HTMLElementContext;
class Location;
class Navigator;
class WindowTimers;

// The window object represents a window containing a DOM document.
//   http://www.w3.org/TR/html5/browsers.html#the-window-object
//
// TODO(***REMOVED***): Properly handle viewport resolution change event.
class Window : public EventTarget {
 public:
  typedef script::CallbackFunction<void(double)> FrameRequestCallback;
  typedef script::CallbackFunction<void()> TimerCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;
  Window(int width, int height, cssom::CSSParser* css_parser,
         loader::FetcherFactory* fetcher_factory,
         media::WebMediaPlayerFactory* web_media_player_factory,
         script::ScriptRunner* script_runner, const GURL& url,
         const std::string& user_agent, const ErrorCallback& error_callback);

  // Web API: Window
  //
  scoped_refptr<Window> window() { return this; }
  const scoped_refptr<Document>& document() const;
  scoped_refptr<Location> location() const;
  const scoped_refptr<Navigator>& navigator() const;

  int32_t RequestAnimationFrame(const scoped_refptr<FrameRequestCallback>&) {
    NOTIMPLEMENTED();
    return 0;
  }

  // Web API: CSSOM View Module (partial interface)
  //   http://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-window-interface
  int inner_width() const { return width_; }
  int inner_height() const { return height_; }

  // Web API: GlobalEventHandlers (implements)
  //   http://www.w3.org/TR/html5/webappapis.html#event-handlers
  scoped_refptr<EventListener> onload();
  void set_onload(const scoped_refptr<EventListener>& event_listener);

  // Web API: WindowTimers (implements)
  //   http://www.w3.org/TR/html5/webappapis.html#timers
  //
  int SetTimeout(const scoped_refptr<TimerCallback>& handler) {
    return SetTimeout(handler, 0);
  }

  int SetTimeout(const scoped_refptr<TimerCallback>& handler, int timeout);

  void ClearTimeout(int handle);

  // Custom, not in any spec.
  const scoped_refptr<Console>& console() const;

  DEFINE_WRAPPABLE_TYPE(Window);

 private:
  class RelayOnLoadEvent;

  ~Window() OVERRIDE;

  int width_;
  int height_;

  scoped_ptr<HTMLElementContext> html_element_context_;
  scoped_refptr<Document> document_;
  scoped_ptr<DocumentBuilder> document_builder_;
  scoped_refptr<Navigator> navigator_;
  scoped_ptr<RelayOnLoadEvent> relay_on_load_event_;
  scoped_refptr<Console> console_;
  scoped_ptr<WindowTimers> window_timers_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_WINDOW_H_
