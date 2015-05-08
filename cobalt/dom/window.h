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

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_runner.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

class Document;
class DocumentBuilder;
class HTMLElementFactory;
class Location;
class Navigator;

// The window object represents a window containing a DOM document.
//   http://www.w3.org/TR/html5/browsers.html#the-window-object
//
// TODO(***REMOVED***): Properly handle viewport resolution change event.
class Window : public EventTarget {
 public:
  typedef script::CallbackFunction<void(double)> FrameRequestCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;
  Window(int width, int height, cssom::CSSParser* css_parser,
         loader::FetcherFactory* fetcher_factory,
         media::WebMediaPlayerFactory* web_media_player_factory,
         script::ScriptRunner* script_runner, const GURL& url,
         const std::string& user_agent, const ErrorCallback& error_callback);

  // Web API: Window
  scoped_refptr<Window> window() { return this; }
  const scoped_refptr<Document>& document() const;
  scoped_refptr<Location> location() const;
  const scoped_refptr<Navigator>& navigator() const;

  // Web API: CSSOM View Module (partial interface)
  //   http://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-window-interface
  int inner_width() const { return width_; }
  int inner_height() const { return height_; }

  int32_t RequestAnimationFrame(const scoped_refptr<FrameRequestCallback>&) {
    NOTIMPLEMENTED();
    return 0;
  }

  DEFINE_WRAPPABLE_TYPE(Window);

 private:
  ~Window() OVERRIDE;

  int width_;
  int height_;

  scoped_ptr<HTMLElementFactory> html_element_factory_;
  scoped_refptr<Document> document_;
  scoped_ptr<DocumentBuilder> document_builder_;
  scoped_refptr<Navigator> navigator_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_WINDOW_H_
