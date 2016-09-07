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
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/user_agent_style_sheet.h"
#include "cobalt/dom/console.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/history.h"
#include "cobalt/dom/html_element.h"
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
    window_->PostToDispatchEvent(FROM_HERE, base::Tokens::load());
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
               loader::font::RemoteTypefaceCache* remote_typeface_cache,
               LocalStorageDatabase* local_storage_database,
               media::CanPlayTypeHandler* can_play_type_handler,
               media::WebMediaPlayerFactory* web_media_player_factory,
               script::ExecutionState* execution_state,
               script::ScriptRunner* script_runner,
               MediaSource::Registry* media_source_registry,
               DomStatTracker* dom_stat_tracker, const GURL& url,
               const std::string& user_agent, const std::string& language,
               const base::Callback<void(const GURL&)> navigation_callback,
               const base::Callback<void(const std::string&)>& error_callback,
               network_bridge::CookieJar* cookie_jar,
               const network_bridge::PostSender& post_sender,
               const std::string& default_security_policy,
               CspEnforcementType csp_enforcement_mode,
               const base::Closure& csp_policy_changed_callback,
               int csp_insecure_allowed_token)
    : width_(width),
      height_(height),
      html_element_context_(new HTMLElementContext(
          fetcher_factory, css_parser, dom_parser, can_play_type_handler,
          web_media_player_factory, script_runner, media_source_registry,
          resource_provider, image_cache, remote_typeface_cache,
          dom_stat_tracker, language)),
      performance_(new Performance(new base::SystemMonotonicClock())),
      ALLOW_THIS_IN_INITIALIZER_LIST(document_(new Document(
          html_element_context_.get(),
          Document::Options(
              url, this,
              base::Bind(&Window::FireHashChangeEvent, base::Unretained(this)),
              performance_->timing()->GetNavigationStartClock(),
              navigation_callback, ParseUserAgentStyleSheet(css_parser),
              math::Size(width_, height_), cookie_jar, post_sender,
              default_security_policy, csp_enforcement_mode,
              csp_policy_changed_callback, csp_insecure_allowed_token)))),
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

const scoped_refptr<Location>& Window::location() const {
  return document_->location();
}

const scoped_refptr<History>& Window::history() const { return history_; }

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

scoped_refptr<cssom::CSSStyleDeclaration> Window::GetComputedStyle(
    const scoped_refptr<Element>& elt) {
  scoped_refptr<HTMLElement> html_element = elt->AsHTMLElement();
  if (html_element) {
    document_->UpdateComputedStyles();
    return html_element->css_computed_style_declaration();
  }
  return NULL;
}

scoped_refptr<cssom::CSSStyleDeclaration> Window::GetComputedStyle(
    const scoped_refptr<Element>& elt, const std::string& pseudoElt) {
  // The getComputedStyle(elt, pseudoElt) method must run these steps:
  // https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-window-getcomputedstyle

  // 1. Let doc be the Document associated with the Window object on which the
  // method was invoked.
  DCHECK_EQ(document_, elt->node_document())
      << "getComputedStyle not supported for elements outside of the document";

  scoped_refptr<HTMLElement> html_element = elt->AsHTMLElement();
  scoped_refptr<cssom::CSSComputedStyleDeclaration> obj;
  if (html_element) {
    document_->UpdateComputedStyles();

    // 2. Let obj be elt.
    obj = html_element->css_computed_style_declaration();

    // 3. If pseudoElt is as an ASCII case-insensitive match for either
    // ':before' or '::before' let obj be the ::before pseudo-element of elt.
    if (LowerCaseEqualsASCII(pseudoElt, ":before") ||
        LowerCaseEqualsASCII(pseudoElt, "::before")) {
      PseudoElement* pseudo_element =
          html_element->pseudo_element(kBeforePseudoElementType);
      obj = pseudo_element ? pseudo_element->css_computed_style_declaration()
                           : NULL;
    }

    // 4. If pseudoElt is as an ASCII case-insensitive match for either ':after'
    // or '::after' let obj be the ::after pseudo-element of elt.
    if (LowerCaseEqualsASCII(pseudoElt, ":after") ||
        LowerCaseEqualsASCII(pseudoElt, "::after")) {
      PseudoElement* pseudo_element =
          html_element->pseudo_element(kAfterPseudoElementType);
      obj = pseudo_element ? pseudo_element->css_computed_style_declaration()
                           : NULL;
    }
  }
  // 5. Return a live CSS declaration block.
  return obj;
}

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
    base::StopWatch stop_watch_inject_event(
        DomStatTracker::kStopWatchTypeInjectEvent,
        base::StopWatch::kAutoStartOn,
        html_element_context_->dom_stat_tracker());

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
  } else {
    NOTREACHED();
  }
}

void Window::SetSynchronousLayoutCallback(
    const base::Closure& synchronous_layout_callback) {
  document_->set_synchronous_layout_callback(synchronous_layout_callback);
}

Window::~Window() {}

void Window::FireHashChangeEvent() {
  PostToDispatchEvent(FROM_HERE, base::Tokens::hashchange());
}

}  // namespace dom
}  // namespace cobalt
