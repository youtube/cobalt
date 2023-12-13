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

#ifndef COBALT_DOM_WINDOW_H_
#define COBALT_DOM_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/timer.h"
#include "cobalt/base/application_state.h"
#include "cobalt/base/clock.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/animation_frame_request_callback_list.h"
#include "cobalt/dom/application_lifecycle_state.h"
#include "cobalt/dom/captions/system_caption_settings.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/media_query_list.h"
#include "cobalt/dom/on_screen_keyboard.h"
#include "cobalt/dom/on_screen_keyboard_bridge.h"
#include "cobalt/dom/parser.h"
#include "cobalt/dom/screenshot_manager.h"
#if defined(ENABLE_TEST_RUNNER)
#include "cobalt/dom/test_runner.h"
#endif  // ENABLE_TEST_RUNNER
#include "cobalt/input/camera_3d.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/font/remote_typeface_cache.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/loader_factory.h"
#include "cobalt/loader/mesh/mesh_cache.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/network_bridge/cookie_jar.h"
#include "cobalt/network_bridge/net_poster.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/error_report.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/ui_navigation/nav_item.h"
#include "cobalt/web/csp_delegate_type.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/url_registry.h"
#include "cobalt/web/user_agent_platform_info.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "starboard/window.h"
#include "url/gurl.h"


namespace cobalt {
namespace media_session {
class MediaSession;
}  // namespace media_session
namespace speech {
class SpeechSynthesis;
}  // namespace speech
namespace web {
class Event;
class WindowTimers;
}  // namespace web
namespace dom {

class Camera3D;
class Document;
class Element;
class History;
class LocalStorageDatabase;
class Location;
class MediaSource;
class Navigator;
class OnScreenKeyboard;
class Performance;
class Screen;
class Storage;

// The window object represents a window containing a DOM document.
//   https://www.w3.org/TR/html50/browsers.html#the-window-object
//
// TODO: Properly handle viewport resolution change event.
class Window : public web::WindowOrWorkerGlobalScope,
               public ApplicationLifecycleState::Observer {
 public:
  typedef AnimationFrameRequestCallbackList::FrameRequestCallback
      FrameRequestCallback;
  typedef web::WindowTimers::TimerCallback TimerCallback;
  typedef base::Callback<void(const scoped_refptr<web::Event>& event)>
      OnStartDispatchEventCallback;
  typedef base::Callback<void(const scoped_refptr<web::Event>& event)>
      OnStopDispatchEventCallback;

  // Callback that will be called when window.close() is called.  The
  // base::TimeDelta parameter will contain the document's timeline time when
  // close() was called.
  typedef base::Callback<void(base::TimeDelta)> CloseCallback;
  typedef web::UrlRegistry<MediaSource> MediaSourceRegistry;
  typedef base::Callback<void(const std::string&,
                              const base::Optional<std::string>&)>
      CacheCallback;
  typedef base::Callback<void(
      const scoped_refptr<ui_navigation::NavItem>& nav_item)>
      NavItemCallback;

  enum ClockType {
    kClockTypeTestRunner,
    kClockTypeSystemTime,
    kClockTypeResolutionLimitedSystemTime
  };

  Window(
      web::EnvironmentSettings* settings, const cssom::ViewportSize& view_size,
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
      MediaSourceRegistry* media_source_registry,
      DomStatTracker* dom_stat_tracker, const std::string& font_language_script,
      const base::Callback<void(const GURL&)> navigation_callback,
      const loader::Decoder::OnCompleteFunction& load_complete_callback,
      network_bridge::CookieJar* cookie_jar,
      const web::CspDelegate::Options& csp_options,
      const base::Closure& ran_animation_frame_callbacks_callback,
      CloseCallback window_close_callback,
      base::Closure window_minimize_callback,
      OnScreenKeyboardBridge* on_screen_keyboard_bridge,
      const scoped_refptr<input::Camera3D>& camera_3d,
      const OnStartDispatchEventCallback&
          start_tracking_dispatch_event_callback,
      const OnStopDispatchEventCallback& stop_tracking_dispatch_event_callback,
      const ScreenshotManager::ProvideScreenshotFunctionCallback&
          screenshot_function_callback,
      const NavItemCallback& cancel_scroll_callback,
      base::WaitableEvent* synchronous_loader_interrupt,
      bool enable_inline_script_warnings = false,
      const scoped_refptr<ui_navigation::NavItem>& ui_nav_root = nullptr,
      bool enable_map_to_mesh = true, int csp_insecure_allowed_token = 0,
      int dom_max_element_depth = 0, float video_playback_rate_multiplier = 1.f,
      ClockType clock_type = kClockTypeSystemTime,
      const CacheCallback& splash_screen_cache_callback = CacheCallback(),
      const scoped_refptr<captions::SystemCaptionSettings>& captions = nullptr,
      bool log_tts = false);

  // From web::WindowOrWorkerGlobalScope
  //
  Window* AsWindow() override { return this; }

  // Web API: Window
  //
  scoped_refptr<Window> window() { return this; }
  scoped_refptr<Window> self() { return this; }
  const scoped_refptr<Document>& document() const;
  const scoped_refptr<Location>& location() const;
  const scoped_refptr<History>& history() const;
  void Close();
  void Minimize();

  scoped_refptr<Window> frames() { return this; }
  unsigned int length() { return 0; }
  scoped_refptr<Window> top() { return this; }
  scoped_refptr<Window> opener() { return this; }
  scoped_refptr<Window> parent() { return this; }
  scoped_refptr<Window> AnonymousIndexedGetter(unsigned int index) {
    return NULL;
  }

  const scoped_refptr<Navigator>& navigator() const;

  script::Handle<ScreenshotManager::InterfacePromise> Screenshot(
      script::EnvironmentSettings* environment_settings);

  // Web API: CSSOM (partial interface)
  // Returns the computed style of the given element, as described in
  // https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-window-getcomputedstyle.
  scoped_refptr<cssom::CSSStyleDeclaration> GetComputedStyle(
      const scoped_refptr<Element>& elt);
  scoped_refptr<cssom::CSSStyleDeclaration> GetComputedStyle(
      const scoped_refptr<Element>& elt, const std::string& pseudo_elt);

  // Web API: Timing control for script-based animations (partial interface)
  //   https://www.w3.org/TR/animation-timing/#Window-interface-extensions
  int32 RequestAnimationFrame(
      const AnimationFrameRequestCallbackList::FrameRequestCallbackArg&);
  void CancelAnimationFrame(int32 handle);

  float viewport_diagonal_inches() const {
    return viewport_size_.diagonal_inches();
  }

  // Web API: CSSOM View Module (partial interface)
  //

  // Parses a media query.
  scoped_refptr<MediaQueryList> MatchMedia(const std::string& query);

  // As its name suggests, the Screen interface represents information about the
  // screen of the output device.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#the-screen-interface
  const scoped_refptr<Screen>& screen();

  // The innerWidth attribute must return the viewport width including the size
  // of a rendered scroll bar (if any), or zero if there is no viewport.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-innerwidth
  float inner_width() const {
    return static_cast<float>(viewport_size_.width());
  }
  // The innerHeight attribute must return the viewport height including the
  // size of a rendered scroll bar (if any), or zero if there is no viewport.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-innerheight
  float inner_height() const {
    return static_cast<float>(viewport_size_.height());
  }

  // The screenX attribute must return the x-coordinate, relative to the origin
  // of the screen of the output device, of the left of the client window as
  // number of pixels, or zero if there is no such thing.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-screenx
  float screen_x() const { return 0.0f; }
  // The screenY attribute must return the y-coordinate, relative to the origin
  // of the screen of the output device, of the top of the client window as
  // number of pixels, or zero if there is no such thing.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-screeny
  float screen_y() const { return 0.0f; }
  // The outerWidth attribute must return the width of the client window.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-outerwidth
  float outer_width() const { return inner_width(); }
  // The outerHeight attribute must return the height of the client window.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-outerheight
  float outer_height() const { return inner_height(); }
  // The devicePixelRatio attribute returns the ratio of CSS pixels per device
  // pixel.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-devicepixelratio
  float device_pixel_ratio() const {
    return viewport_size_.device_pixel_ratio();
  }

  // base64 encoding and decoding
  std::string Btoa(const std::string& string_to_encode,
                   script::ExceptionState* exception_state);

  std::vector<uint8_t> Atob(const std::string& encoded_string,
                            script::ExceptionState* exception_state);

  // Web API: Storage (implements)
  scoped_refptr<Storage> local_storage() const;
  scoped_refptr<Storage> session_storage() const;

  // Web API: WindowEventHandlers (implements)
  const EventListenerScriptValue* onhashchange() {
    return GetAttributeEventListener(base::Tokens::hashchange());
  }
  void set_onhashchange(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::hashchange(), event_listener);
  }

  const EventListenerScriptValue* onunload() {
    return GetAttributeEventListener(base::Tokens::unload());
  }
  void set_onunload(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::unload(), event_listener);
  }

  // Access to the Performance API (partial interface)
  //   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-window.performance-attribute
  const scoped_refptr<Performance>& performance() const;

  // Web API: SpeechSynthesisGetter (implements)
  //   https://dvcs.w3.org/hg/speech-api/raw-file/4f41ea1126bb/webspeechapi.html#tts-section
  scoped_refptr<speech::SpeechSynthesis> speech_synthesis() const;

  const scoped_refptr<Camera3D>& camera_3d() const;

#if defined(ENABLE_TEST_RUNNER)
  const scoped_refptr<TestRunner>& test_runner() const;
#endif  // ENABLE_TEST_RUNNER

  void Gc(script::EnvironmentSettings* settings);

  HTMLElementContext* html_element_context() const {
    return html_element_context_.get();
  }

  // Will fire the animation frame callbacks and reset the animation frame
  // request callback list.
  void RunAnimationFrameCallbacks();

  bool HasPendingAnimationFrameCallbacks() const;

  // Call this to inject an event into the window which will ultimately make
  // its way to the appropriate object in DOM.
  void InjectEvent(const scoped_refptr<web::Event>& event);

  scoped_refptr<Window> opener() const { return NULL; }

  // Sets the function to call to trigger a synchronous layout.
  using SynchronousLayoutAndProduceRenderTreeCallback =
      base::Callback<scoped_refptr<render_tree::Node>()>;
  void SetSynchronousLayoutCallback(
      const base::Closure& synchronous_layout_callback);
  void SetSynchronousLayoutAndProduceRenderTreeCallback(
      const SynchronousLayoutAndProduceRenderTreeCallback&
          synchronous_layout_callback);

  void SetSize(cssom::ViewportSize size);

  void UpdateCamera3D(const scoped_refptr<input::Camera3D>& camera_3d);

  void set_web_media_player_factory(
      media::WebMediaPlayerFactory* web_media_player_factory) {
    html_element_context_->set_web_media_player_factory(
        web_media_player_factory);
  }

  // Sets the current application state, forwarding on to the
  // ApplicationLifecycleState associated with it and its document, causing
  // precipitate events to be dispatched.
  void SetApplicationState(base::ApplicationState state,
                           SbTimeMonotonic timestamp);

  // ApplicationLifecycleState::Observer implementation.
  void OnWindowFocusChanged(bool has_focus) override;
  void OnVisibilityStateChanged(VisibilityState visibility_state) override;
  void OnFrozennessChanged(bool is_frozen) override;

  // Called when the document's root element has its offset dimensions requested
  // and is unable to provide them.
  void OnDocumentRootElementUnableToProvideOffsetDimensions();

  void OnWindowOnOnlineEvent();
  void OnWindowOnOfflineEvent();

  // Cache the passed in splash screen content for the window.location URL.
  void CacheSplashScreen(const std::string& content,
                         const base::Optional<std::string>& topic);

  void CancelScroll(const scoped_refptr<ui_navigation::NavItem>& nav_item);

  // Custom on screen keyboard.
  const scoped_refptr<OnScreenKeyboard>& on_screen_keyboard() const;
  void ReleaseOnScreenKeyboard();

  void OnStartDispatchEvent(const scoped_refptr<web::Event>& event);
  void OnStopDispatchEvent(const scoped_refptr<web::Event>& event);

  // |PointerState| will in general create reference cycles back to us, which is
  // ok, as they are cleared over time.  During shutdown however, since no
  // more queue progress can possibly be made, we must forcibly clear the
  // queue.
  void ClearPointerStateForShutdown();

  void TraceMembers(script::Tracer* tracer) override;

  const scoped_refptr<ui_navigation::NavItem>& GetUiNavRoot() const {
    return ui_nav_root_;
  }

  bool enable_map_to_mesh() { return enable_map_to_mesh_; }

  const scoped_refptr<media_session::MediaSession> media_session() const;

  loader::Loader* GetDocumentLoader() const { return document_loader_.get(); }

  DEFINE_WRAPPABLE_TYPE(Window);

 private:
  void StartDocumentLoad(
      loader::FetcherFactory* fetcher_factory, const GURL& url,
      Parser* dom_parser,
      const loader::Decoder::OnCompleteFunction& load_complete_callback);
  scoped_refptr<base::BasicClock> MakePerformanceClock(
      Window::ClockType clock_type);

  class RelayLoadEvent;

  ~Window() override;

  // From EventTarget.
  std::string GetDebugName() override { return "Window"; }

  void FireHashChangeEvent();

  cssom::ViewportSize viewport_size_;

  // A resize event can be pending if a resize occurs and the current visibility
  // state is not visible. In this case, the resize event will run when the
  // visibility state changes to visible.
  bool is_resize_event_pending_;

#if defined(ENABLE_TEST_RUNNER)
  scoped_refptr<TestRunner> test_runner_;
#endif  // ENABLE_TEST_RUNNER

  scoped_refptr<Performance> performance_;
  const std::unique_ptr<HTMLElementContext> html_element_context_;
  scoped_refptr<Document> document_;
  std::unique_ptr<loader::Loader> document_loader_;
  scoped_refptr<History> history_;
  scoped_refptr<Navigator> navigator_;
  std::unique_ptr<RelayLoadEvent> relay_on_load_event_;
  scoped_refptr<Camera3D> camera_3d_;
  std::unique_ptr<AnimationFrameRequestCallbackList>
      animation_frame_request_callback_list_;

  scoped_refptr<speech::SpeechSynthesis> speech_synthesis_;

  scoped_refptr<Storage> local_storage_;
  scoped_refptr<Storage> session_storage_;

  scoped_refptr<Screen> screen_;

  const base::Closure ran_animation_frame_callbacks_callback_;
  const CloseCallback window_close_callback_;
  const base::Closure window_minimize_callback_;

  scoped_refptr<OnScreenKeyboard> on_screen_keyboard_;

  CacheCallback splash_screen_cache_callback_;

  NavItemCallback cancel_scroll_callback_;

  OnStartDispatchEventCallback on_start_dispatch_event_callback_;
  OnStopDispatchEventCallback on_stop_dispatch_event_callback_;

  ScreenshotManager screenshot_manager_;

  // This UI navigation root container should contain all active UI navigation
  // items for this window.
  scoped_refptr<ui_navigation::NavItem> ui_nav_root_;

  bool enable_map_to_mesh_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_WINDOW_H_
