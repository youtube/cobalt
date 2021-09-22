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

#ifndef COBALT_BROWSER_WEB_MODULE_H_
#define COBALT_BROWSER_WEB_MODULE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/address_sanitizer.h"
#include "cobalt/base/source_location.h"
#include "cobalt/browser/lifecycle_observer.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/splash_screen_cache.h"
#include "cobalt/browser/user_agent_platform_info.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/on_screen_keyboard_bridge.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/screenshot_manager.h"
#include "cobalt/dom/wheel_event_init.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/math/size.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/media_module.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/ui_navigation/nav_item.h"
#include "cobalt/webdriver/session_driver.h"
#include "url/gurl.h"

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/debugger_state.h"
#include "cobalt/debug/backend/render_overlay.h"
#include "cobalt/debug/console/command_manager.h"
#endif  // ENABLE_DEBUGGER

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
// implementation object is constructed that manages all internal components.
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
    base::FilePath extra_web_file_dir;

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

    // If true, Cobalt will log a warning each time it parses a non-async
    // <script> tag inlined in HTML.  Cobalt has a known issue where if it is
    // blurred or frozen while loading inlined <script> tags, it will abort
    // the script fetch and silently fail without any follow up actions.  It is
    // recommended that production code always avoid non-async <script> tags
    // inlined in HTML.  This is likely not an issue for tests, however, where
    // we control the freeze/unfreeze activities, so this flag can be used in
    // these cases to disable the warning.
    bool enable_inline_script_warnings = true;

    // Encoded image cache capacity in bytes.
    int encoded_image_cache_capacity = 1024 * 1024;

    // Image cache capacity in bytes.
    int image_cache_capacity = 32 * 1024 * 1024;

    // Typeface cache capacity in bytes.
    int remote_typeface_cache_capacity = 4 * 1024 * 1024;

    // Mesh cache capacity in bytes.
    int mesh_cache_capacity;

    // Whether map-to-mesh is enabled.
    bool enable_map_to_mesh = true;

    // Content Security Policy enforcement mode for this web module.
    dom::CspEnforcementType csp_enforcement_mode = dom::kCspEnforcementEnable;

    // Token obtained from CSP to allow creation of insecure delegates.
    int csp_insecure_allowed_token = 0;

    // Whether or not the web module's stat tracker should track event stats.
    bool track_event_stats = false;

    // If set to something other than 1.0f, when a video starts to play, the
    // image cache will be flushed and temporarily multiplied by this value (
    // must be less than or equal to 1.0f) until the video ends.  This can
    // help for platforms that are low on image memory while playing a video.
    float image_cache_capacity_multiplier_when_playing_video = 1.0f;

    // Specifies the priority of the web module's thread.  This is the thread
    // that is responsible for executing JavaScript, managing the DOM, and
    // performing layouts.  The default value is base::ThreadPriority::NORMAL.
    base::ThreadPriority thread_priority = base::ThreadPriority::NORMAL;

    // Specifies the priority that the web module's corresponding loader thread
    // will be assigned.  This is the thread responsible for performing resource
    // decoding, such as image decoding.  The default value is
    // base::ThreadPriority::BACKGROUND.
    base::ThreadPriority loader_thread_priority =
        base::ThreadPriority::BACKGROUND;

    // Specifies the priority that the web module's animated image decoding
    // thread will be assigned. This thread is responsible for decoding,
    // blending and constructing individual frames from animated images. The
    // default value is base::ThreadPriority::BACKGROUND.
    base::ThreadPriority animated_image_decode_thread_priority =
        base::ThreadPriority::BACKGROUND;

    // To support 3D camera movements.
    scoped_refptr<input::Camera3D> camera_3d;

    script::JavaScriptEngine::Options javascript_engine_options;

    // The video playback rate will be multiplied with the following value.  Its
    // default value is 1.0.
    float video_playback_rate_multiplier = 1.f;

    // Allows image animations to be enabled/disabled.  Its default value
    // is true to enable them.
    bool enable_image_animations = true;

    // Whether or not to retain the remote typeface cache when the app enters
    // the frozen state.
    bool should_retain_remote_typeface_cache_on_freeze = false;

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
    bool can_fetch_cache = false;

    // The dom::OnScreenKeyboard forwards calls to this interface.
    dom::OnScreenKeyboardBridge* on_screen_keyboard_bridge = NULL;

    // This function takes in a render tree as input, and then calls the 2nd
    // argument (which is another callback) when the screenshot is available.
    // The callback's first parameter points to an unencoded image, where the
    // format is R8G8B8A8 pixels (with no padding at the end of each row),
    // and the second parameter is the dimensions of the image.
    // Note that the callback could be called on a different thread, and is not
    // guaranteed to be called on the caller thread.
    // By using Callbacks here, it is easier to write tests, and use this
    // functionality in Cobalt.
    dom::ScreenshotManager::ProvideScreenshotFunctionCallback
        provide_screenshot_function;

    // If true, the initial containing block's background color will be applied
    // as a clear, i.e. with blending disabled.  This means that a background
    // color of transparent will replace existing pixel values, effectively
    // clearing the screen.
    bool clear_window_with_background_color = true;

    // As a preventative measure against Spectre attacks, we explicitly limit
    // the resolution of the performance timer by default.  Setting this option
    // can allow the limit to be disabled.
    bool limit_performance_timer_resolution = true;

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
    // Whether layout is optimized to re-use boxes for still-valid elements.
    bool enable_partial_layout = true;
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

#if defined(ENABLE_DEBUGGER)
    // Whether the debugger should block until remote devtools connects.
    bool wait_for_web_debugger = false;

    // The debugger state returned from a previous web module's FreezeDebugger()
    // that should be restored in the new WebModule after navigation. Null if
    // there is no state to restore.
    debug::backend::DebuggerState* debugger_state = nullptr;
#endif  // defined(ENABLE_DEBUGGER)

    // This callback is for checking the mediasession actions transitions. When
    // there is no playback during Concealed state, we should provide a chance
    // for Cobalt to freeze.
    base::Closure maybe_freeze_callback;
  };

  typedef layout::LayoutManager::LayoutResults LayoutResults;
  typedef base::Callback<void(const LayoutResults&)>
      OnRenderTreeProducedCallback;
  typedef base::Callback<void(const GURL&, const std::string&)> OnErrorCallback;
  typedef dom::Window::CloseCallback CloseCallback;
  typedef base::Callback<void(const script::HeapStatistics&)>
      JavaScriptHeapStatisticsCallback;

  WebModule(const GURL& initial_url,
            base::ApplicationState initial_application_state,
            const OnRenderTreeProducedCallback& render_tree_produced_callback,
            const OnErrorCallback& error_callback,
            const CloseCallback& window_close_callback,
            const base::Closure& window_minimize_callback,
            media::CanPlayTypeHandler* can_play_type_handler,
            media::MediaModule* media_module,
            network::NetworkModule* network_module,
            const cssom::ViewportSize& window_dimensions,
            render_tree::ResourceProvider* resource_provider,
            float layout_refresh_rate, const Options& options);
  ~WebModule();

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
  // Injects an on screen keyboard input event into the web module. The value
  // for type represents beforeinput or input.
  void InjectOnScreenKeyboardInputEvent(base::Token type,
                                        const dom::InputEventInit& event);
  // Injects an on screen keyboard shown event into the web module.
  void InjectOnScreenKeyboardShownEvent(int ticket);
  // Injects an on screen keyboard hidden event into the web module.
  void InjectOnScreenKeyboardHiddenEvent(int ticket);
  // Injects an on screen keyboard focused event into the web module.
  void InjectOnScreenKeyboardFocusedEvent(int ticket);
  // Injects an on screen keyboard blurred event into the web module.
  void InjectOnScreenKeyboardBlurredEvent(int ticket);
  // Injects an on screen keyboard suggestions updated event into the web
  // module.
  void InjectOnScreenKeyboardSuggestionsUpdatedEvent(int ticket);
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

  void InjectWindowOnOnlineEvent(const base::Event* event);
  void InjectWindowOnOfflineEvent(const base::Event* event);

  // Injects a keyboard event into the web module. The value for type
  // represents the event name, for example 'keydown' or 'keyup'.
  void InjectKeyboardEvent(base::Token type,
                           const dom::KeyboardEventInit& event);

  // Injects a pointer event into the web module. The value for type represents
  // the event name, for example 'pointerdown', 'pointerup', or 'pointermove'.
  void InjectPointerEvent(base::Token type, const dom::PointerEventInit& event);

  // Injects a wheel event into the web module. The value for type represents
  // the event name, for example 'wheel'.
  void InjectWheelEvent(base::Token type, const dom::WheelEventInit& event);

  // Injects a beforeunload event into the web module. If this event is not
  // handled by the web application, |on_before_unload_fired_but_not_handled_|
  // will be called.
  void InjectBeforeUnloadEvent();

  void InjectCaptionSettingsChangedEvent();

  // Update the date/time configuration of relevant web modules.
  void UpdateDateTimeConfiguration();

  // Executes Javascript code in this web module.  The calling thread will
  // block until the JavaScript has executed and the output results are
  // available.
  std::string ExecuteJavascript(const std::string& script_utf8,
                                const base::SourceLocation& script_location,
                                bool* out_succeeded);

#if defined(ENABLE_WEBDRIVER)
  // Creates a new webdriver::WindowDriver that interacts with the Window that
  // is owned by this WebModule instance.
  std::unique_ptr<webdriver::WindowDriver> CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id);
#endif

#if defined(ENABLE_DEBUGGER)
  // Gets a reference to the debug dispatcher that interacts with this web
  // module. The debug dispatcher is part of the debug module owned by this web
  // module, which is lazily created by this function if necessary.
  debug::backend::DebugDispatcher* GetDebugDispatcher();

  // Moves the debugger state out of this WebModule prior to navigating so that
  // it can be restored in the new WebModule after the navigation.
  std::unique_ptr<debug::backend::DebuggerState> FreezeDebugger();
#endif  // ENABLE_DEBUGGER

  // Sets the size of this web module, possibly causing relayout and re-render
  // with the new parameters. Does nothing if the parameters are not different
  // from the current parameters.
  void SetSize(const cssom::ViewportSize& viewport_size);

  void UpdateCamera3D(const scoped_refptr<input::Camera3D>& camera_3d);
  void SetMediaModule(media::MediaModule* media_module);
  void SetImageCacheCapacity(int64_t bytes);
  void SetRemoteTypefaceCacheCapacity(int64_t bytes);

  // This returns the UI navigation root container which contains all active
  // UI navigation items created by this web module.
  const scoped_refptr<ui_navigation::NavItem>& GetUiNavRoot() const {
    return ui_nav_root_;
  }

  // LifecycleObserver implementation
  void Blur(SbTimeMonotonic timestamp) override;
  void Conceal(render_tree::ResourceProvider* resource_provider,
               SbTimeMonotonic timestamp) override;
  void Freeze(SbTimeMonotonic timestamp) override;
  void Unfreeze(render_tree::ResourceProvider* resource_provider,
                SbTimeMonotonic timestamp) override;
  void Reveal(render_tree::ResourceProvider* resource_provider,
              SbTimeMonotonic timestamp) override;
  void Focus(SbTimeMonotonic timestamp) override;

  // Attempt to reduce overall memory consumption. Called in response to a
  // system indication that memory usage is nearing a critical level.
  void ReduceMemory();

  // Post a task that gets the current |script::HeapStatistics| for our
  // |JavaScriptEngine| to the web module thread, and then passes that to
  // |callback|.  Note that |callback| will be called on the main web module
  // thread.  It is the responsibility of |callback| to get back to its
  // intended thread should it want to.
  void RequestJavaScriptHeapStatistics(
      const JavaScriptHeapStatisticsCallback& callback);

  // Indicate the web module is ready to freeze.
  bool IsReadyToFreeze();

  scoped_refptr<render_tree::Node> DoSynchronousLayoutAndGetRenderTree();

  void SetApplicationStartOrPreloadTimestamp(bool is_preload,
                                             SbTimeMonotonic timestamp);
  void SetDeepLinkTimestamp(SbTimeMonotonic timestamp);

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
        media::CanPlayTypeHandler* can_play_type_handler,
        media::MediaModule* media_module,
        network::NetworkModule* network_module,
        const cssom::ViewportSize& window_dimensions,
        render_tree::ResourceProvider* resource_provider,
        int dom_max_element_depth, float layout_refresh_rate,
        const scoped_refptr<ui_navigation::NavItem>& ui_nav_root,
        const Options& options)
        : initial_url(initial_url),
          initial_application_state(initial_application_state),
          render_tree_produced_callback(render_tree_produced_callback),
          error_callback(error_callback),
          window_close_callback(window_close_callback),
          window_minimize_callback(window_minimize_callback),
          can_play_type_handler(can_play_type_handler),
          media_module(media_module),
          network_module(network_module),
          window_dimensions(window_dimensions),
          resource_provider(resource_provider),
          dom_max_element_depth(dom_max_element_depth),
          layout_refresh_rate(layout_refresh_rate),
          ui_nav_root(ui_nav_root),
          options(options) {}

    GURL initial_url;
    base::ApplicationState initial_application_state;
    OnRenderTreeProducedCallback render_tree_produced_callback;
    OnErrorCallback error_callback;
    const CloseCallback& window_close_callback;
    const base::Closure& window_minimize_callback;
    media::CanPlayTypeHandler* can_play_type_handler;
    media::MediaModule* media_module;
    network::NetworkModule* network_module;
    cssom::ViewportSize window_dimensions;
    render_tree::ResourceProvider* resource_provider;
    int dom_max_element_depth;
    float layout_refresh_rate;
    scoped_refptr<ui_navigation::NavItem> ui_nav_root;
    Options options;
  };

  // Forward declaration of the private implementation class.
  class Impl;

  // Destruction observer used to safely tear down this WebModule after the
  // thread has been stopped.
  class DestructionObserver : public base::MessageLoop::DestructionObserver {
   public:
    explicit DestructionObserver(WebModule* web_module);
    void WillDestroyCurrentMessageLoop() override;

   private:
    WebModule* web_module_;
  };

  // Called by the constructor to create the private implementation object and
  // perform any other initialization required on the dedicated thread.
  void Initialize(const ConstructionData& data);

  void ClearAllIntervalsAndTimeouts();

  void CancelSynchronousLoads();

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const { return thread_.message_loop(); }

  // Private implementation object.
  std::unique_ptr<Impl> impl_;

  // The thread created and owned by this WebModule.
  // All sub-objects of this object are created on this thread, and all public
  // member functions are re-posted to this thread if necessary.
  base::Thread thread_;

  // This is the root UI navigation container which contains all active UI
  // navigation items created by this web module.
  scoped_refptr<ui_navigation::NavItem> ui_nav_root_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_WEB_MODULE_H_
