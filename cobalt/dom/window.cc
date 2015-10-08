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

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "cobalt/dom/console.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_names.h"
#include "cobalt/dom/history.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/screen.h"
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
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(
                static_cast<bool (Window::*)(const scoped_refptr<Event>&)>(
                    &Window::DispatchEvent)),
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
               loader::image::ImageCache* image_cache,
               loader::font::RemoteFontCache* remote_font_cache,
               LocalStorageDatabase* local_storage_database,
               media::WebMediaPlayerFactory* web_media_player_factory,
               script::ExecutionState* execution_state,
               script::ScriptRunner* script_runner, const GURL& url,
               const std::string& user_agent, const std::string& language,
               const base::Callback<void(const std::string&)>& error_callback)
    : width_(width),
      height_(height),
      html_element_context_(new HTMLElementContext(
          fetcher_factory, css_parser, dom_parser, web_media_player_factory,
          script_runner, image_cache, remote_font_cache)),
      performance_(new Performance(new base::SystemMonotonicClock())),
      document_(new Document(
          html_element_context_.get(),
          Document::Options(
              url, performance_->timing()->GetNavigationStartClock()))),
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

const scoped_refptr<Screen>& Window::screen() { return screen_; }

scoped_refptr<Crypto> Window::crypto() const { return crypto_; }

int Window::SetTimeout(const WindowTimers::TimerCallbackArg& handler,
                       int timeout) {
  return window_timers_->SetTimeout(handler, timeout);
}

void Window::ClearTimeout(int handle) { window_timers_->ClearTimeout(handle); }

int Window::SetInterval(const WindowTimers::TimerCallbackArg& handler,
                        int timeout) {
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

const scoped_refptr<debug::DebugHub>& Window::debug_hub() const {
  return debug_hub_;
}

void Window::set_debug_hub(const scoped_refptr<debug::DebugHub>& debug_hub) {
  debug_hub_ = debug_hub;
}

#if defined(ENABLE_TEST_RUNNER)
const scoped_refptr<TestRunner>& Window::test_runner() const {
  return test_runner_;
}
#endif  // ENABLE_TEST_RUNNER

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
  frame_request_list->RunCallbacks(
      document_->timeline_sample_time()->InMillisecondsF());
}

void Window::InjectEvent(const scoped_refptr<Event>& event) {
  // Forward the event on to the correct object in DOM.
  if (event->type() == EventNames::GetInstance()->keydown() ||
      event->type() == EventNames::GetInstance()->keypress() ||
      event->type() == EventNames::GetInstance()->keyup()) {
    // Event.target:focused element processing the key event or if no element
    // focused, then the body element if available, otherwise the root element.
    //   http://www.w3.org/TR/DOM-Level-3-Events/#event-type-keydown
    //   http://www.w3.org/TR/DOM-Level-3-Events/#event-type-keypress
    //   http://www.w3.org/TR/DOM-Level-3-Events/#event-type-keyup
    if (document_->active_element()) {
      document_->active_element()->DispatchEvent(event);
    } else {
      document_->DispatchEvent(event);
    }
  } else {
    NOTREACHED();
  }
}

Window::~Window() {}

}  // namespace dom
}  // namespace cobalt
