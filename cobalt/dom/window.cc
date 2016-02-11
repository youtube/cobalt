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

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/user_agent_style_sheet.h"
#include "cobalt/dom/console.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/history.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom/storage.h"
#include "cobalt/dom/window_timers.h"
#include "cobalt/script/javascript_engine.h"

namespace cobalt {
namespace dom {

// This class fires the window's load event when the document is loaded.
class Window::RelayLoadEvent : public DocumentObserver {
 public:
  explicit RelayLoadEvent(Window* window) : window_(window) {}

  // From DocumentObserver.
  void OnLoad() OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(
                static_cast<bool (Window::*)(const scoped_refptr<Event>&)>(
                    &Window::DispatchEvent)),
            base::AsWeakPtr<Window>(window_),
            make_scoped_refptr(new Event(base::Tokens::load()))));
  }
  void OnMutation() OVERRIDE {}

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(RelayLoadEvent);
};

Window::Window(int width, int height, cssom::CSSParser* css_parser,
               Parser* dom_parser, loader::FetcherFactory* fetcher_factory,
               render_tree::ResourceProvider* resource_provider,
               loader::image::ImageCache* image_cache,
               loader::font::RemoteFontCache* remote_font_cache,
               LocalStorageDatabase* local_storage_database,
               media::WebMediaPlayerFactory* web_media_player_factory,
               script::ExecutionState* execution_state,
               script::ScriptRunner* script_runner,
               MediaSource::Registry* media_source_registry, const GURL& url,
               const std::string& user_agent, const std::string& language,
               const base::Callback<void(const GURL&)> navigation_callback,
               const base::Callback<void(const std::string&)>& error_callback,
               network_bridge::CookieJar* cookie_jar,
               const network_bridge::PostSender& post_sender,
               const std::string& default_security_policy,
               CSPDelegate::EnforcementType csp_enforcement_mode)
    : width_(width),
      height_(height),
      html_element_context_(new HTMLElementContext(
          fetcher_factory, css_parser, dom_parser, web_media_player_factory,
          script_runner, media_source_registry, resource_provider, image_cache,
          remote_font_cache, language)),
      performance_(new Performance(new base::SystemMonotonicClock())),
      document_(new Document(
          html_element_context_.get(),
          Document::Options(
              url, performance_->timing()->GetNavigationStartClock(),
              navigation_callback, ParseUserAgentStyleSheet(css_parser),
              math::Size(width_, height_), cookie_jar, post_sender,
              default_security_policy, csp_enforcement_mode))),
      document_loader_(new loader::Loader(
          base::Bind(&loader::FetcherFactory::CreateFetcher,
                     base::Unretained(fetcher_factory), url),
          dom_parser->ParseDocumentAsync(
              document_, base::SourceLocation(url.spec(), 1, 1)),
          error_callback)),
      history_(new History()),
      navigator_(new Navigator(user_agent, language)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          relay_on_load_event_(new RelayLoadEvent(this))),
      console_(new Console(execution_state)),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_timers_(new WindowTimers(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_frame_request_callback_list_(
          new AnimationFrameRequestCallbackList(this))),
      crypto_(new Crypto()),
      ALLOW_THIS_IN_INITIALIZER_LIST(local_storage_(
          new Storage(this, Storage::kLocalStorage, local_storage_database))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          session_storage_(new Storage(this, Storage::kSessionStorage, NULL))),
      screen_(new Screen(width, height)) {
#if defined(ENABLE_TEST_RUNNER)
  test_runner_ = new TestRunner();
#endif  // ENABLE_TEST_RUNNER
  document_->AddObserver(relay_on_load_event_.get());
}

const scoped_refptr<Document>& Window::document() const { return document_; }

const scoped_refptr<History>& Window::history() const { return history_; }

scoped_refptr<Location> Window::location() const {
  return document_->location();
}

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

int32 Window::RequestAnimationFrame(
    const AnimationFrameRequestCallbackList::FrameRequestCallbackArg&
        callback) {
  return animation_frame_request_callback_list_->RequestAnimationFrame(
      callback);
}

void Window::CancelAnimationFrame(int32 handle) {
  animation_frame_request_callback_list_->CancelAnimationFrame(handle);
}

scoped_refptr<MediaQueryList> Window::MatchMedia(const std::string& query) {
  DCHECK(html_element_context_);
  DCHECK(html_element_context_->css_parser());
  scoped_refptr<cssom::MediaList> media_list =
      html_element_context_->css_parser()->ParseMediaList(
          query, GetInlineSourceLocation());
  return make_scoped_refptr(new MediaQueryList(media_list, screen_));
}

const scoped_refptr<Screen>& Window::screen() { return screen_; }

scoped_refptr<Crypto> Window::crypto() const { return crypto_; }

int Window::SetTimeout(const WindowTimers::TimerCallbackArg& handler,
                       int timeout) {
  DLOG_IF(WARNING, timeout < 0)
      << "Window::SetTimeout received negative timeout: " << timeout;
  timeout = std::max(timeout, 0);
  return window_timers_->SetTimeout(handler, timeout);
}

void Window::ClearTimeout(int handle) { window_timers_->ClearTimeout(handle); }

int Window::SetInterval(const WindowTimers::TimerCallbackArg& handler,
                        int timeout) {
  DLOG_IF(WARNING, timeout < 0)
      << "Window::SetInterval received negative timeout: " << timeout;
  timeout = std::max(timeout, 0);
  return window_timers_->SetInterval(handler, timeout);
}

void Window::ClearInterval(int handle) {
  window_timers_->ClearInterval(handle);
}

scoped_refptr<Storage> Window::local_storage() const { return local_storage_; }

scoped_refptr<Storage> Window::session_storage() const {
  return session_storage_;
}

const scoped_refptr<Performance>& Window::performance() const {
  return performance_;
}

const scoped_refptr<Console>& Window::console() const { return console_; }

#if defined(ENABLE_TEST_RUNNER)
const scoped_refptr<TestRunner>& Window::test_runner() const {
  return test_runner_;
}
#endif  // ENABLE_TEST_RUNNER

void Window::Gc(script::EnvironmentSettings* settings) {
  if (settings) {
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    dom_settings->javascript_engine()->CollectGarbage();
  }
}

HTMLElementContext* Window::html_element_context() const {
  return html_element_context_.get();
}

void Window::RunAnimationFrameCallbacks() {
  // First grab the current list of frame request callbacks and hold on to it
  // here locally.
  scoped_ptr<AnimationFrameRequestCallbackList> frame_request_list =
      animation_frame_request_callback_list_.Pass();

  // Then setup the Window's frame request callback list with a freshly created
  // and empty one.
  animation_frame_request_callback_list_.reset(
      new AnimationFrameRequestCallbackList(this));

  // Now, iterate through each of the callbacks and call them.
  frame_request_list->RunCallbacks(*document_->timeline()->current_time());
}

void Window::InjectEvent(const scoped_refptr<Event>& event) {
  // Forward the event on to the correct object in DOM.
  if (event->type() == base::Tokens::keydown() ||
      event->type() == base::Tokens::keypress() ||
      event->type() == base::Tokens::keyup()) {
    // Event.target:focused element processing the key event or if no element
    // focused, then the body element if available, otherwise the root element.
    //   https://www.w3.org/TR/DOM-Level-3-Events/#event-type-keydown
    //   https://www.w3.org/TR/DOM-Level-3-Events/#event-type-keypress
    //   https://www.w3.org/TR/DOM-Level-3-Events/#event-type-keyup
    if (document_->active_element()) {
      document_->active_element()->DispatchEvent(event);
    } else {
      document_->DispatchEvent(event);
    }
  } else if (event->type() == base::Tokens::hashchange()) {
    // Hashchange event should always trigger asychronous event.
    // See comment in BrowserModule::NavigateWithCallbackInternal.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(
                static_cast<bool (Window::*)(const scoped_refptr<Event>&)>(
                    &Window::DispatchEvent)),
            base::AsWeakPtr<Window>(this), event));
  } else {
    NOTREACHED();
  }
}

void Window::SetSynchronousLayoutCallback(
    const base::Closure& synchronous_layout_callback) {
  document_->set_synchronous_layout_callback(synchronous_layout_callback);
}

Window::~Window() {}

}  // namespace dom
}  // namespace cobalt
