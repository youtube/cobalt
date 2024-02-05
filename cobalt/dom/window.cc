// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "cobalt/dom/window.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/user_agent_style_sheet.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/base64.h"
#include "cobalt/dom/camera_3d.h"
#include "cobalt/dom/device_orientation_event.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/history.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/input_event.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/mouse_event.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/screen.h"
#include "cobalt/dom/screenshot.h"
#include "cobalt/dom/screenshot_manager.h"
#include "cobalt/dom/storage.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/speech/speech_synthesis.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/environment_settings_helper.h"
#include "cobalt/web/event.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "starboard/file.h"

using cobalt::cssom::ViewportSize;
using cobalt::media_session::MediaSession;

namespace cobalt {
namespace dom {

// This class fires the window's load event when the document is loaded.
class Window::RelayLoadEvent : public DocumentObserver {
 public:
  explicit RelayLoadEvent(Window* window) : window_(window) {}

  // From DocumentObserver.
  void OnLoad() override {
    window_->PostToDispatchEventName(FROM_HERE, base::Tokens::load());
  }
  void OnMutation() override {}
  void OnFocusChanged() override {}

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(RelayLoadEvent);
};

Window::Window(
    web::EnvironmentSettings* settings, const ViewportSize& view_size,
    base::ApplicationState initial_application_state,
    cssom::CSSParser* css_parser, Parser* dom_parser,
    loader::FetcherFactory* fetcher_factory,
    loader::LoaderFactory* loader_factory,
    render_tree::ResourceProvider** resource_provider,
    loader::image::AnimatedImageTracker* animated_image_tracker,
    loader::image::ImageCache* image_cache,
    loader::image::ReducedCacheCapacityManager*
        reduced_image_cache_capacity_manager,
    loader::font::RemoteTypefaceCache* remote_typeface_cache,
    loader::mesh::MeshCache* mesh_cache,
    LocalStorageDatabase* local_storage_database,
    media::CanPlayTypeHandler* can_play_type_handler,
    media::WebMediaPlayerFactory* web_media_player_factory,
    script::ExecutionState* execution_state,
    script::ScriptRunner* script_runner,
    script::ScriptValueFactory* script_value_factory,
    MediaSource::Registry* media_source_registry,
    DomStatTracker* dom_stat_tracker, const std::string& font_language_script,
    const base::Callback<void(const GURL&)> navigation_callback,
    const loader::Decoder::OnCompleteFunction& load_complete_callback,
    network_bridge::CookieJar* cookie_jar,
    const web::CspDelegate::Options& csp_options,
    const base::Closure& ran_animation_frame_callbacks_callback,
    CloseCallback window_close_callback, base::Closure window_minimize_callback,
    OnScreenKeyboardBridge* on_screen_keyboard_bridge,
    const scoped_refptr<input::Camera3D>& camera_3d,
    const OnStartDispatchEventCallback& on_start_dispatch_event_callback,
    const OnStopDispatchEventCallback& on_stop_dispatch_event_callback,
    const ScreenshotManager::ProvideScreenshotFunctionCallback&
        screenshot_function_callback,
    const NavItemCallback& cancel_scroll_callback,
    base::WaitableEvent* synchronous_loader_interrupt,
    bool enable_inline_script_warnings,
    const scoped_refptr<ui_navigation::NavItem>& ui_nav_root,
    bool enable_map_to_mesh, int csp_insecure_allowed_token,
    int dom_max_element_depth, float video_playback_rate_multiplier,
    ClockType clock_type, const CacheCallback& splash_screen_cache_callback,
    const scoped_refptr<captions::SystemCaptionSettings>& captions,
    bool log_tts)
    // 'window' object EventTargets require special handling for onerror events,
    // see EventTarget constructor for more details.
    : web::WindowOrWorkerGlobalScope(
          settings,
          web::WindowOrWorkerGlobalScope::Options(
              initial_application_state, csp_options, dom_stat_tracker)),
      viewport_size_(view_size),
      is_resize_event_pending_(false),
#if defined(ENABLE_TEST_RUNNER)
      test_runner_(new TestRunner()),
#endif  // ENABLE_TEST_RUNNER
      performance_(new Performance(settings, MakePerformanceClock(clock_type))),
      html_element_context_(new HTMLElementContext(
          settings, fetcher_factory, loader_factory, css_parser, dom_parser,
          can_play_type_handler, web_media_player_factory, script_runner,
          script_value_factory, media_source_registry, resource_provider,
          animated_image_tracker, image_cache,
          reduced_image_cache_capacity_manager, remote_typeface_cache,
          mesh_cache, dom_stat_tracker, font_language_script,
          initial_application_state, synchronous_loader_interrupt,
          performance_.get(), enable_inline_script_warnings,
          video_playback_rate_multiplier)),
      document_loader_(nullptr),
      history_(new History()),
      navigator_(new Navigator(environment_settings(), captions)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          relay_on_load_event_(new RelayLoadEvent(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_frame_request_callback_list_(
          new AnimationFrameRequestCallbackList(this, debugger_hooks()))),
      speech_synthesis_(
          new speech::SpeechSynthesis(settings, navigator_, log_tts)),
      ALLOW_THIS_IN_INITIALIZER_LIST(local_storage_(
          new Storage(this, Storage::kLocalStorage, local_storage_database))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          session_storage_(new Storage(this, Storage::kSessionStorage, NULL))),
      screen_(new Screen(view_size)),
      ran_animation_frame_callbacks_callback_(
          ran_animation_frame_callbacks_callback),
      window_close_callback_(window_close_callback),
      window_minimize_callback_(window_minimize_callback),
      // We only have an on_screen_keyboard_bridge when the platform supports
      // it. Otherwise don't even expose it in the DOM.
      on_screen_keyboard_(on_screen_keyboard_bridge
                              ? new OnScreenKeyboard(settings,
                                                     on_screen_keyboard_bridge,
                                                     script_value_factory)
                              : NULL),
      splash_screen_cache_callback_(splash_screen_cache_callback),
      cancel_scroll_callback_(cancel_scroll_callback),
      on_start_dispatch_event_callback_(on_start_dispatch_event_callback),
      on_stop_dispatch_event_callback_(on_stop_dispatch_event_callback),
      screenshot_manager_(settings, screenshot_function_callback),
      ui_nav_root_(ui_nav_root),
      enable_map_to_mesh_(enable_map_to_mesh) {
  document_ = new Document(
      html_element_context(),
      Document::Options(
          settings->creation_url(),
          base::Bind(&Window::FireHashChangeEvent, base::Unretained(this)),
          performance_->timing()->GetNavigationStartClock(),
          navigation_callback, ParseUserAgentStyleSheet(css_parser), view_size,
          cookie_jar, dom_max_element_depth),
      csp_delegate());

  set_navigator_base(navigator_);
  document_->AddObserver(relay_on_load_event_.get());
  html_element_context()->application_lifecycle_state()->AddObserver(this);
  UpdateCamera3D(camera_3d);

  // Document load start is deferred from this constructor so that we can be
  // guaranteed that this Window object is fully constructed before document
  // loading begins.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Window::StartDocumentLoad, base::Unretained(this),
                            fetcher_factory, settings->creation_url(),
                            dom_parser, load_complete_callback));
}

void Window::StartDocumentLoad(
    loader::FetcherFactory* fetcher_factory, const GURL& url,
    Parser* dom_parser,
    const loader::Decoder::OnCompleteFunction& load_complete_callback) {
  document_loader_.reset(new loader::Loader(
      base::Bind(&loader::FetcherFactory::CreateFetcher,
                 base::Unretained(fetcher_factory), url, /*main_resource=*/true,
                 network::disk_cache::kHTML),
      base::Bind(&Parser::ParseDocumentAsync, base::Unretained(dom_parser),
                 document_, base::SourceLocation(url.spec(), 1, 1)),
      load_complete_callback));
}

scoped_refptr<base::BasicClock> Window::MakePerformanceClock(
    ClockType clock_type) {
  switch (clock_type) {
    case kClockTypeTestRunner: {
#if defined(ENABLE_TEST_RUNNER)
      return test_runner_->GetClock();
#else
      NOTREACHED();
#endif
    } break;
    case kClockTypeSystemTime: {
      return new base::SystemMonotonicClock();
    } break;
    case kClockTypeResolutionLimitedSystemTime: {
      return new base::MinimumResolutionClock(
          new base::SystemMonotonicClock(),
          base::TimeDelta::FromMicroseconds(
              Performance::kPerformanceTimerMinResolutionInMicroseconds));
    } break;
  }
  NOTREACHED();
  return scoped_refptr<base::BasicClock>();
}

const scoped_refptr<Document>& Window::document() const { return document_; }

const scoped_refptr<Location>& Window::location() const {
  return document_->location();
}

const scoped_refptr<History>& Window::history() const { return history_; }

// https://www.w3.org/TR/html50/browsers.html#dom-window-close
void Window::Close() {
  LOG(INFO) << __func__;
  if (!window_close_callback_.is_null()) {
    window_close_callback_.Run(
        performance_->timing()->GetNavigationStartClock()->Now());
  }
}

void Window::Minimize() {
  if (!window_minimize_callback_.is_null()) {
    window_minimize_callback_.Run();
  }
}

const scoped_refptr<Navigator>& Window::navigator() const { return navigator_; }

script::Handle<ScreenshotManager::InterfacePromise> Window::Screenshot(
    script::EnvironmentSettings* environment_settings) {
  scoped_refptr<render_tree::Node> render_tree_root =
      document_->DoSynchronousLayoutAndGetRenderTree();

  script::Handle<ScreenshotManager::InterfacePromise> promise =
      html_element_context()
          ->script_value_factory()
          ->CreateInterfacePromise<dom::Screenshot>();

  auto* global_wrappable = web::get_global_wrappable(environment_settings);
  std::unique_ptr<ScreenshotManager::InterfacePromiseValue::Reference>
      promise_reference(new ScreenshotManager::InterfacePromiseValue::Reference(
          global_wrappable, promise));

  screenshot_manager_.Screenshot(
      loader::image::EncodedStaticImage::ImageFormat::kPNG, render_tree_root,
      std::move(promise_reference));

  return promise;
}

scoped_refptr<cssom::CSSStyleDeclaration> Window::GetComputedStyle(
    const scoped_refptr<Element>& elt) {
  scoped_refptr<HTMLElement> html_element = elt->AsHTMLElement();
  if (html_element) {
    document_->UpdateComputedStyleOnElementAndAncestor(html_element.get());
    return html_element->css_computed_style_declaration();
  }
  return NULL;
}

scoped_refptr<cssom::CSSStyleDeclaration> Window::GetComputedStyle(
    const scoped_refptr<Element>& elt, const std::string& pseudo_elt) {
  // The getComputedStyle(elt, pseudoElt) method must run these steps:
  // https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-window-getcomputedstyle

  // 1. Let doc be the Document associated with the Window object on which the
  // method was invoked.
  DCHECK_EQ(document_, elt->node_document())
      << "getComputedStyle not supported for elements outside of the document";

  scoped_refptr<HTMLElement> html_element = elt->AsHTMLElement();
  scoped_refptr<cssom::CSSComputedStyleDeclaration> obj;
  if (html_element) {
    document_->UpdateComputedStyleOnElementAndAncestor(html_element.get());

    // 2. Let obj be elt.
    obj = html_element->css_computed_style_declaration();

    // 3. If pseudoElt is as an ASCII case-insensitive match for either
    // ':before' or '::before' let obj be the ::before pseudo-element of elt.
    if (base::EqualsCaseInsensitiveASCII(pseudo_elt, ":before") ||
        base::EqualsCaseInsensitiveASCII(pseudo_elt, "::before")) {
      PseudoElement* pseudo_element =
          html_element->pseudo_element(kBeforePseudoElementType);
      obj = pseudo_element ? pseudo_element->css_computed_style_declaration()
                           : NULL;
    }

    // 4. If pseudoElt is as an ASCII case-insensitive match for either ':after'
    // or '::after' let obj be the ::after pseudo-element of elt.
    if (base::EqualsCaseInsensitiveASCII(pseudo_elt, ":after") ||
        base::EqualsCaseInsensitiveASCII(pseudo_elt, "::after")) {
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
  DCHECK(html_element_context()->css_parser());
  scoped_refptr<cssom::MediaList> media_list =
      html_element_context()->css_parser()->ParseMediaList(
          query, GetInlineSourceLocation());
  return base::WrapRefCounted(new MediaQueryList(media_list, screen_));
}

const scoped_refptr<Screen>& Window::screen() { return screen_; }

std::string Window::Btoa(const std::string& string_to_encode,
                         script::ExceptionState* exception_state) {
  TRACE_EVENT0("cobalt::dom", "Window::Btoa()");
  LOG(WARNING) << "In older Cobalt(<19), btoa() can not take a string"
                  " containing NULL. Be careful that you don't need to stay "
                  "compatible with old versions of Cobalt if you use btoa.";
  auto output = ForgivingBase64Encode(string_to_encode);
  if (!output) {
    web::DOMException::Raise(web::DOMException::kInvalidCharacterErr,
                             exception_state);
    return std::string();
  }
  return *output;
}

std::vector<uint8_t> Window::Atob(const std::string& encoded_string,
                                  script::ExceptionState* exception_state) {
  TRACE_EVENT0("cobalt::dom", "Window::Atob()");
  auto output = ForgivingBase64Decode(encoded_string);
  if (!output) {
    web::DOMException::Raise(web::DOMException::kInvalidCharacterErr,
                             exception_state);
    return {};
  }
  return *output;
}

scoped_refptr<Storage> Window::local_storage() const { return local_storage_; }

scoped_refptr<Storage> Window::session_storage() const {
  return session_storage_;
}

const scoped_refptr<Performance>& Window::performance() const {
  return performance_;
}

scoped_refptr<speech::SpeechSynthesis> Window::speech_synthesis() const {
  return speech_synthesis_;
}

const scoped_refptr<Camera3D>& Window::camera_3d() const { return camera_3d_; }

#if defined(ENABLE_TEST_RUNNER)
const scoped_refptr<TestRunner>& Window::test_runner() const {
  return test_runner_;
}
#endif  // ENABLE_TEST_RUNNER

void Window::Gc(script::EnvironmentSettings* settings) {
  if (settings) {
    base::polymorphic_downcast<web::EnvironmentSettings*>(settings)
        ->context()
        ->javascript_engine()
        ->CollectGarbage();
  }
}

void Window::RunAnimationFrameCallbacks() {
  // Scope the StopWatch. It should not include any processing from
  // |ran_animation_frame_callbacks_callback_|.
  {
    base::StopWatch stop_watch_run_animation_frame_callbacks(
        DomStatTracker::kStopWatchTypeRunAnimationFrameCallbacks,
        base::StopWatch::kAutoStartOn,
        html_element_context()->dom_stat_tracker());

    // First grab the current list of frame request callbacks and hold on to it
    // here locally.
    std::unique_ptr<AnimationFrameRequestCallbackList> frame_request_list =
        std::move(animation_frame_request_callback_list_);

    // Then setup the Window's frame request callback list with a freshly
    // created and empty one.
    animation_frame_request_callback_list_.reset(
        new AnimationFrameRequestCallbackList(this, debugger_hooks()));

    // Now, iterate through each of the callbacks and call them.
    frame_request_list->RunCallbacks(*document_->timeline()->current_time());
  }

  // Run the callback if one exists.
  if (!ran_animation_frame_callbacks_callback_.is_null()) {
    ran_animation_frame_callbacks_callback_.Run();
  }
}

bool Window::HasPendingAnimationFrameCallbacks() const {
  return animation_frame_request_callback_list_->HasPendingCallbacks();
}

void Window::InjectEvent(const scoped_refptr<web::Event>& event) {
  TRACE_EVENT1("cobalt::dom", "Window::InjectEvent()", "event",
               TRACE_STR_COPY(event->type().c_str()));

  // Forward the event on to the correct object in DOM.
  if (event->GetWrappableType() == base::GetTypeId<KeyboardEvent>()) {
    // Event.target:focused element processing the key event or if no element
    // focused, then the body element if available, otherwise the root element.
    //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-keydown
    //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-keypress
    //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-keyup
    if (document_->active_element()) {
      document_->active_element()->DispatchEvent(event);
    } else {
      document_->DispatchEvent(event);
    }
  } else if (event->GetWrappableType() == base::GetTypeId<InputEvent>()) {
    // Dispatch any InputEvent directly to the OnScreenKeyboard element.
    if (on_screen_keyboard_) {
      on_screen_keyboard_->DispatchEvent(event);
    }
  } else {
    SB_NOTREACHED();
  }
}

void Window::SetApplicationState(base::ApplicationState state,
                                 SbTimeMonotonic timestamp) {
  html_element_context()->application_lifecycle_state()->SetApplicationState(
      state);
  if (timestamp == 0) return;
  performance_->SetApplicationState(state, timestamp);
  window_timers_.SetApplicationState(state);
}

void Window::SetSynchronousLayoutCallback(const base::Closure& callback) {
  document_->set_synchronous_layout_callback(callback);
}

void Window::SetSynchronousLayoutAndProduceRenderTreeCallback(
    const SynchronousLayoutAndProduceRenderTreeCallback& callback) {
  document_->set_synchronous_layout_and_produce_render_tree_callback(callback);
}

void Window::SetSize(ViewportSize size) {
  if (size == viewport_size_) {
    return;
  }

  viewport_size_ = size;
  screen_->SetSize(viewport_size_);
  // This will cause layout invalidation.
  document_->SetViewport(viewport_size_);

  if (html_element_context()
          ->application_lifecycle_state()
          ->GetVisibilityState() == kVisibilityStateVisible) {
    DispatchEvent(new web::Event(base::Tokens::resize()));
  } else {
    is_resize_event_pending_ = true;
  }
}

void Window::UpdateCamera3D(const scoped_refptr<input::Camera3D>& camera_3d) {
  if (camera_3d_ && camera_3d_->impl()) {
    // Update input object for existing camera.
    camera_3d_->impl()->SetInput(camera_3d);
  } else {
    // Create a new camera which uses the given input camera object.
    camera_3d_ = new Camera3D(camera_3d);
    camera_3d_->StartOrientationEvents(base::AsWeakPtr(this));
  }
}

void Window::OnWindowFocusChanged(bool has_focus) {
  DispatchEvent(
      new web::Event(has_focus ? base::Tokens::focus() : base::Tokens::blur()));
}

void Window::OnVisibilityStateChanged(VisibilityState visibility_state) {
  if (is_resize_event_pending_ && visibility_state == kVisibilityStateVisible) {
    is_resize_event_pending_ = false;
    DispatchEvent(new web::Event(base::Tokens::resize()));
  }
}

void Window::OnFrozennessChanged(bool is_frozen) {
  // Ignored by this class.
}

void Window::OnDocumentRootElementUnableToProvideOffsetDimensions() {
  DLOG(WARNING) << "Document root element unable to provide offset dimensions!";
  // If the root element was unable to provide its dimensions as a result of
  // the app being in a visibility state that disables layout, then prepare a
  // pending resize event, so that the resize will occur once layouts are again
  // available.
  if (html_element_context()
          ->application_lifecycle_state()
          ->GetVisibilityState() != kVisibilityStateVisible) {
    is_resize_event_pending_ = true;
  }
}

void Window::OnWindowOnOnlineEvent() {
  DispatchEvent(new web::Event(base::Tokens::online()));
}

void Window::OnWindowOnOfflineEvent() {
  DispatchEvent(new web::Event(base::Tokens::offline()));
}

void Window::OnStartDispatchEvent(const scoped_refptr<web::Event>& event) {
  if (!on_start_dispatch_event_callback_.is_null()) {
    on_start_dispatch_event_callback_.Run(event);
  }
}

void Window::OnStopDispatchEvent(const scoped_refptr<web::Event>& event) {
  if (!on_stop_dispatch_event_callback_.is_null()) {
    on_stop_dispatch_event_callback_.Run(event);
  }
}

void Window::ClearPointerStateForShutdown() {
  document_->pointer_state()->ClearForShutdown();
}

void Window::TraceMembers(script::Tracer* tracer) {
  web::EventTarget::TraceMembers(tracer);

#if defined(ENABLE_TEST_RUNNER)
  tracer->Trace(test_runner_);
#endif  // ENABLE_TEST_RUNNER
  tracer->Trace(performance_);
  tracer->Trace(document_);
  tracer->Trace(history_);
  tracer->Trace(navigator_);
  tracer->Trace(camera_3d_);
  tracer->Trace(crypto_);
  tracer->Trace(speech_synthesis_);
  tracer->Trace(local_storage_);
  tracer->Trace(session_storage_);
  tracer->Trace(screen_);
  tracer->Trace(on_screen_keyboard_);
}

const scoped_refptr<media_session::MediaSession> Window::media_session() const {
  return navigator_->media_session();
}

void Window::CacheSplashScreen(const std::string& content,
                               const base::Optional<std::string>& topic) {
  if (splash_screen_cache_callback_.is_null()) {
    return;
  }
  DLOG(INFO) << "Caching splash screen for URL " << location()->url();
  splash_screen_cache_callback_.Run(content, topic);
}

void Window::CancelScroll(
    const scoped_refptr<ui_navigation::NavItem>& nav_item) {
  if (cancel_scroll_callback_.is_null()) {
    return;
  }
  cancel_scroll_callback_.Run(nav_item);
}

const scoped_refptr<OnScreenKeyboard>& Window::on_screen_keyboard() const {
  return on_screen_keyboard_;
}

void Window::ReleaseOnScreenKeyboard() { on_screen_keyboard_ = nullptr; }

Window::~Window() {
  if (ui_nav_root_) {
    ui_nav_root_->SetEnabled(false);
  }
  html_element_context()->application_lifecycle_state()->RemoveObserver(this);
}

void Window::FireHashChangeEvent() {
  PostToDispatchEventName(FROM_HERE, base::Tokens::hashchange());
}

}  // namespace dom
}  // namespace cobalt
