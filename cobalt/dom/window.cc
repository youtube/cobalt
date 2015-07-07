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

#include "cobalt/dom/window.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "cobalt/dom/console.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_builder.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/window_timers.h"

namespace cobalt {
namespace dom {

// This class fires the window's OnLoad event when the document is loaded.
class Window::RelayOnLoadEvent : public DocumentObserver {
 public:
  explicit RelayOnLoadEvent(Window* window) : window_(window) {}

  // From DocumentObserver.
  void OnLoad() OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(base::IgnoreResult(&Window::DispatchEvent),
                              base::AsWeakPtr<Window>(window_),
                              make_scoped_refptr(new Event("load"))));
  }
  void OnMutation() OVERRIDE {}

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(RelayOnLoadEvent);
};

Window::Window(int width, int height, cssom::CSSParser* css_parser,
               loader::FetcherFactory* fetcher_factory,
               media::WebMediaPlayerFactory* web_media_player_factory,
               script::ScriptRunner* script_runner, const GURL& url,
               const std::string& user_agent,
               const ErrorCallback& error_callback)
    : width_(width),
      height_(height),
      html_element_context_(new HTMLElementContext(fetcher_factory, css_parser,
                                                   web_media_player_factory,
                                                   script_runner)),
      document_(
          new Document(html_element_context_.get(), Document::Options(url))),
      document_builder_(new DocumentBuilder(document_, url, fetcher_factory,
                                            html_element_context_.get(),
                                            base::Closure(), error_callback)),
      navigator_(new Navigator(user_agent)),
      performance_(new Performance()),
      relay_on_load_event_(new RelayOnLoadEvent(this)),
      console_(new Console()),
      window_timers_(new WindowTimers()) {
  document_->AddObserver(relay_on_load_event_.get());
}

const scoped_refptr<Document>& Window::document() const { return document_; }

scoped_refptr<Location> Window::location() const {
  return document_->location();
}

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

const scoped_refptr<Performance>& Window::performance() const {
  return performance_;
}

scoped_refptr<EventListener> Window::onload() {
  return GetAttributeEventListener("load");
}

void Window::set_onload(const scoped_refptr<EventListener>& listener) {
  SetAttributeEventListener("load", listener);
}

int Window::SetTimeout(const scoped_refptr<TimerCallback>& handler,
                       int timeout) {
  return window_timers_->SetTimeout(handler, timeout);
}

void Window::ClearTimeout(int handle) { window_timers_->ClearTimeout(handle); }

const scoped_refptr<Console>& Window::console() const { return console_; }

Window::~Window() {}

}  // namespace dom
}  // namespace cobalt
