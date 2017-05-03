// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/dom/animation_frame_request_callback_list.h"
#include "cobalt/dom/crypto.h"
#include "cobalt/dom/csp_delegate_type.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/media_query_list.h"
#include "cobalt/dom/parser.h"
#include "cobalt/dom/url_registry.h"
#include "cobalt/network_bridge/cookie_jar.h"
#include "cobalt/network_bridge/net_poster.h"
#if defined(ENABLE_TEST_RUNNER)
#include "cobalt/dom/test_runner.h"
#endif  // ENABLE_TEST_RUNNER
#include "cobalt/dom/window_timers.h"
#include "cobalt/input/camera_3d.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/font/remote_typeface_cache.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/loader/loader.h"
#include "cobalt/loader/mesh/mesh_cache.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/system_window/system_window.h"
#include "googleurl/src/gurl.h"
#include "starboard/window.h"

namespace cobalt {
namespace media_session {
class MediaSession;
}  // namespace media_session
namespace speech {
class SpeechSynthesis;
}  // namespace speech
}  // namespace cobalt

namespace cobalt {
namespace dom {

class Camera3D;
class Console;
class Document;
class Element;
class Event;
class History;
class HTMLElementContext;
class LocalStorageDatabase;
class Location;
class MediaSource;
class Navigator;
class Performance;
class Screen;
class Storage;
class WindowTimers;

// The window object represents a window containing a DOM document.
//   https://www.w3.org/TR/html5/browsers.html#the-window-object
//
// TODO: Properly handle viewport resolution change event.
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
  typedef UrlRegistry<MediaSource> MediaSourceRegistry;

  Window(
      int width, int height, cssom::CSSParser* css_parser, Parser* dom_parser,
      loader::FetcherFactory* fetcher_factory,
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
      DomStatTracker* dom_stat_tracker, const GURL& url,
      const std::string& user_agent, const std::string& language,
      const base::Callback<void(const GURL&)> navigation_callback,
      const base::Callback<void(const std::string&)>& error_callback,
      network_bridge::CookieJar* cookie_jar,
      const network_bridge::PostSender& post_sender,
      const std::string& default_security_policy,
      dom::CspEnforcementType csp_enforcement_mode,
      const base::Closure& csp_policy_changed_callback,
      const base::Closure& ran_animation_frame_callbacks_callback,
      const base::Closure& window_close_callback,
      const base::Closure& window_minimize_callback,
      system_window::SystemWindow* system_window,
      const scoped_refptr<input::Camera3D>& camera_3d,
      const scoped_refptr<cobalt::media_session::MediaSession>& media_session,
      int csp_insecure_allowed_token = 0, int dom_max_element_depth = 0);

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
  scoped_refptr<Window> AnonymousIndexedGetter(unsigned int /* index */) {
    return NULL;
  }

  const scoped_refptr<Navigator>& navigator() const;

  // Web API: CSSOM (partial interface)
  // Returns the computed style of the given element, as described in
  // https://www.w3.org/TR/2013/WD-cssom-20131205/#dom-window-getcomputedstyle.
  scoped_refptr<cssom::CSSStyleDeclaration> GetComputedStyle(
      const scoped_refptr<Element>& elt);
  scoped_refptr<cssom::CSSStyleDeclaration> GetComputedStyle(
      const scoped_refptr<Element>& elt, const std::string& pseudoElt);

  // Web API: Timing control for script-based animations (partial interface)
  //   https://www.w3.org/TR/animation-timing/#Window-interface-extensions
  int32 RequestAnimationFrame(
      const AnimationFrameRequestCallbackList::FrameRequestCallbackArg&);
  void CancelAnimationFrame(int32 handle);

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
  float inner_width() const { return static_cast<float>(width_); }
  // The innerHeight attribute must return the viewport height including the
  // size of a rendered scroll bar (if any), or zero if there is no viewport.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-innerheight
  float inner_height() const { return static_cast<float>(height_); }

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
  float outer_width() const { return static_cast<float>(width_); }
  // The outerHeight attribute must return the height of the client window.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-outerheight
  float outer_height() const { return static_cast<float>(height_); }
  // The devicePixelRatio attribute returns the ratio of CSS pixels per device
  // pixel.
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-window-devicepixelratio
  float device_pixel_ratio() const { return device_pixel_ratio_; }

  // Web API: GlobalCrypto (implements)
  //   https://www.w3.org/TR/WebCryptoAPI/#crypto-interface
  scoped_refptr<Crypto> crypto() const;

  // Web API: WindowTimers (implements)
  //   https://www.w3.org/TR/html5/webappapis.html#timers
  //
  int SetTimeout(const WindowTimers::TimerCallbackArg& handler) {
    return SetTimeout(handler, 0);
  }

  int SetTimeout(const WindowTimers::TimerCallbackArg& handler, int timeout);

  void ClearTimeout(int handle);

  int SetInterval(const WindowTimers::TimerCallbackArg& handler) {
    return SetInterval(handler, 0);
  }

  int SetInterval(const WindowTimers::TimerCallbackArg& handler, int timeout);

  void ClearInterval(int handle);

  void DestroyTimers();

  // Web API: Storage (implements)
  scoped_refptr<Storage> local_storage() const;
  scoped_refptr<Storage> session_storage() const;

  // Access to the Performance API (partial interface)
  //   https://dvcs.w3.org/hg/webperf/raw-file/tip/specs/NavigationTiming/Overview.html#sec-window.performance-attribute
  const scoped_refptr<Performance>& performance() const;

  // Web API: SpeechSynthesisGetter (implements)
  //   https://dvcs.w3.org/hg/speech-api/raw-file/4f41ea1126bb/webspeechapi.html#tts-section
  scoped_refptr<speech::SpeechSynthesis> speech_synthesis() const;

  // Custom, not in any spec.
  //
  const scoped_refptr<Console>& console() const;

  const scoped_refptr<Camera3D>& camera_3d() const;

#if defined(ENABLE_TEST_RUNNER)
  const scoped_refptr<TestRunner>& test_runner() const;
#endif  // ENABLE_TEST_RUNNER

  void Gc(script::EnvironmentSettings* settings);

  HTMLElementContext* html_element_context() const;

  // Will fire the animation frame callbacks and reset the animation frame
  // request callback list.
  void RunAnimationFrameCallbacks();

  bool HasPendingAnimationFrameCallbacks() const;

  // Call this to inject an event into the window which will ultimately make
  // its way to the appropriate object in DOM.
  void InjectEvent(const scoped_refptr<Event>& event);

  scoped_refptr<Window> opener() const { return NULL; }

  // Sets the function to call to trigger a synchronous layout.
  void SetSynchronousLayoutCallback(
      const base::Closure& synchronous_layout_callback);

  void TraceMembers(script::Tracer* tracer) OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(Window);

 private:
  void StartDocumentLoad(
      loader::FetcherFactory* fetcher_factory, const GURL& url,
      Parser* dom_parser,
      const base::Callback<void(const std::string&)>& error_callback);
  class RelayLoadEvent;

  ~Window() OVERRIDE;

  // From EventTarget.
  std::string GetDebugName() OVERRIDE { return "Window"; }

  void FireHashChangeEvent();

  int width_;
  int height_;
  float device_pixel_ratio_;

  const scoped_ptr<HTMLElementContext> html_element_context_;
  scoped_refptr<Performance> performance_;
  scoped_refptr<Document> document_;
  scoped_ptr<loader::Loader> document_loader_;
  scoped_refptr<History> history_;
  scoped_refptr<Navigator> navigator_;
  scoped_ptr<RelayLoadEvent> relay_on_load_event_;
  scoped_refptr<Console> console_;
  scoped_refptr<Camera3D> camera_3d_;
  scoped_ptr<WindowTimers> window_timers_;
  scoped_ptr<AnimationFrameRequestCallbackList>
      animation_frame_request_callback_list_;

  scoped_refptr<Crypto> crypto_;
  scoped_refptr<speech::SpeechSynthesis> speech_synthesis_;

  scoped_refptr<Storage> local_storage_;
  scoped_refptr<Storage> session_storage_;

  scoped_refptr<Screen> screen_;

  const base::Closure ran_animation_frame_callbacks_callback_;
  const base::Closure window_close_callback_;
  const base::Closure window_minimize_callback_;

  system_window::SystemWindow* system_window_;

#if defined(ENABLE_TEST_RUNNER)
  scoped_refptr<TestRunner> test_runner_;
#endif  // ENABLE_TEST_RUNNER

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_WINDOW_H_
