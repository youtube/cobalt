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
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/storage.h"
#include "cobalt/dom/window_timers.h"

namespace cobalt {
namespace dom {

// This class fires the window's load event when the document is loaded.
class Window::RelayLoadEvent : public DocumentObserver {
 public:
  explicit RelayLoadEvent(Window* window) : window_(window) {}

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

  DISALLOW_COPY_AND_ASSIGN(RelayLoadEvent);
};

Window::Window(int width, int height, cssom::CSSParser* css_parser,
               Parser* dom_parser, loader::FetcherFactory* fetcher_factory,
               LocalStorageDatabase* local_storage_database,
               media::WebMediaPlayerFactory* web_media_player_factory,
               script::ScriptRunner* script_runner, const GURL& url,
               const std::string& user_agent,
               const base::Callback<void(const std::string&)>& error_callback)
    : width_(width),
      height_(height),
      html_element_context_(
          new HTMLElementContext(fetcher_factory, css_parser, dom_parser,
                                 web_media_player_factory, script_runner)),
      document_(
          new Document(html_element_context_.get(), Document::Options(url))),
      document_loader_(new loader::Loader(
          base::Bind(&loader::FetcherFactory::CreateFetcher,
                     base::Unretained(fetcher_factory), url),
          dom_parser->ParseDocumentAsync(
              document_, base::SourceLocation(url.spec(), 1, 1)),
          error_callback)),
      navigator_(new Navigator(user_agent)),
      performance_(new Performance()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          relay_on_load_event_(new RelayLoadEvent(this))),
      console_(new Console()),
      window_timers_(new WindowTimers()),
      animation_frame_request_callback_list_(
          new AnimationFrameRequestCallbackList()),
      crypto_(new Crypto()),
      ALLOW_THIS_IN_INITIALIZER_LIST(local_storage_(
          new Storage(this, Storage::kLocalStorage, local_storage_database))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          session_storage_(new Storage(this, Storage::kSessionStorage, NULL))) {
  document_->AddObserver(relay_on_load_event_.get());
}

HTMLElementContext* Window::html_element_context() const {
  return html_element_context_.get();
}

const scoped_refptr<Document>& Window::document() const { return document_; }

scoped_refptr<Location> Window::location() const {
  return document_->location();
}

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

const scoped_refptr<Performance>& Window::performance() const {
  return performance_;
}

scoped_refptr<Crypto> Window::crypto() const { return crypto_; }

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

scoped_refptr<Storage> Window::local_storage() const { return local_storage_; }
scoped_refptr<Storage> Window::session_storage() const {
  return session_storage_;
}

const scoped_refptr<Console>& Window::console() const { return console_; }

Window::~Window() {}

int32 Window::RequestAnimationFrame(
    const scoped_refptr<FrameRequestCallback>& callback) {
  return animation_frame_request_callback_list_->RequestAnimationFrame(
      callback);
}

void Window::CancelAnimationFrame(int32 handle) {
  animation_frame_request_callback_list_->CancelAnimationFrame(handle);
}

void Window::RunAnimationFrameCallbacks() {
  // First grab the current list of frame request callbacks and hold on to it
  // here locally.
  scoped_ptr<AnimationFrameRequestCallbackList> frame_request_list =
      animation_frame_request_callback_list_.Pass();

  // Then setup the Window's frame request callback list with a freshly created
  // and empty one.
  animation_frame_request_callback_list_.reset(
      new AnimationFrameRequestCallbackList());

  // Now, iterate through each of the callbacks and call them.
  frame_request_list->RunCallbacks(performance()->Now());
}

}  // namespace dom
}  // namespace cobalt
