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

#ifndef COBALT_BROWSER_WEB_MODULE_H_
#define COBALT_BROWSER_WEB_MODULE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/base/address_sanitizer.h"
#include "cobalt/base/console_commands.h"
#include "cobalt/base/source_location.h"
#include "cobalt/browser/lifecycle_observer.h"
#include "cobalt/browser/splash_screen_cache.h"
#include "cobalt/css_parser/parser.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/render_overlay.h"
#endif  // ENABLE_DEBUG_CONSOLE
#include "cobalt/dom/blob.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/wheel_event_init.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/math/size.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/webdriver/session_driver.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

// WebModule hosts all components of Cobalt that deal with or implement the
// WebAPI.  This includes the ability to fetch resources given a URL, parse
// various web formats like HTML and CSS, host a DOM tree, manage a JavaScript
// engine, and lay out a web page.  Ultimately, interaction with WebModule is
// done through calls to InjectEvent() such as when dealing with external input
// (e.g. keyboards and gamepads), and handling render tree output from WebModule
// when it calls the on_render_tree_produced_ callback (provided upon
// construction).
// At creation, the WebModule starts a dedicated thread, on which a private
// implementation object is construted that manages all internal components.
// All methods of the WebModule post tasks to the implementation object on that
// thread, so all internal functions are executed synchronously with respect to
// each other.
// This necessarily implies that details contained within WebModule, such as the
// DOM, are intentionally kept private, since these structures expect to be
// accessed from only one thread.
class WebModule : public LifecycleObserver {
 public:
  struct Options {
    typedef base::Callback<scoped_refptr<script::Wrappable>(
        const scoped_refptr<dom::Window>& window,
        dom::MutationObserverTaskManager* mutation_observer_task_manager,
        script::GlobalEnvironment* global_environment)>
        CreateObjectFunction;
    typedef base::hash_map<std::string, CreateObjectFunction>
        InjectedWindowAttributes;

    // All optional parameters defined in this structure should have their
    // values initialized in the default constructor to useful defaults.
    Options();

    // The name of the WebModule.  This is useful for debugging purposes as in
    // the case where multiple WebModule objects exist, it can be used to
    // differentiate which objects belong to which WebModule.  It is used
    // to name some CVals.
    std::string name;

    // The LayoutTrigger parameter dictates when a layout should be triggered.
    // Tests will often set this up so that layouts are only performed when
    // we specifically request them to be.
    layout::LayoutManager::LayoutTrigger layout_trigger;

    // Optional directory to add to the search path for web files (file://).
    FilePath extra_web_file_dir;

    // The navigation_callback functor will be called when JavaScript internal
    // to the WebModule requests a page navigation, e.g. by modifying
    // 'window.location.href'.
    base::Callback<void(const GURL&)> navigation_callback;

    // A list of callbacks to be called once the web page finishes loading.
    std::vector<base::Closure> loaded_callbacks;

    // injected_window_attributes contains a map of attributes to be injected
    // into the WebModule's window object upon construction.  This provides
    // a mechanism to inject custom APIs into the WebModule object.
    InjectedWindowAttributes injected_window_attributes;

    // Options to customize DOMSettings.
    dom::DOMSettings::Options dom_settings_options;

    // Whether Cobalt is forbidden to render without receiving CSP headers.
    csp::CSPHeaderPolicy require_csp;

    // Image cache capacity in bytes.
    int image_cache_capacity;

    // Typeface cache capacity in bytes.
    int remote_typeface_cache_capacity;

    // Mesh cache capacity in bytes.
    int mesh_cache_capacity;

    // Content Security Policy enforcement mode for this web module.
    dom::CspEnforcementType csp_enforcement_mode;

    // Token obtained from CSP to allow creation of insecure delegates.
    int csp_insecure_allowed_token;

    // Whether or not the web module's stat tracker should track event stats.
    bool track_event_stats;

    // If set to something other than 1.0f, when a video starts to play, the
    // image cache will be flushed and temporarily multiplied by this value (
    // must be less than or equal to 1.0f) until the video ends.  This can
    // help for platforms that are low on image memory while playing a video.
    float image_cache_capacity_multiplier_when_playing_video;

    // Specifies the priority of the web module's thread.  This is the thread
    // that is responsible for executing JavaScript, managing the DOM, and
    // performing layouts.  The default value is base::kThreadPriority_Normal.
    base::ThreadPriority thread_priority;

    // Specifies the priority that the web module's corresponding loader thread
    // will be assigned.  This is the thread responsible for performing resource
    // decoding, such as image decoding.  The default value is
    // base::kThreadPriority_Low.
    base::ThreadPriority loader_thread_priority;

    // Specifies the priority tha the web module's animated image decoding
    // thread will be assigned. This thread is responsible for decoding,
    // blending and constructing individual frames from animated images. The
    // default value is base::kThreadPriority_Low.
    base::ThreadPriority animated_image_decode_thread_priority;

    // To support 3D camera movements.
    scoped_refptr<input::Camera3D> camera_3d;

    script::JavaScriptEngine::Options javascript_engine_options;

    // The video playback rate will be multiplied with the following value.  Its
    // default value is 1.0.
    float video_playback_rate_multiplier;

    // Allows image animations to be enabled/disabled.  Its default value
    // is true to enable them.
    bool enable_image_animations;

    // Whether or not to retain the remote typeface cache when the app enters
    // the suspend state.
    bool should_retain_remote_typeface_cache_on_suspend;

    // The language and script to use with fonts. If left empty, then the
    // language-script combination provided by base::GetSystemLanguageScript()
    // is used.
    std::string font_language_script_override;

    // The splash screen cache object, owned by the BrowserModule.
    SplashScreenCache* splash_screen_cache;

    // The beforeunload event can give a web page a chance to shut
    // itself down softly and ultimately call window.close(), however
    // if it is not handled by the web application, we indicate this
    // situation externally by calling this callback, so that if the
    // beforeunload event was generated it can be known that there is
    // no window.close() call pending.
    base::Closure on_before_unload_fired_but_not_handled;

    // Whether or not the WebModule is allowed to fetch from cache via
    // h5vcc-cache://.
    bool can_fetch_cache;
  };

  typedef layout::LayoutManager::LayoutResults LayoutResults;
  typedef base::Callback<void(const LayoutResults&)>
      OnRenderTreeProducedCallback;
  typedef base::Callback<void(const GURL&, const std::string&)> OnErrorCallback;
  typedef dom::Window::CloseCallback CloseCallback;

  WebModule(const GURL& initial_url,
            base::ApplicationState initial_application_state,
            const OnRenderTreeProducedCallback& render_tree_produced_callback,
            const OnErrorCallback& error_callback,
            const CloseCallback& window_close_callback,
            const base::Closure& window_minimize_callback,
            const dom::Window::GetSbWindowCallback& get_sb_window_callback,
            media::CanPlayTypeHandler* can_play_type_handler,
            media::WebMediaPlayerFactory* web_media_player_factory,
            network::NetworkModule* network_module,
            const math::Size& window_dimensions, float video_pixel_ratio,
            render_tree::ResourceProvider* resource_provider,
            float layout_refresh_rate, const Options& options);
  ~WebModule();

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // Call this to inject an on screen keyboard input event into the web module.
  // The value for type represents beforeinput or input.
  void InjectOnScreenKeyboardInputEvent(base::Token type,
                                        const dom::InputEventInit& event);
  // Call this to inject an on screen keyboard shown event into the web module.
  void InjectOnScreenKeyboardShownEvent();
  // Call this to inject an on screen keyboard hidden event into the web module.
  void InjectOnScreenKeyboardHiddenEvent();
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

  // Call this to inject a keyboard event into the web module. The value for
  // type represents the event name, for example 'keydown' or 'keyup'.
  void InjectKeyboardEvent(base::Token type,
                           const dom::KeyboardEventInit& event);

  // Call this to inject a pointer event into the web module. The value for type
  // represents the event name, for example 'pointerdown', 'pointerup', or
  // 'pointermove'.
  void InjectPointerEvent(base::Token type, const dom::PointerEventInit& event);

  // Call this to inject a wheel event into the web module. The value for type
  // represents the event name, for example 'wheel'.
  void InjectWheelEvent(base::Token type, const dom::WheelEventInit& event);

  // Call this to inject a beforeunload event into the web module. If
  // this event is not handled by the web application,
  // |on_before_unload_fired_but_not_handled_| will be called.
  void InjectBeforeUnloadEvent();

  // Call this to execute Javascript code in this web module.  The calling
  // thread will block until the JavaScript has executed and the output results
  // are available.
  std::string ExecuteJavascript(const std::string& script_utf8,
                                const base::SourceLocation& script_location,
                                bool* out_succeeded);

#if defined(ENABLE_WEBDRIVER)
  // Creates a new webdriver::WindowDriver that interacts with the Window that
  // is owned by this WebModule instance.
  scoped_ptr<webdriver::WindowDriver> CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id);
#endif

#if defined(ENABLE_DEBUG_CONSOLE)
  // Gets a reference to the debug server that interacts with this web module.
  // The debug server is part of the debug server module owned by this web
  // module, which is lazily created by this function if necessary.
  debug::DebugServer* GetDebugServer();
#endif  // ENABLE_DEBUG_CONSOLE

  // Sets the size and pixel ratio of this web module, possibly causing relayout
  // and re-render with the new parameters. Does nothing if the parameters are
  // not different from the current parameters.
  void SetSize(const math::Size& window_dimensions, float video_pixel_ratio);

  void SetCamera3D(const scoped_refptr<input::Camera3D>& camera_3d);
  void SetWebMediaPlayerFactory(
      media::WebMediaPlayerFactory* web_media_player_factory);
  void SetImageCacheCapacity(int64_t bytes);
  void SetRemoteTypefaceCacheCapacity(int64_t bytes);
  void SetJavascriptGcThreshold(int64_t bytes);

  // LifecycleObserver implementation
  void Prestart() OVERRIDE;
  void Start(render_tree::ResourceProvider* resource_provider) OVERRIDE;
  void Pause() OVERRIDE;
  void Unpause() OVERRIDE;
  void Suspend() OVERRIDE;
  void Resume(render_tree::ResourceProvider* resource_provider) OVERRIDE;

  // Attempt to reduce overall memory consumption. Called in response to a
  // system indication that memory usage is nearing a critical level.
  void ReduceMemory();

 private:
  // Data required to construct a WebModule, initialized in the constructor and
  // passed to |Initialize|.
  struct ConstructionData {
    ConstructionData(
        const GURL& initial_url,
        base::ApplicationState initial_application_state,
        const OnRenderTreeProducedCallback& render_tree_produced_callback,
        const OnErrorCallback& error_callback,
        const CloseCallback& window_close_callback,
        const base::Closure& window_minimize_callback,
        const dom::Window::GetSbWindowCallback& get_sb_window_callback,
        media::CanPlayTypeHandler* can_play_type_handler,
        media::WebMediaPlayerFactory* web_media_player_factory,
        network::NetworkModule* network_module,
        const math::Size& window_dimensions, float video_pixel_ratio,
        render_tree::ResourceProvider* resource_provider,
        int dom_max_element_depth, float layout_refresh_rate,
        const Options& options)
        : initial_url(initial_url),
          initial_application_state(initial_application_state),
          render_tree_produced_callback(render_tree_produced_callback),
          error_callback(error_callback),
          window_close_callback(window_close_callback),
          window_minimize_callback(window_minimize_callback),
          get_sb_window_callback(get_sb_window_callback),
          can_play_type_handler(can_play_type_handler),
          web_media_player_factory(web_media_player_factory),
          network_module(network_module),
          window_dimensions(window_dimensions),
          video_pixel_ratio(video_pixel_ratio),
          resource_provider(resource_provider),
          dom_max_element_depth(dom_max_element_depth),
          layout_refresh_rate(layout_refresh_rate),
          options(options) {}

    GURL initial_url;
    base::ApplicationState initial_application_state;
    OnRenderTreeProducedCallback render_tree_produced_callback;
    OnErrorCallback error_callback;
    const CloseCallback& window_close_callback;
    const base::Closure& window_minimize_callback;
    const dom::Window::GetSbWindowCallback& get_sb_window_callback;
    media::CanPlayTypeHandler* can_play_type_handler;
    media::WebMediaPlayerFactory* web_media_player_factory;
    network::NetworkModule* network_module;
    math::Size window_dimensions;
    float video_pixel_ratio;
    render_tree::ResourceProvider* resource_provider;
    int dom_max_element_depth;
    float layout_refresh_rate;
    Options options;
  };

  // Forward declaration of the private implementation class.
  class Impl;

  // Destruction observer used to safely tear down this WebModule after the
  // thread has been stopped.
  class DestructionObserver : public MessageLoop::DestructionObserver {
   public:
    explicit DestructionObserver(WebModule* web_module);
    void WillDestroyCurrentMessageLoop() OVERRIDE;

   private:
    WebModule* web_module_;
  };

  // Called by the constructor to create the private implementation object and
  // perform any other initialization required on the dedicated thread.
  void Initialize(const ConstructionData& data);

  void ClearAllIntervalsAndTimeouts();

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  void OnPartialLayoutConsoleCommandReceived(const std::string& message);
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  // The message loop this object is running on.
  MessageLoop* message_loop() const { return thread_.message_loop(); }

  // Private implementation object.
  scoped_ptr<Impl> impl_;

  // The thread created and owned by this WebModule.
  // All sub-objects of this object are created on this thread, and all public
  // member functions are re-posted to this thread if necessary.
  base::Thread thread_;

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  // Handles the 'partial_layout' command.
  scoped_ptr<base::ConsoleCommandManager::CommandHandler>
      partial_layout_command_handler_;
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_WEB_MODULE_H_
