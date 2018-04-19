// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_BROWSER_MODULE_H_
#define COBALT_BROWSER_BROWSER_MODULE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "cobalt/account/account_manager.h"
#include "cobalt/base/accessibility_caption_settings_changed_event.h"
#include "cobalt/base/application_state.h"
#include "cobalt/base/message_queue.h"
#include "cobalt/base/on_screen_keyboard_blurred_event.h"
#include "cobalt/base/on_screen_keyboard_focused_event.h"
#include "cobalt/base/on_screen_keyboard_hidden_event.h"
#include "cobalt/base/on_screen_keyboard_shown_event.h"
#include "cobalt/browser/h5vcc_url_handler.h"
#include "cobalt/browser/lifecycle_observer.h"
#include "cobalt/browser/memory_settings/auto_mem.h"
#include "cobalt/browser/memory_settings/checker.h"
#include "cobalt/browser/render_tree_combiner.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/splash_screen.h"
#include "cobalt/browser/suspend_fuzzer.h"
#include "cobalt/browser/system_platform_error_handler.h"
#include "cobalt/browser/url_handler.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/on_screen_keyboard_bridge.h"
#include "cobalt/dom/pointer_event_init.h"
#include "cobalt/dom/wheel_event_init.h"
#include "cobalt/input/input_device_manager.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/media_module.h"
#include "cobalt/network/network_module.h"
#include "cobalt/overlay_info/qr_code_overlay.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/webdriver/session_driver.h"
#include "googleurl/src/gurl.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/base/console_commands.h"
#include "cobalt/browser/debug_console.h"
#include "cobalt/browser/trace_manager.h"
#include "cobalt/debug/debug_server.h"
#endif  // ENABLE_DEBUG_CONSOLE
#include "starboard/configuration.h"
#include "starboard/window.h"

namespace cobalt {
namespace browser {

// BrowserModule hosts all major components of the Cobalt browser application.
// It also contains all of the glue components required to connect the
// different subsystems together.
class BrowserModule {
 public:
  // All browser subcomponent options should have default constructors that
  // setup reasonable default options.
  struct Options {
    explicit Options(const WebModule::Options& web_options)
        : web_module_options(web_options),
          command_line_auto_mem_settings(
              memory_settings::AutoMemSettings::kTypeCommandLine),
          build_auto_mem_settings(memory_settings::AutoMemSettings::kTypeBuild),
          enable_splash_screen_on_reloads(true) {}
    network::NetworkModule::Options network_module_options;
    renderer::RendererModule::Options renderer_module_options;
    storage::StorageManager::Options storage_manager_options;
    WebModule::Options web_module_options;
    media::MediaModule::Options media_module_options;
    std::string initial_deep_link;
    base::Closure web_module_recreated_callback;
    memory_settings::AutoMemSettings command_line_auto_mem_settings;
    memory_settings::AutoMemSettings build_auto_mem_settings;
    base::optional<GURL> fallback_splash_screen_url;
    base::optional<math::Size> requested_viewport_size;
    bool enable_splash_screen_on_reloads;
  };

  // Type for a collection of URL handler callbacks that can potentially handle
  // a URL before using it to initialize a new WebModule.
  typedef std::vector<URLHandler::URLHandlerCallback> URLHandlerCollection;

  BrowserModule(const GURL& url,
                base::ApplicationState initial_application_state,
                base::EventDispatcher* event_dispatcher,
                account::AccountManager* account_manager,
                const Options& options);
  ~BrowserModule();

  const std::string& GetUserAgent() { return network_module_.GetUserAgent(); }

  // Recreates web module with the given URL. In the case where Cobalt is
  // currently suspended, this defers the navigation and instead sets
  // |pending_navigate_url_| to the specified url, which will trigger a
  // navigation when Cobalt resumes.
  void Navigate(const GURL& url);
  // Reloads web module.
  void Reload();

  SystemPlatformErrorHandler* system_platform_error_handler() {
    return &system_platform_error_handler_;
  }

  // Adds/removes a URL handler.
  void AddURLHandler(const URLHandler::URLHandlerCallback& callback);
  void RemoveURLHandler(const URLHandler::URLHandlerCallback& callback);

  // Request a screenshot to be written to the specified path. Callback will
  // be fired after the screenshot has been written to disk.
  void RequestScreenshotToFile(
      const FilePath& path,
      loader::image::EncodedStaticImage::ImageFormat image_format,
      const base::Closure& done_cb);

  // Request a screenshot to an in-memory buffer.
  void RequestScreenshotToBuffer(
      loader::image::EncodedStaticImage::ImageFormat image_format,
      const ScreenShotWriter::ImageEncodeCompleteCallback& screenshot_ready);

#if defined(ENABLE_WEBDRIVER)
  scoped_ptr<webdriver::SessionDriver> CreateSessionDriver(
      const webdriver::protocol::SessionId& session_id);
#endif

#if defined(ENABLE_DEBUG_CONSOLE)
  debug::DebugServer* GetDebugServer();
  void GetDebugServerInternal(debug::DebugServer** out_debug_server);
#endif  // ENABLE_DEBUG_CONSOLE

  // Change the network proxy settings while the application is running.
  void SetProxy(const std::string& proxy_rules);

  // LifecycleObserver-similar interface.
  void Start();
  void Pause();
  void Unpause();
  void Suspend();
  void Resume();

  // Attempt to reduce overall memory consumption. Called in response to a
  // system indication that memory usage is nearing a critical level.
  void ReduceMemory();

  void CheckMemory(const int64_t& used_cpu_memory,
                   const base::optional<int64_t>& used_gpu_memory);

  // Post a task to the main web module to update
  // |javascript_reserved_memory_|.
  void UpdateJavaScriptHeapStatistics();

#if SB_API_VERSION >= 8
  // Called when a kSbEventTypeWindowSizeChange event is fired.
  void OnWindowSizeChanged(const SbWindowSize& size);
#endif  // SB_API_VERSION >= 8

#if SB_HAS(ON_SCREEN_KEYBOARD)
  void OnOnScreenKeyboardShown(const base::OnScreenKeyboardShownEvent* event);
  void OnOnScreenKeyboardHidden(const base::OnScreenKeyboardHiddenEvent* event);
  void OnOnScreenKeyboardFocused(
      const base::OnScreenKeyboardFocusedEvent* event);
  void OnOnScreenKeyboardBlurred(
      const base::OnScreenKeyboardBlurredEvent* event);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

#if SB_HAS(CAPTIONS)
  void OnCaptionSettingsChanged(
      const base::AccessibilityCaptionSettingsChangedEvent* event);
#endif  // SB_HAS(CAPTIONS)

 private:
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
  static void CoreDumpHandler(void* browser_module_as_void);
  int on_error_triggered_count_;
#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  int recovery_mechanism_triggered_count_;
  int timeout_response_trigger_count_;
#endif  // defined(COBALT_CHECK_RENDER_TIMEOUT)
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)

  // Called when the WebModule's Window.onload event is fired.
  void OnLoad();

  // Wait for the onload event to be fired with the specified timeout. If the
  // webmodule is not currently loading the document, this will return
  // immediately.
  bool WaitForLoad(const base::TimeDelta& timeout);

  // Glue function to deal with the production of the main render tree,
  // and will manage handing it off to the renderer.
  void QueueOnRenderTreeProduced(
      int web_module_generation,
      const browser::WebModule::LayoutResults& layout_results);
  void OnRenderTreeProduced(
      int web_module_generation,
      const browser::WebModule::LayoutResults& layout_results);

  // Glue function to deal with the production of the splash screen render tree,
  // and will manage handing it off to the renderer.
  void QueueOnSplashScreenRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);
  void OnSplashScreenRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  // Glue function to deal with the production of the qr code overlay render
  // tree, and will manage handing it off to the renderer.
  void QueueOnQrCodeOverlayRenderTreeProduced(
      const scoped_refptr<render_tree::Node>& render_tree);
  void OnQrCodeOverlayRenderTreeProduced(
      const scoped_refptr<render_tree::Node>& render_tree);

  // Saves/loads the debug console mode to/from local storage so we can
  // persist the user's preference.
  void SaveDebugConsoleMode();

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // Glue function to deal with the production of an input event from an on
  // screen keyboard input device, and manage handing it off to the web module
  // for interpretation.
  void OnOnScreenKeyboardInputEventProduced(base::Token type,
                                            const dom::InputEventInit& event);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

  // Glue function to deal with the production of a keyboard input event from a
  // keyboard input device, and manage handing it off to the web module for
  // interpretation.
  void OnKeyEventProduced(base::Token type,
                          const dom::KeyboardEventInit& event);

  // Glue function to deal with the production of a pointer input event from a
  // pointer input device, and manage handing it off to the web module for
  // interpretation.
  void OnPointerEventProduced(base::Token type,
                              const dom::PointerEventInit& event);

  // Glue function to deal with the production of a wheel input event from a
  // wheel input device, and manage handing it off to the web module for
  // interpretation.
  void OnWheelEventProduced(base::Token type, const dom::WheelEventInit& event);

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // Injects an on screen keyboard input event directly into the main web
  // module.
  void InjectOnScreenKeyboardInputEventToMainWebModule(
      base::Token type, const dom::InputEventInit& event);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

  // Injects a key event directly into the main web module, useful for setting
  // up an input fuzzer whose input should be sent directly to the main
  // web module and not filtered into the debug console.
  void InjectKeyEventToMainWebModule(base::Token type,
                                     const dom::KeyboardEventInit& event);

  // Error callback for any error that stops the program.
  void OnError(const GURL& url, const std::string& error);

  // OnErrorRetry() runs a retry URL through the URL handlers. It should only be
  // called by |on_error_retry_timer_|.
  void OnErrorRetry();

  // Filters a key event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEvent(base::Token type, const dom::KeyboardEventInit& event);

  // Filters a key event for hotkeys.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEventForHotkeys(base::Token type,
                                const dom::KeyboardEventInit& event);

  // Tries all registered URL handlers for a URL. Returns true if one of the
  // handlers handled the URL, false if otherwise.
  bool TryURLHandlers(const GURL& url);

  // Destroys the splash screen, if currently displayed.
  void DestroySplashScreen(base::TimeDelta close_time);

  // Called when web module has received window.close().
  void OnWindowClose(base::TimeDelta close_time);

  // Called when web module has received window.minimize().
  void OnWindowMinimize();

#if defined(ENABLE_DEBUG_CONSOLE)
  // Toggles the input fuzzer on/off.  Ignores the parameter.
  void OnFuzzerToggle(const std::string&);

  // Use the config in the form of '<string name>=<int value>' to call
  // MediaModule::SetConfiguration().
  void OnSetMediaConfig(const std::string& config);

  // Glue function to deal with the production of the debug console render tree,
  // and will manage handing it off to the renderer.
  void QueueOnDebugConsoleRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);
  void OnDebugConsoleRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);
#endif  // defined(ENABLE_DEBUG_CONSOLE)

#if defined(ENABLE_WEBDRIVER)
  scoped_ptr<webdriver::WindowDriver> CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id);

  void CreateWindowDriverInternal(
      const webdriver::protocol::WindowId& window_id,
      scoped_ptr<webdriver::WindowDriver>* out_window_driver);
#endif

  // Called when a renderer submission has been rasterized. Used to hide the
  // system splash screen after the first render has completed.
  void OnRendererSubmissionRasterized();

  // Process all messages queued into the |render_tree_submission_queue_|.
  void ProcessRenderTreeSubmissionQueue();

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  // Poll for render timeout. Called from timeout_polling_thread_.
  void OnPollForRenderTimeout(const GURL& url);
#endif

  // Gets the current resource provider.
  render_tree::ResourceProvider* GetResourceProvider();

  // Initializes the system window, and all components that require it.
  void InitializeSystemWindow();

  // Instantiates a renderer module and dependent objects.
  void InstantiateRendererModule();

  // Destroys the renderer module and dependent objects.
  void DestroyRendererModule();

  // Update web modules with the current viewport size.
  void UpdateScreenSize();

  // Does all the steps for either a Suspend or the first half of a Start.
  void SuspendInternal(bool is_start);

  // Does all the steps for either a Resume or the second half of a Start that
  // happen prior to the app state update.
  void StartOrResumeInternalPreStateUpdate(bool is_start);
  // Does all the steps for either a Resume or the second half of a Start that
  // happen after the app state update.
  void StartOrResumeInternalPostStateUpdate();

  // Gets a viewport size to use for now. This may change depending on the
  // current application state. While preloading, this returns the requested
  // viewport size. If there was no requested viewport size, it returns a
  // default viewport size of 1280x720 (720p). Once a system window is created,
  // it returns the confirmed size of the window.
  math::Size GetViewportSize();

  // Applies the current AutoMem settings to all applicable submodules.
  void ApplyAutoMemSettings();

  // The callback posted to the main web module in for
  // |UpdateJavaScriptHeapStatistics|.
  void GetHeapStatisticsCallback(const script::HeapStatistics& heap_statistics);

  // If it exists, takes the current combined render tree from
  // |render_tree_combiner_| and submits it to the pipeline in the renderer
  // module.
  void SubmitCurrentRenderTreeToRenderer();

  // Get the SbWindow via |system_window_| or potentially NULL.
  SbWindow GetSbWindow();

  // TODO:
  //     WeakPtr usage here can be avoided if BrowserModule has a thread to
  //     own where it can ensure that its tasks are all resolved when it is
  //     destructed.
  //
  // BrowserModule provides weak pointers to itself to prevent tasks generated
  // by its components when attempting to communicate to other components (e.g.
  // input events being generated and passed into WebModule).  By passing
  // references to BrowserModule as WeakPtrs, we do not access invalid memory
  // if they are processed after the BrowserModule is destroyed.
  base::WeakPtrFactory<BrowserModule> weak_ptr_factory_;
  // After constructing |weak_ptr_factory_| we immediately construct a WeakPtr
  // in order to bind the BrowserModule WeakPtr object to its thread.  When we
  // need a WeakPtr, we copy construct this, which is safe to do from any
  // thread according to weak_ptr.h (versus calling
  // |weak_ptr_factory_.GetWeakPtr() which is not).
  base::WeakPtr<BrowserModule> weak_this_;

  // Memory configuration tool.
  scoped_ptr<memory_settings::AutoMem> auto_mem_;

  // A copy of the BrowserModule Options passed into the constructor.
  Options options_;

  // The browser module runs on this message loop.
  MessageLoop* const self_message_loop_;

  // Handler for system errors, which is owned by browser module.
  SystemPlatformErrorHandler system_platform_error_handler_;

  // Collection of URL handlers that can potentially handle a URL before
  // using it to initialize a new WebModule.
  URLHandlerCollection url_handlers_;

  base::EventDispatcher* event_dispatcher_;

  storage::StorageManager storage_manager_;

  // Whether the browser module has yet rendered anything. On the very first
  // render, we hide the system splash screen.
  bool is_rendered_;

  // The main system window for our application. This routes input event
  // callbacks, and provides a native window handle on desktop systems.
  scoped_ptr<system_window::SystemWindow> system_window_;

  // Wraps input device and produces input events that can be passed into
  // the web module.
  scoped_ptr<input::InputDeviceManager> input_device_manager_;

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  scoped_ptr<renderer::RendererModule> renderer_module_;

  // A stub implementation of ResourceProvider that can be used until a real
  // ResourceProvider is created. Only valid in the Preloading state.
  base::optional<render_tree::ResourceProviderStub> resource_provider_stub_;

  // Controls all media playback related objects/resources.
  scoped_ptr<media::MediaModule> media_module_;

  // Allows checking if particular media type can be played.
  scoped_ptr<media::CanPlayTypeHandler> can_play_type_handler_;

  // Sets up the network component for requesting internet resources.
  network::NetworkModule network_module_;

  // Manages the three render trees, combines and renders them.
  RenderTreeCombiner render_tree_combiner_;
  scoped_ptr<RenderTreeCombiner::Layer> main_web_module_layer_;
  scoped_ptr<RenderTreeCombiner::Layer> splash_screen_layer_;
#if defined(ENABLE_DEBUG_CONSOLE)
  scoped_ptr<RenderTreeCombiner::Layer> debug_console_layer_;
#endif  // defined(ENABLE_DEBUG_CONSOLE)
  scoped_ptr<RenderTreeCombiner::Layer> qr_overlay_info_layer_;

  // Helper object to create screen shots of the last layout tree.
  scoped_ptr<ScreenShotWriter> screen_shot_writer_;

  // Keeps track of all messages containing render tree submissions that will
  // ultimately reference the |render_tree_combiner_| and the
  // |renderer_module_|.  It is important that this is destroyed before the
  // above mentioned references are.  It must however outlive all WebModules
  // that may be producing render trees.
  base::MessageQueue render_tree_submission_queue_;

  // The splash screen cache.
  scoped_ptr<SplashScreenCache> splash_screen_cache_;

  scoped_ptr<dom::OnScreenKeyboardBridge> on_screen_keyboard_bridge_;
  bool on_screen_keyboard_show_called_ = false;

  // Sets up everything to do with web page management, from loading and
  // parsing the web page and all referenced files to laying it out.  The
  // web module will ultimately produce a render tree that can be passed
  // into the renderer module.
  scoped_ptr<WebModule> web_module_;

  // Will be signalled when the WebModule's Window.onload event is fired.
  base::WaitableEvent web_module_loaded_;

  // This will be called after the WebModule has been destroyed and recreated,
  // which could occur on navigation.
  base::Closure web_module_recreated_callback_;

  // The total number of navigations that have occurred.
  int navigate_count_;

  // The time when a URL navigation starts. This is recorded after the previous
  // WebModule is destroyed.
  base::CVal<int64, base::CValPublic> navigate_time_;

  // The time when the WebModule's Window.onload event is fired.
  base::CVal<int64, base::CValPublic> on_load_event_time_;

  // The total memory that is reserved by the JavaScript engine, which
  // includes both parts that have live JavaScript values, as well as
  // preallocated space for future values.
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      javascript_reserved_memory_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Possibly null, but if not, will contain a reference to an instance of
  // a debug fuzzer input device manager.
  scoped_ptr<input::InputDeviceManager> input_device_manager_fuzzer_;

  // Manages a second web module to implement the debug console.
  scoped_ptr<DebugConsole> debug_console_;

  TraceManager trace_manager_;

  // Command handler object for toggling the input fuzzer on/off.
  base::ConsoleCommandManager::CommandHandler fuzzer_toggle_command_handler_;

  // Command handler object for setting media module config.
  base::ConsoleCommandManager::CommandHandler set_media_config_command_handler_;

  // Command handler object for screenshot command from the debug console.
  base::ConsoleCommandManager::CommandHandler screenshot_command_handler_;

  base::optional<SuspendFuzzer> suspend_fuzzer_;
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  // Handler object for h5vcc URLs.
  scoped_ptr<H5vccURLHandler> h5vcc_url_handler_;

  // The splash screen. The pointer wrapped here should be non-NULL iff
  // the splash screen is currently displayed.
  scoped_ptr<SplashScreen> splash_screen_;

  // The qr code overlay to display qr codes on top of all layers.
  scoped_ptr<overlay_info::QrCodeOverlay> qr_code_overlay_;

  // Reset when the browser is paused, signalled to resume.
  base::WaitableEvent has_resumed_;

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  base::Thread timeout_polling_thread_;

  // Counts the number of continuous render timeout expirations. This value is
  // updated and used from OnPollForRenderTimeout.
  int render_timeout_count_;
#endif

  // The URL that Cobalt will attempt to navigate to during an OnErrorRetry()
  // and also when starting from a preloaded state or resuming from a suspended
  // state. This url is set within OnError() and also when a navigation is
  // deferred as a result of Cobalt being suspended; it is cleared when a
  // navigation occurs.
  std::string pending_navigate_url_;

  // The number of OnErrorRetry() calls that have occurred since the last
  // OnDone() call. This is used to determine the exponential backoff delay
  // between the call to OnError() and the timer call to OnErrorRetry().
  int on_error_retry_count_;
  // The time OnErrorRetry() was last called. This is used to limit how
  // frequently |on_error_retry_timer_| can call OnErrorRetry().
  base::TimeTicks on_error_retry_time_;
  // The timer for the next call to OnErrorRetry(). It is started in OnError()
  // when it is not already active.
  base::OneShotTimer<BrowserModule> on_error_retry_timer_;

  // Set when we've posted a system error for network failure until we receive
  // the next navigation. This is used to suppress retrying the current URL on
  // resume until the error retry occurs.
  bool waiting_for_error_retry_;

  // Set when the application is about to quit. May be set from a thread other
  // than the one hosting this object, and read from another.
  bool will_quit_;

  // The |will_quit_| flag may be set from one thread (e.g. not the one hosting
  // this object) and read from another. This lock is used to
  // ensure synchronous access.
  base::Lock quit_lock_;

  // The current application state.
  base::ApplicationState application_state_;

  // The list of LifecycleObserver that need to be managed.
  ObserverList<LifecycleObserver> lifecycle_observers_;

  // Fires memory warning once when memory exceeds specified max cpu/gpu
  // memory.
  memory_settings::Checker memory_settings_checker_;

  // The fallback URL to the splash screen. If empty (the default), no splash
  // screen will be displayed.
  base::optional<GURL> fallback_splash_screen_url_;

  // Number of main web modules that have take place so far, helpful for
  // ditinguishing lingering events produced by older web modules as we switch
  // from one to another.  This is incremented with each navigation.
  int main_web_module_generation_;

  // Keeps track of a unique next ID to be assigned to new splash screen or
  // main web module timelines.
  int next_timeline_id_;

  // The following values are specified on submissions sent into the renderer
  // so that the renderer can identify when it is changing from one timeline
  // to another (in which case it may need to clear its submission queue).
  int current_splash_screen_timeline_id_;
  int current_main_web_module_timeline_id_;

  // Remember the first set value for JavaScript's GC threshold setting computed
  // by automem.  We want this so that we can check that it never changes, since
  // we do not have the ability to modify it after startup.
  base::optional<int64_t> javascript_gc_threshold_in_bytes_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_BROWSER_MODULE_H_
