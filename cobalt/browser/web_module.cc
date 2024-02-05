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

#include "cobalt/browser/web_module.h"

#include <memory>
#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/base/language.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/type_id.h"
#include "cobalt/browser/splash_screen_cache.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/browser/web_module_stat_tracker.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/input_event.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/navigation_type.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/storage.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/dom/visibility_state.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/layout/topmost_event_target.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/loader/loader_factory.h"
#include "cobalt/loader/switches.h"
#include "cobalt/media/decoder_buffer_allocator.h"
#include "cobalt/media/media_module.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/ui_navigation/scroll_engine/scroll_engine.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/context.h"
#include "cobalt/web/csp_delegate_factory.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event.h"
#include "cobalt/web/url.h"
#include "starboard/accessibility.h"
#include "starboard/common/log.h"
#include "starboard/gles.h"

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/backend/debug_module.h"  // nogncheck
#endif                                          // defined(ENABLE_DEBUGGER)

namespace cobalt {
namespace browser {

using cobalt::cssom::ViewportSize;

namespace {

// The maximum number of element depth in the DOM tree. Elements at a level
// deeper than this could be discarded, and will not be rendered.
const int kDOMMaxElementDepth = 32;

void CacheUrlContent(SplashScreenCache* splash_screen_cache,
                     const std::string& content,
                     const base::Optional<std::string>& topic) {
  splash_screen_cache->SplashScreenCache::CacheSplashScreen(content, topic);
}

base::Callback<void(const std::string&, const base::Optional<std::string>&)>
CacheUrlContentCallback(SplashScreenCache* splash_screen_cache) {
  // This callback takes in first the url, then the content string.
  if (splash_screen_cache) {
    return base::Bind(CacheUrlContent, base::Unretained(splash_screen_cache));
  } else {
    return base::Callback<void(const std::string&,
                               const base::Optional<std::string>&)>();
  }
}

void CancelScroll(ui_navigation::scroll_engine::ScrollEngine* scroll_engine,
                  const scoped_refptr<ui_navigation::NavItem>& nav_item) {
  auto nav_items = std::vector<scoped_refptr<ui_navigation::NavItem>>{nav_item};
  scroll_engine->thread()->message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ui_navigation::scroll_engine::ScrollEngine::
                                CancelActiveScrollsForNavItems,
                            base::Unretained(scroll_engine), nav_items));
}

dom::Window::NavItemCallback CancelScrollCallback(
    ui_navigation::scroll_engine::ScrollEngine* scroll_engine) {
  if (scroll_engine) {
    return base::Bind(CancelScroll, base::Unretained(scroll_engine));
  } else {
    return base::Callback<void(
        const scoped_refptr<cobalt::ui_navigation::NavItem>& nav_item)>();
  }
}

}  // namespace

// Private WebModule implementation. Each WebModule owns a single instance of
// this class, which performs all the actual work. All functions of this class
// must be called on the message loop of the WebModule thread, so they
// execute synchronously with respect to one another.
class WebModule::Impl {
 public:
  Impl(web::Context* web_context, const ConstructionData& data);
  ~Impl();

#if defined(ENABLE_DEBUGGER)
  debug::backend::DebugDispatcher* debug_dispatcher() {
    return debug_module_ ? debug_module_->debug_dispatcher() : nullptr;
  }
#endif  // ENABLE_DEBUGGER

  // Injects an on screen keyboard input event into the web module. Event is
  // directed at a specific element if the element is non-null. Otherwise, the
  // currently focused element receives the event. If element is specified, we
  // must be on the WebModule's message loop.
  void InjectOnScreenKeyboardInputEvent(
      base_token::Token type, const dom::InputEventInit& event,
      scoped_refptr<dom::Element> element = scoped_refptr<dom::Element>());
  // Injects an on screen keyboard shown event into the web module. Event is
  // directed at the on screen keyboard element.
  void InjectOnScreenKeyboardShownEvent(int ticket);
  // Injects an on screen keyboard hidden event into the web module. Event is
  // directed at the on screen keyboard element.
  void InjectOnScreenKeyboardHiddenEvent(int ticket);
  // Injects an on screen keyboard focused event into the web module. Event is
  // directed at the on screen keyboard element.
  void InjectOnScreenKeyboardFocusedEvent(int ticket);
  // Injects an on screen keyboard blurred event into the web module. Event is
  // directed at the on screen keyboard element.
  void InjectOnScreenKeyboardBlurredEvent(int ticket);
  // Injects an on screen keyboard suggestions updated event into the web
  // module. Event is directed at the on screen keyboard element.
  void InjectOnScreenKeyboardSuggestionsUpdatedEvent(int ticket);

  // Injects a keyboard event into the web module. Event is directed at a
  // specific element if the element is non-null. Otherwise, the currently
  // focused element receives the event. If element is specified, we must be
  // on the WebModule's message loop
  void InjectKeyboardEvent(
      base_token::Token type, const dom::KeyboardEventInit& event,
      scoped_refptr<dom::Element> element = scoped_refptr<dom::Element>());

  // Injects a pointer event into the web module. Event is directed at a
  // specific element if the element is non-null. Otherwise, the currently
  // focused element receives the event. If element is specified, we must be
  // on the WebModule's message loop
  void InjectPointerEvent(
      base_token::Token type, const dom::PointerEventInit& event,
      scoped_refptr<dom::Element> element = scoped_refptr<dom::Element>());

  // Injects a wheel event into the web module. Event is directed at a
  // specific element if the element is non-null. Otherwise, the currently
  // focused element receives the event. If element is specified, we must be
  // on the WebModule's message loop
  void InjectWheelEvent(
      base_token::Token type, const dom::WheelEventInit& event,
      scoped_refptr<dom::Element> element = scoped_refptr<dom::Element>());

  // Injects a beforeunload event into the web module. If this event is not
  // handled by the web application, |on_before_unload_fired_but_not_handled_|
  // will be called. The event is not directed at a specific element.
  void InjectBeforeUnloadEvent();

  void InjectCaptionSettingsChangedEvent();

  void InjectWindowOnOnlineEvent();
  void InjectWindowOnOfflineEvent();

  void UpdateDateTimeConfiguration();

  // Executes JavaScript in this WebModule. Sets the |result| output parameter
  // and signals |got_result|.
  void ExecuteJavascript(const std::string& script_utf8,
                         const base::SourceLocation& script_location,
                         std::string* result, bool* out_succeeded);

  // Clears disables timer related objects
  // so that the message loop can easily exit
  void ClearAllIntervalsAndTimeouts();

#if defined(ENABLE_WEBDRIVER)
  // Creates a new webdriver::WindowDriver that interacts with the Window that
  // is owned by this WebModule instance.
  void CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id,
      std::unique_ptr<webdriver::WindowDriver>* window_driver_out);
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUGGER)
  void WaitForWebDebugger();

  void FreezeDebugger(
      std::unique_ptr<debug::backend::DebuggerState>* debugger_state) {
    if (debugger_state) {
      if (debug_module_) {
        *debugger_state = debug_module_->Freeze();
      } else {
        debugger_state->reset();
      }
    }
  }
#endif  // defined(ENABLE_DEBUGGER)

  void SetSize(cssom::ViewportSize viewport_size);
  void UpdateCamera3D(const scoped_refptr<input::Camera3D>& camera_3d);
  void SetMediaModule(media::MediaModule* media_module);
  void SetImageCacheCapacity(int64_t bytes);
  void SetRemoteTypefaceCacheCapacity(int64_t bytes);

  // Sets the application state, asserts preconditions to transition to that
  // state, and dispatches any precipitate web events.
  void SetApplicationState(base::ApplicationState state,
                           SbTimeMonotonic timestamp);

  // See LifecycleObserver. These functions do not implement the interface, but
  // have the same basic function.
  void Blur(SbTimeMonotonic timestamp);
  void Conceal(render_tree::ResourceProvider* resource_provider,
               SbTimeMonotonic timestamp);
  void Freeze(SbTimeMonotonic timestamp);
  void Unfreeze(render_tree::ResourceProvider* resource_provider,
                SbTimeMonotonic timestamp);
  void Reveal(render_tree::ResourceProvider* resource_provider,
              SbTimeMonotonic timestamp);
  void Focus(SbTimeMonotonic timestamp);

  void ReduceMemory();

  void IsReadyToFreeze(volatile bool* is_ready_to_freeze) {
    if (window_->media_session()->media_session_client() == NULL) {
      *is_ready_to_freeze = true;
      return;
    }

    *is_ready_to_freeze =
        !window_->media_session()->media_session_client()->is_active();
  }

  void DoSynchronousLayoutAndGetRenderTree(
      scoped_refptr<render_tree::Node>* render_tree);

  void SetApplicationStartOrPreloadTimestamp(bool is_preload,
                                             SbTimeMonotonic timestamp);
  void SetDeepLinkTimestamp(SbTimeMonotonic timestamp);

  void SetUnloadEventTimingInfo(base::TimeTicks start_time,
                                base::TimeTicks end_time);

 private:
  class DocumentLoadedObserver;

  // Purge all resource caches owned by the WebModule.
  void PurgeResourceCaches(bool should_retain_remote_typeface_cache);

  // Disable callbacks in all resource caches owned by the WebModule.
  void DisableCallbacksInResourceCaches();

  // Called by |layout_manager_| after it runs the animation frame callbacks.
  void OnRanAnimationFrameCallbacks();

  // Called by |layout_manager_| when it produces a render tree. May modify
  // the render tree (e.g. to add a debug overlay), then runs the callback
  // specified in the constructor, |render_tree_produced_callback_|.
  void OnRenderTreeProduced(const LayoutResults& layout_results);

  // Called by the Renderer on the Renderer thread when it rasterizes a render
  // tree with this callback attached. It includes the time the render tree was
  // produced.
  void OnRenderTreeRasterized(
      scoped_refptr<base::SingleThreadTaskRunner> web_module_message_loop,
      const base::TimeTicks& produced_time);

  // WebModule thread handling of the OnRenderTreeRasterized() callback. It
  // includes the time that the render tree was produced and the time that the
  // render tree was rasterized.
  void ProcessOnRenderTreeRasterized(const base::TimeTicks& produced_time,
                                     const base::TimeTicks& rasterized_time);

  scoped_refptr<script::GlobalEnvironment> global_environment() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return web_context_->global_environment();
  }

  void OnLoadComplete(const base::Optional<std::string>& error) {
    if (error) error_callback_.Run(window_->location()->url(), *error);

    // Create Performance navigation timing info after document loading
    // completed.
    DCHECK(window_);
    DCHECK(window_->performance());
    if (window_->GetDocumentLoader()) {
      net::LoadTimingInfo load_timing_info =
          window_->GetDocumentLoader()->get_load_timing_info();
      // Check if the load happens through net mdoule.
      bool is_load_timing_info_valid =
          !load_timing_info.request_start.is_null();
      if (is_load_timing_info_valid) {
        window_->document()->CreatePerformanceNavigationTiming(
            window_->performance(), load_timing_info);
      }
    }
  }

  // Inject the DOM event object into the window or the element.
  void InjectInputEvent(scoped_refptr<dom::Element> element,
                        const scoped_refptr<web::Event>& event);

  // Handle queued pointer events. Called by LayoutManager on_layout callback.
  void HandlePointerEvents();

  // Initializes the ResourceProvider and dependent resources.
  void SetResourceProvider(render_tree::ResourceProvider* resource_provider);

  void OnStartDispatchEvent(const scoped_refptr<web::Event>& event);
  void OnStopDispatchEvent(const scoped_refptr<web::Event>& event);

  // Thread checker ensures all calls to the WebModule are made from the same
  // thread that it is created in.
  THREAD_CHECKER(thread_checker_);

  web::Context* web_context_;

  ui_navigation::scroll_engine::ScrollEngine* scroll_engine_;

  // Simple flag used for basic error checking.
  bool is_running_;

  // The most recent time that a new render tree was produced.
  base::TimeTicks last_render_tree_produced_time_;

  // Whether or not a render tree has been produced but not yet rasterized.
  base::CVal<bool, base::CValPublic> is_render_tree_rasterization_pending_;

  // Object that provides renderer resources like images and fonts.
  render_tree::ResourceProvider* resource_provider_;
  // The type id of resource provider being used by the WebModule. Whenever this
  // changes, the caches may have obsolete data and must be blown away.
  base::TypeId resource_provider_type_id_;

  // CSS parser.
  std::unique_ptr<css_parser::Parser> css_parser_;

  // DOM (HTML / XML) parser.
  std::unique_ptr<dom_parser::Parser> dom_parser_;

  // LoaderFactory that is used to acquire references to resources from a
  // URL.
  std::unique_ptr<loader::LoaderFactory> loader_factory_;

  std::unique_ptr<loader::image::AnimatedImageTracker> animated_image_tracker_;

  // ImageCache that is used to manage image cache logic.
  std::unique_ptr<loader::image::ImageCache> image_cache_;

  // The reduced cache capacity manager can be used to force a reduced image
  // cache over periods of time where memory is known to be restricted, such
  // as when a video is playing.
  std::unique_ptr<loader::image::ReducedCacheCapacityManager>
      reduced_image_cache_capacity_manager_;

  // RemoteTypefaceCache that is used to manage loading and caching typefaces
  // from URLs.
  std::unique_ptr<loader::font::RemoteTypefaceCache> remote_typeface_cache_;

  // MeshCache that is used to manage mesh cache logic.
  std::unique_ptr<loader::mesh::MeshCache> mesh_cache_;

  // Interface between LocalStorage and the Storage Manager.
  std::unique_ptr<dom::LocalStorageDatabase> local_storage_database_;

  // Stats for the web module. Both the dom stat tracker and layout stat
  // tracker are contained within it.
  std::unique_ptr<browser::WebModuleStatTracker> web_module_stat_tracker_;

  // Post and run tasks to notify MutationObservers.
  dom::MutationObserverTaskManager mutation_observer_task_manager_;

  // Object to register and retrieve MediaSource object with a string key.
  std::unique_ptr<dom::MediaSource::Registry> media_source_registry_;

  // The Window object wraps all DOM-related components.
  scoped_refptr<dom::Window> window_;

  // Cache a WeakPtr in the WebModule that is bound to the Window's message loop
  // so we can ensure that all subsequently created WeakPtr's are also bound to
  // the same loop.
  // See the documentation in base/memory/weak_ptr.h for details.
  base::WeakPtr<dom::Window> window_weak_;

  // Used only when MediaModule is null
  std::unique_ptr<media::DecoderBufferMemoryInfo>
      stub_decoder_buffer_memory_info_;

  // Called by |OnRenderTreeProduced|.
  OnRenderTreeProducedCallback render_tree_produced_callback_;

  // Called by |OnError|.
  OnErrorCallback error_callback_;

  // Triggers layout whenever the document changes.
  std::unique_ptr<layout::LayoutManager> layout_manager_;

#if defined(ENABLE_DEBUGGER)
  // Allows the debugger to add render components to the web module.
  // Used for DOM node highlighting and overlay messages.
  std::unique_ptr<debug::backend::RenderOverlay> debug_overlay_;

  // The core of the debugging system.
  std::unique_ptr<debug::backend::DebugModule> debug_module_;

  // Used to avoid a deadlock when running |Impl::Pause| while waiting for the
  // web debugger to connect.
  starboard::atomic_bool* waiting_for_web_debugger_;

  // Interface to report behaviour relevant to the web debugger.
  debug::backend::DebuggerHooksImpl debugger_hooks_;
#else
  // Null implementation used in gold builds without checking ENABLE_DEBUGGER.
  base::NullDebuggerHooks debugger_hooks_;
#endif  // ENABLE_DEBUGGER

  // DocumentObserver that observes the loading document.
  std::unique_ptr<DocumentLoadedObserver> document_load_observer_;

  std::unique_ptr<layout::TopmostEventTarget> topmost_event_target_;

  base::Closure on_before_unload_fired_but_not_handled_;

  bool should_retain_remote_typeface_cache_on_freeze_;

  scoped_refptr<cobalt::dom::captions::SystemCaptionSettings>
      system_caption_settings_;

  // This event is used to interrupt the loader when JavaScript is loaded
  // synchronously.  It is manually reset so that events like Freeze can be
  // correctly execute, even if there are multiple synchronous loads in queue
  // before the freeze (or other) event handlers.
  base::WaitableEvent* synchronous_loader_interrupt_;

  base::Callback<void(base::TimeTicks, base::TimeTicks)>
      report_unload_timing_info_callback_;
};

void WebModule::WillDestroyCurrentMessageLoop() { impl_.reset(); }

class WebModule::Impl::DocumentLoadedObserver : public dom::DocumentObserver {
 public:
  typedef std::vector<base::Closure> ClosureVector;
  explicit DocumentLoadedObserver(const ClosureVector& loaded_callbacks)
      : loaded_callbacks_(loaded_callbacks) {}
  // Called at most once, when document and all referred resources are loaded.
  void OnLoad() override {
    for (size_t i = 0; i < loaded_callbacks_.size(); ++i) {
      loaded_callbacks_[i].Run();
    }
  }

  void OnMutation() override {}
  void OnFocusChanged() override {}

 private:
  ClosureVector loaded_callbacks_;
};

WebModule::Impl::Impl(web::Context* web_context, const ConstructionData& data)
    : web_context_(web_context),
      scroll_engine_(data.scroll_engine),
      is_running_(false),
      is_render_tree_rasterization_pending_(
          base::StringPrintf("%s.IsRenderTreeRasterizationPending",
                             web_context_->name().c_str()),
          false, "True when a render tree is produced but not yet rasterized."),
      resource_provider_(data.resource_provider),
      resource_provider_type_id_(data.resource_provider->GetTypeId()),
#if defined(ENABLE_DEBUGGER)
      waiting_for_web_debugger_(data.waiting_for_web_debugger),
#endif  // defined(ENABLE_DEBUGGER)
      synchronous_loader_interrupt_(data.synchronous_loader_interrupt) {
  DCHECK(web_context_);
  css_parser::Parser::SupportsMapToMeshFlag supports_map_to_mesh =
      data.options.enable_map_to_mesh
          ? css_parser::Parser::kSupportsMapToMesh
          : css_parser::Parser::kDoesNotSupportMapToMesh;
  css_parser_ =
      css_parser::Parser::Create(debugger_hooks_, supports_map_to_mesh);
  DCHECK(css_parser_);

  dom_parser_.reset(new dom_parser::Parser(
      kDOMMaxElementDepth,
      base::Bind(&WebModule::Impl::OnLoadComplete, base::Unretained(this)),
      data.options.csp_header_policy));
  DCHECK(dom_parser_);

  on_before_unload_fired_but_not_handled_ =
      data.options.on_before_unload_fired_but_not_handled;

  should_retain_remote_typeface_cache_on_freeze_ =
      data.options.should_retain_remote_typeface_cache_on_freeze;

  DCHECK_LE(0, data.options.encoded_image_cache_capacity);
  loader_factory_.reset(new loader::LoaderFactory(
      web_context_->name().c_str(), web_context_->fetcher_factory(),
      resource_provider_, debugger_hooks_,
      data.options.encoded_image_cache_capacity,
      data.options.loader_thread_priority));

  animated_image_tracker_.reset(new loader::image::AnimatedImageTracker(
      web_context_->name().c_str(),
      data.options.animated_image_decode_thread_priority));

  DCHECK_LE(0, data.options.image_cache_capacity);
  image_cache_ = loader::image::CreateImageCache(
      base::StringPrintf("%s.ImageCache", web_context_->name().c_str()),
      debugger_hooks_, static_cast<uint32>(data.options.image_cache_capacity),
      loader_factory_.get());
  DCHECK(image_cache_);

  reduced_image_cache_capacity_manager_.reset(
      new loader::image::ReducedCacheCapacityManager(
          image_cache_.get(),
          data.options.image_cache_capacity_multiplier_when_playing_video));

  DCHECK_LE(0, data.options.remote_typeface_cache_capacity);
  remote_typeface_cache_ = loader::font::CreateRemoteTypefaceCache(
      base::StringPrintf("%s.RemoteTypefaceCache",
                         web_context_->name().c_str()),
      debugger_hooks_,
      static_cast<uint32>(data.options.remote_typeface_cache_capacity),
      loader_factory_.get());
  DCHECK(remote_typeface_cache_);

  DCHECK_LE(0, data.options.mesh_cache_capacity);
  mesh_cache_ = loader::mesh::CreateMeshCache(
      base::StringPrintf("%s.MeshCache", web_context_->name().c_str()),
      debugger_hooks_, static_cast<uint32>(data.options.mesh_cache_capacity),
      loader_factory_.get());
  DCHECK(mesh_cache_);

  local_storage_database_.reset(new dom::LocalStorageDatabase(
      web_context_->network_module()->storage_manager()));
  DCHECK(local_storage_database_);

  web_module_stat_tracker_.reset(new browser::WebModuleStatTracker(
      web_context_->name(), data.options.track_event_stats));
  DCHECK(web_module_stat_tracker_);

  media_source_registry_.reset(new dom::MediaSource::Registry);

  const media::DecoderBufferMemoryInfo* memory_info = nullptr;

  if (data.media_module) {
    memory_info = data.media_module->GetDecoderBufferAllocator();
  } else {
    stub_decoder_buffer_memory_info_.reset(
        new media::StubDecoderBufferMemoryInfo);
    memory_info = stub_decoder_buffer_memory_info_.get();
  }

  web_context_->SetupEnvironmentSettings(new dom::DOMSettings(
      debugger_hooks_, kDOMMaxElementDepth, media_source_registry_.get(),
      data.can_play_type_handler, memory_info, &mutation_observer_task_manager_,
      data.options.dom_settings_options));
  DCHECK(web_context_->environment_settings());
  // From algorithm to setup up a window environment settings object:
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#set-up-a-window-environment-settings-object
  // 6. Set settings object's creation URL to creationURL.
  web_context_->environment_settings()->set_creation_url(data.initial_url);

  system_caption_settings_ = new cobalt::dom::captions::SystemCaptionSettings(
      web_context_->environment_settings());

  dom::Window::CacheCallback splash_screen_cache_callback =
      CacheUrlContentCallback(data.options.splash_screen_cache);

  dom::Window::NavItemCallback cancel_scroll_callback =
      CancelScrollCallback(data.scroll_engine);

  // These members will reference other |Traceable|s, however are not
  // accessible from |Window|, so we must explicitly add them as roots.
  web_context_->global_environment()->AddRoot(&mutation_observer_task_manager_);
  web_context_->global_environment()->AddRoot(media_source_registry_.get());

#if defined(ENABLE_DEBUGGER)
  if (data.options.enable_debugger && data.options.wait_for_web_debugger) {
    // Post a task that blocks the message loop and waits for the web debugger.
    // This must be posted before the the window's task to load the document.
    waiting_for_web_debugger_->store(true);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebModule::Impl::WaitForWebDebugger,
                              base::Unretained(this)));
  }
#endif  // defined(ENABLE_DEBUGGER)

  bool log_tts = false;
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  log_tts =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseTTS);
#endif

  DCHECK(web_context_->network_module());
  window_ = new dom::Window(
      web_context_->environment_settings(), data.window_dimensions,
      data.initial_application_state, css_parser_.get(), dom_parser_.get(),
      web_context_->fetcher_factory(), loader_factory_.get(),
      &resource_provider_, animated_image_tracker_.get(), image_cache_.get(),
      reduced_image_cache_capacity_manager_.get(), remote_typeface_cache_.get(),
      mesh_cache_.get(), local_storage_database_.get(),
      data.can_play_type_handler, data.media_module,
      web_context_->execution_state(), web_context_->script_runner(),
      web_context_->global_environment()->script_value_factory(),
      media_source_registry_.get(),
      web_module_stat_tracker_->dom_stat_tracker(),
      base::GetSystemLanguageScript(), data.options.navigation_callback,
      base::Bind(&WebModule::Impl::OnLoadComplete, base::Unretained(this)),
      web_context_->network_module()->cookie_jar(),
      web::CspDelegate::Options(web_context_->network_module()->GetPostSender(),
                                data.options.csp_header_policy,
                                data.options.csp_enforcement_type,
                                data.options.csp_insecure_allowed_token),
      base::Bind(&WebModule::Impl::OnRanAnimationFrameCallbacks,
                 base::Unretained(this)),
      data.window_close_callback, data.window_minimize_callback,
      data.options.on_screen_keyboard_bridge, data.options.camera_3d,
      base::Bind(&WebModule::Impl::OnStartDispatchEvent,
                 base::Unretained(this)),
      base::Bind(&WebModule::Impl::OnStopDispatchEvent, base::Unretained(this)),
      data.options.provide_screenshot_function, cancel_scroll_callback,
      synchronous_loader_interrupt_, data.options.enable_inline_script_warnings,
      data.ui_nav_root, data.options.enable_map_to_mesh,
      data.options.csp_insecure_allowed_token, data.dom_max_element_depth,
      data.options.video_playback_rate_multiplier,
#if defined(ENABLE_TEST_RUNNER)
      data.options.layout_trigger == layout::LayoutManager::kTestRunnerMode
          ? dom::Window::kClockTypeTestRunner
          : (data.options.limit_performance_timer_resolution
                 ? dom::Window::kClockTypeResolutionLimitedSystemTime
                 : dom::Window::kClockTypeSystemTime),
#else
      dom::Window::kClockTypeSystemTime,
#endif
      splash_screen_cache_callback, system_caption_settings_, log_tts);
  DCHECK(window_);

  window_weak_ = base::AsWeakPtr(window_.get());
  DCHECK(window_weak_);

  web_context_->global_environment()->CreateGlobalObject(
      window_, web_context_->environment_settings());
  DCHECK(web_context_->GetWindowOrWorkerGlobalScope()->IsWindow());
  DCHECK(!web_context_->GetWindowOrWorkerGlobalScope()->IsDedicatedWorker());
  DCHECK(!web_context_->GetWindowOrWorkerGlobalScope()->IsServiceWorker());
  DCHECK(web_context_->GetWindowOrWorkerGlobalScope()->GetWrappableType() ==
         base::GetTypeId<dom::Window>());
  DCHECK_EQ(window_, base::polymorphic_downcast<dom::Window*>(
                         web_context_->GetWindowOrWorkerGlobalScope()));

  render_tree_produced_callback_ = data.render_tree_produced_callback;
  DCHECK(!render_tree_produced_callback_.is_null());

  error_callback_ = data.error_callback;
  DCHECK(!error_callback_.is_null());

  window_->navigator()->set_maybefreeze_callback(
      data.options.maybe_freeze_callback);
  window_->navigator()->set_media_player_factory(data.media_module);

  bool is_concealed =
      (data.initial_application_state == base::kApplicationStateConcealed);
  layout_manager_.reset(new layout::LayoutManager(
      is_concealed, web_context_->name(), window_.get(),
      base::Bind(&WebModule::Impl::OnRenderTreeProduced,
                 base::Unretained(this)),
      base::Bind(&WebModule::Impl::HandlePointerEvents, base::Unretained(this)),
      data.options.layout_trigger, data.dom_max_element_depth,
      data.layout_refresh_rate,
      web_context_->network_module()->preferred_language(),
      data.options.enable_image_animations,
      web_module_stat_tracker_->layout_stat_tracker(),
      data.options.clear_window_with_background_color));
  DCHECK(layout_manager_);

  if (!data.options.loaded_callbacks.empty()) {
    document_load_observer_.reset(
        new DocumentLoadedObserver(data.options.loaded_callbacks));
    window_->document()->AddObserver(document_load_observer_.get());
  }

#if defined(ENABLE_DEBUGGER)
  if (data.options.enable_debugger) {
    debug_overlay_.reset(
        new debug::backend::RenderOverlay(render_tree_produced_callback_));

    debug_module_.reset(new debug::backend::DebugModule(
        &debugger_hooks_, web_context_->global_environment(),
        debug_overlay_.get(), resource_provider_, window_,
        data.options.debugger_state));
  }
#endif  // ENABLE_DEBUGGER

  report_unload_timing_info_callback_ =
      data.options.collect_unload_event_time_callback;

  web_context_->SetupFinished();
  is_running_ = true;
}

WebModule::Impl::~Impl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  is_running_ = false;

  // Collect document's unload event start time.
  base::TimeTicks unload_event_start_time = base::TimeTicks::Now();

  window_->DispatchEvent(new web::Event(base::Tokens::unload()));

  // Collect document's unload event end time.
  base::TimeTicks unload_event_end_time = base::TimeTicks::Now();

  // Send the unload event start/end time back to application.
  if (!report_unload_timing_info_callback_.is_null()) {
    report_unload_timing_info_callback_.Run(unload_event_start_time,
                                            unload_event_end_time);
  }

  document_load_observer_.reset();

#if defined(ENABLE_DEBUGGER)
  debug_module_.reset();
  debug_overlay_.reset();
#endif  // ENABLE_DEBUGGER

  // Disable callbacks for the resource caches. Otherwise, it is possible for a
  // callback to occur into a DOM object that is being kept alive by a JS engine
  // reference even after the DOM tree has been destroyed. This can result in a
  // crash when the callback attempts to access a stale Document pointer.
  DisableCallbacksInResourceCaches();

  topmost_event_target_.reset();
  layout_manager_.reset();
  stub_decoder_buffer_memory_info_.reset();
  window_weak_.reset();
  window_->ClearPointerStateForShutdown();
  window_ = NULL;
  media_source_registry_.reset();
  web_context_->ShutDownJavaScriptEngine();
  local_storage_database_.reset();
  mesh_cache_.reset();
  remote_typeface_cache_.reset();
  // Warning: The image cache may contain animated images that rely on the
  // thread in AnimatedImageTracker, so it's important to destroy the image
  // cache before the animated image tracker object.
  image_cache_.reset();
  animated_image_tracker_.reset();
  dom_parser_.reset();
  css_parser_.reset();
  web_module_stat_tracker_.reset();
}

void WebModule::Impl::InjectInputEvent(scoped_refptr<dom::Element> element,
                                       const scoped_refptr<web::Event>& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::Impl::InjectInputEvent()",
               "event", TRACE_STR_COPY(event->type().c_str()));
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);

  if (element) {
    element->DispatchEvent(event);
  } else {
    if (dom::PointerState::CanQueueEvent(event)) {
      // As an optimization we batch together pointer/mouse events for as long
      // as we can get away with it (e.g. until a non-pointer event is received
      // or whenever the next layout occurs).
      window_->document()->pointer_state()->QueuePointerEvent(event);
    } else {
      // In order to maintain the correct input event ordering, we first
      // dispatch any queued pending pointer events.
      HandlePointerEvents();
      window_->InjectEvent(event);
    }
  }
}

void WebModule::Impl::InjectOnScreenKeyboardInputEvent(
    base_token::Token type, const dom::InputEventInit& event,
    scoped_refptr<dom::Element> element) {
  scoped_refptr<dom::InputEvent> input_event(
      new dom::InputEvent(type, window_, event));
  InjectInputEvent(element, input_event);
}

void WebModule::Impl::InjectOnScreenKeyboardShownEvent(int ticket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchShowEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardHiddenEvent(int ticket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchHideEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardFocusedEvent(int ticket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchFocusEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardBlurredEvent(int ticket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchBlurEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardSuggestionsUpdatedEvent(
    int ticket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchSuggestionsUpdatedEvent(ticket);
}


void WebModule::Impl::InjectKeyboardEvent(base_token::Token type,
                                          const dom::KeyboardEventInit& event,
                                          scoped_refptr<dom::Element> element) {
  scoped_refptr<dom::KeyboardEvent> keyboard_event(
      new dom::KeyboardEvent(type, window_, event));
  InjectInputEvent(element, keyboard_event);
}

void WebModule::Impl::InjectPointerEvent(base_token::Token type,
                                         const dom::PointerEventInit& event,
                                         scoped_refptr<dom::Element> element) {
  scoped_refptr<dom::PointerEvent> pointer_event(
      new dom::PointerEvent(type, window_, event));
  InjectInputEvent(element, pointer_event);
}

void WebModule::Impl::InjectWheelEvent(base_token::Token type,
                                       const dom::WheelEventInit& event,
                                       scoped_refptr<dom::Element> element) {
  scoped_refptr<dom::WheelEvent> wheel_event(
      new dom::WheelEvent(type, window_, event));
  InjectInputEvent(element, wheel_event);
}

void WebModule::Impl::UpdateDateTimeConfiguration() {
  if (web_context_ && web_context_->javascript_engine()) {
    web_context_->javascript_engine()->UpdateDateTimeConfiguration();
  }
}

void WebModule::Impl::ExecuteJavascript(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    std::string* result, bool* out_succeeded) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(web_context_->script_runner());

  // JavaScript is being run. Track it in the global stats.
  dom::GlobalStats::GetInstance()->StartJavaScriptEvent();

  // This should only be called for Cobalt JavaScript, error reports are
  // allowed.
  if (result) {
    *result = web_context_->script_runner()->Execute(
        script_utf8, script_location, false /*mute_errors*/, out_succeeded);
  } else {
    web_context_->script_runner()->Execute(
        script_utf8, script_location, false /*mute_errors*/, out_succeeded);
  }


  // JavaScript is done running. Stop tracking it in the global stats.
  dom::GlobalStats::GetInstance()->StopJavaScriptEvent();
}

void WebModule::Impl::ClearAllIntervalsAndTimeouts() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(window_);
  window_->DestroyTimers();
}

void WebModule::Impl::OnRanAnimationFrameCallbacks() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  // Notify the stat tracker that the animation frame callbacks have finished.
  // This may end the current event being tracked.
  web_module_stat_tracker_->OnRanAnimationFrameCallbacks(
      layout_manager_->IsRenderTreePending());
}

void WebModule::Impl::OnRenderTreeProduced(
    const LayoutResults& layout_results) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);

  last_render_tree_produced_time_ = base::TimeTicks::Now();
  is_render_tree_rasterization_pending_ = true;

  web_module_stat_tracker_->OnRenderTreeProduced(
      last_render_tree_produced_time_);

  LayoutResults layout_results_with_callback(
      layout_results.render_tree, layout_results.layout_time,
      base::Bind(&WebModule::Impl::OnRenderTreeRasterized,
                 base::Unretained(this), base::ThreadTaskRunnerHandle::Get(),
                 last_render_tree_produced_time_));

#if defined(ENABLE_DEBUGGER)
  if (debug_overlay_) {
    debug_overlay_->OnRenderTreeProduced(layout_results_with_callback);
  } else {
    render_tree_produced_callback_.Run(layout_results_with_callback);
  }
#else   // ENABLE_DEBUGGER
  render_tree_produced_callback_.Run(layout_results_with_callback);
#endif  // ENABLE_DEBUGGER
}

void WebModule::Impl::OnRenderTreeRasterized(
    scoped_refptr<base::SingleThreadTaskRunner> web_module_message_loop,
    const base::TimeTicks& produced_time) {
  web_module_message_loop->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::ProcessOnRenderTreeRasterized,
                            base::Unretained(this), produced_time,
                            base::TimeTicks::Now()));
}

void WebModule::Impl::ProcessOnRenderTreeRasterized(
    const base::TimeTicks& produced_time,
    const base::TimeTicks& rasterized_time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  web_module_stat_tracker_->OnRenderTreeRasterized(produced_time,
                                                   rasterized_time);
  animated_image_tracker_->OnRenderTreeRasterized();
  if (produced_time >= last_render_tree_produced_time_) {
    is_render_tree_rasterization_pending_ = false;
  }
}

void WebModule::Impl::DoSynchronousLayoutAndGetRenderTree(
    scoped_refptr<render_tree::Node>* render_tree) {
  TRACE_EVENT0("cobalt::browser",
               "WebModule::Impl::DoSynchronousLayoutAndGetRenderTree()");
  DCHECK(render_tree);
  scoped_refptr<render_tree::Node> tree =
      window_->document()->DoSynchronousLayoutAndGetRenderTree();
  if (render_tree) *render_tree = tree;
}

void WebModule::Impl::SetApplicationStartOrPreloadTimestamp(
    bool is_preload, SbTimeMonotonic timestamp) {
  DCHECK(window_);
  DCHECK(window_->performance());
  window_->performance()->SetApplicationStartOrPreloadTimestamp(is_preload,
                                                                timestamp);
}

void WebModule::Impl::SetDeepLinkTimestamp(SbTimeMonotonic timestamp) {
  DCHECK(window_);
  window_->performance()->SetDeepLinkTimestamp(timestamp);
}

void WebModule::Impl::SetUnloadEventTimingInfo(base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  DCHECK(window_);
  if (window_->document()) {
    window_->document()->SetUnloadEventTimingInfo(start_time, end_time);
  }
}

#if defined(ENABLE_WEBDRIVER)
void WebModule::Impl::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id,
    std::unique_ptr<webdriver::WindowDriver>* window_driver_out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_weak_);
  DCHECK(window_->document());
  DCHECK(web_context_->global_environment());

  window_driver_out->reset(new webdriver::WindowDriver(
      window_id, window_weak_,
      base::Bind(&WebModule::Impl::global_environment, base::Unretained(this)),
      base::Bind(&WebModule::Impl::InjectKeyboardEvent, base::Unretained(this)),
      base::Bind(&WebModule::Impl::InjectPointerEvent, base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get()));
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUGGER)
void WebModule::Impl::WaitForWebDebugger() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (debug_module_) {
    LOG(WARNING) << "\n-------------------------------------"
                    "\n Waiting for web debugger to connect "
                    "\n-------------------------------------";
    // This blocks until the web debugger connects.
    debug_module_->debug_dispatcher()->SetPaused(true);
  }
  waiting_for_web_debugger_->store(false);
}
#endif  // defined(ENABLE_DEBUGGER)

void WebModule::Impl::SetImageCacheCapacity(int64_t bytes) {
  image_cache_->SetCapacity(static_cast<uint32>(bytes));
}

void WebModule::Impl::SetRemoteTypefaceCacheCapacity(int64_t bytes) {
  remote_typeface_cache_->SetCapacity(static_cast<uint32>(bytes));
}

void WebModule::Impl::SetSize(cssom::ViewportSize viewport_size) {
  window_->SetSize(viewport_size);
}

void WebModule::Impl::UpdateCamera3D(
    const scoped_refptr<input::Camera3D>& camera_3d) {
  window_->UpdateCamera3D(camera_3d);
}

void WebModule::Impl::SetMediaModule(media::MediaModule* media_module) {
  SB_DCHECK(media_module);

  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(
          web_context_->environment_settings());

  dom_settings->set_decoder_buffer_memory_info(
      media_module->GetDecoderBufferAllocator());
  window_->set_web_media_player_factory(media_module);
}

void WebModule::Impl::SetApplicationState(base::ApplicationState state,
                                          SbTimeMonotonic timestamp) {
  window_->SetApplicationState(state, timestamp);
}

void WebModule::Impl::SetResourceProvider(
    render_tree::ResourceProvider* resource_provider) {
  resource_provider_ = resource_provider;
  if (resource_provider_) {
    base::TypeId resource_provider_type_id = resource_provider_->GetTypeId();
    // Check for if the resource provider type id has changed. If it has, then
    // anything contained within the caches is invalid and must be purged.
    if (resource_provider_type_id_ != resource_provider_type_id) {
      PurgeResourceCaches(should_retain_remote_typeface_cache_on_freeze_);
    }
    resource_provider_type_id_ = resource_provider_type_id;
  }
}

void WebModule::Impl::OnStartDispatchEvent(
    const scoped_refptr<web::Event>& event) {
  web_module_stat_tracker_->OnStartDispatchEvent(event);
}

void WebModule::Impl::OnStopDispatchEvent(
    const scoped_refptr<web::Event>& event) {
  web_module_stat_tracker_->OnStopDispatchEvent(
      event, window_->HasPendingAnimationFrameCallbacks(),
      layout_manager_->IsRenderTreePending());
}

void WebModule::Impl::Blur(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Blur()");
  SetApplicationState(base::kApplicationStateBlurred, timestamp);
}

void WebModule::Impl::Conceal(render_tree::ResourceProvider* resource_provider,
                              SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Conceal()");
  SetResourceProvider(resource_provider);

  SetApplicationState(base::kApplicationStateConcealed, timestamp);
  layout_manager_->Suspend();
  // Purge the cached resources prior to the freeze. That may cancel pending
  // loads, allowing the freeze to occur faster and preventing unnecessary
  // callbacks.
  window_->document()->PurgeCachedResources();

  // Clear out any currently tracked animating images.
  animated_image_tracker_->Reset();

  // Purge the resource caches before running any freeze logic. This will force
  // any pending callbacks that the caches are batching to run.
  PurgeResourceCaches(should_retain_remote_typeface_cache_on_freeze_);

#if defined(ENABLE_DEBUGGER)
  // The debug overlay may be holding onto a render tree, clear that out.
  if (debug_overlay_) debug_overlay_->ClearInput();
#endif

  // Force garbage collection in |javascript_engine|.
  if (web_context_ && web_context_->javascript_engine()) {
    web_context_->javascript_engine()->CollectGarbage();
  }

  loader_factory_->UpdateResourceProvider(resource_provider_);

  if (window_->media_session()->media_session_client() != NULL) {
    window_->media_session()
        ->media_session_client()
        ->PostDelayedTaskForMaybeFreezeCallback();
  }
}

void WebModule::Impl::Freeze(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Freeze()");
  SetApplicationState(base::kApplicationStateFrozen, timestamp);

  // Clear out the loader factory's resource provider, possibly aborting any
  // in-progress loads.
  loader_factory_->Suspend();
}

void WebModule::Impl::Unfreeze(render_tree::ResourceProvider* resource_provider,
                               SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Unfreeze()");
  synchronous_loader_interrupt_->Reset();
  DCHECK(resource_provider);

  loader_factory_->Resume(resource_provider);
  SetApplicationState(base::kApplicationStateConcealed, timestamp);
}

void WebModule::Impl::Reveal(render_tree::ResourceProvider* resource_provider,
                             SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Reveal()");
  synchronous_loader_interrupt_->Reset();
  DCHECK(resource_provider);
  SetResourceProvider(resource_provider);

  window_->document()->PurgeCachedResources();
  PurgeResourceCaches(should_retain_remote_typeface_cache_on_freeze_);

  loader_factory_->UpdateResourceProvider(resource_provider_);
  layout_manager_->Resume();

  SetApplicationState(base::kApplicationStateBlurred, timestamp);
}

void WebModule::Impl::Focus(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Focus()");
  synchronous_loader_interrupt_->Reset();
  SetApplicationState(base::kApplicationStateStarted, timestamp);
}

void WebModule::Impl::ReduceMemory() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::ReduceMemory()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_running_) {
    return;
  }
  synchronous_loader_interrupt_->Reset();

  layout_manager_->Purge();

  // Retain the remote typeface cache when reducing memory.
  PurgeResourceCaches(true /*should_retain_remote_typeface_cache*/);
  window_->document()->PurgeCachedResources();

  // Force garbage collection in |javascript_engine|.
  if (web_context_ && web_context_->javascript_engine()) {
    web_context_->javascript_engine()->CollectGarbage();
  }
}

void WebModule::Impl::InjectBeforeUnloadEvent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (window_ && window_->HasEventListener(base::Tokens::beforeunload())) {
    window_->DispatchEvent(new web::Event(base::Tokens::beforeunload()));
  } else if (!on_before_unload_fired_but_not_handled_.is_null()) {
    on_before_unload_fired_but_not_handled_.Run();
  }
}

void WebModule::Impl::InjectCaptionSettingsChangedEvent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  system_caption_settings_->OnCaptionSettingsChanged();
}

void WebModule::Impl::InjectWindowOnOnlineEvent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  window_->OnWindowOnOnlineEvent();
}

void WebModule::Impl::InjectWindowOnOfflineEvent() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  window_->OnWindowOnOfflineEvent();
}

void WebModule::Impl::PurgeResourceCaches(
    bool should_retain_remote_typeface_cache) {
  image_cache_->Purge();
  if (should_retain_remote_typeface_cache) {
    remote_typeface_cache_->ProcessPendingCallbacks();
  } else {
    remote_typeface_cache_->Purge();
  }
  mesh_cache_->Purge();
}

void WebModule::Impl::DisableCallbacksInResourceCaches() {
  image_cache_->DisableCallbacks();
  remote_typeface_cache_->DisableCallbacks();
  mesh_cache_->DisableCallbacks();
}

void WebModule::Impl::HandlePointerEvents() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::HandlePointerEvents");
  const scoped_refptr<dom::Document>& document = window_->document();
  scoped_refptr<web::Event> event;
  do {
    event = document->pointer_state()->GetNextQueuedPointerEvent();
    if (event) {
      SB_DCHECK(
          window_ ==
          base::polymorphic_downcast<const dom::UIEvent* const>(event.get())
              ->view());
      if (!topmost_event_target_) {
        topmost_event_target_.reset(
            new layout::TopmostEventTarget(scroll_engine_));
      }
      topmost_event_target_->MaybeSendPointerEvents(event);
    }
  } while (event && !layout_manager_->IsRenderTreePending());
}

WebModule::Options::Options()
    : layout_trigger(layout::LayoutManager::kOnDocumentMutation),
      mesh_cache_capacity(configuration::Configuration::GetInstance()
                              ->CobaltMeshCacheSizeInBytes()) {
  web_options.stack_size = cobalt::browser::kWebModuleStackSize;
}

WebModule::WebModule(const std::string& name)
    : ui_nav_root_(new ui_navigation::NavItem(
          ui_navigation::kNativeItemTypeContainer,
          // Currently, events do not need to be processed for the root item.
          base::Closure(), base::Closure(), base::Closure())) {
  web_agent_.reset(new web::Agent(name));
}

WebModule::~WebModule() {
  DCHECK(message_loop());
  DCHECK(web_agent_);
  // This will cancel the timers for tasks, which help the thread exit
  ClearAllIntervalsAndTimeouts();
  web_agent_->WaitUntilDone();
  web_agent_->Stop();
  web_agent_.reset();
}

void WebModule::Run(
    const GURL& initial_url, base::ApplicationState initial_application_state,
    ui_navigation::scroll_engine::ScrollEngine* scroll_engine,
    const OnRenderTreeProducedCallback& render_tree_produced_callback,
    OnErrorCallback error_callback, const CloseCallback& window_close_callback,
    const base::Closure& window_minimize_callback,
    media::CanPlayTypeHandler* can_play_type_handler,
    media::MediaModule* media_module, const ViewportSize& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const Options& options) {
  ConstructionData construction_data(
      initial_url, initial_application_state, scroll_engine,
      render_tree_produced_callback, error_callback, window_close_callback,
      window_minimize_callback, can_play_type_handler, media_module,
      window_dimensions, resource_provider, kDOMMaxElementDepth,
      layout_refresh_rate, ui_nav_root_,
#if defined(ENABLE_DEBUGGER)
      &waiting_for_web_debugger_,
#endif  // defined(ENABLE_DEBUGGER)
      &synchronous_loader_interrupt_, options);

  web::Agent::Options web_options(options.web_options);
  if (!options.injected_global_object_attributes.empty()) {
    for (Options::InjectedGlobalObjectAttributes::const_iterator iter =
             options.injected_global_object_attributes.begin();
         iter != options.injected_global_object_attributes.end(); ++iter) {
      DCHECK(!ContainsKey(web_options.injected_global_object_attributes,
                          iter->first));
      // Trampoline to the given callback, adding a pointer to this WebModule.
      web_options.injected_global_object_attributes[iter->first] =
          base::Bind(iter->second, base::Unretained(this));
    }
  }

  web_agent_->Run(web_options,
                  base::Bind(&WebModule::InitializeTaskInThread,
                             base::Unretained(this), construction_data),
                  this);
}

void WebModule::InitializeTaskInThread(const ConstructionData& data,
                                       web::Context* context) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  impl_.reset(new Impl(context, data));
}

// These are helper macros for jumping to the Impl thread without referencing
// impl_ outside of the thread.

// Ensure that we are on the WebModule thread where we can dereference impl_.
// Post a task to the given function if we are not.
#define TASK_TO_ENSURE_IMPL_ON_THREAD0(task_function, function)               \
  DCHECK(message_loop());                                                     \
  if (base::MessageLoop::current() != message_loop()) {                       \
    message_loop()->task_runner()->task_function(                             \
        FROM_HERE, base::Bind(&WebModule::function, base::Unretained(this))); \
    return;                                                                   \
  } else {                                                                    \
    DCHECK(impl_);                                                            \
  }

// Ensure that we are on the WebModule thread where we can dereference impl_.
// Post a task to the given function if we are not.
#define TASK_TO_ENSURE_IMPL_ON_THREAD(task_function, function, ...)         \
  DCHECK(message_loop());                                                   \
  if (base::MessageLoop::current() != message_loop()) {                     \
    message_loop()->task_runner()->task_function(                           \
        FROM_HERE, base::Bind(&WebModule::function, base::Unretained(this), \
                              ##__VA_ARGS__));                              \
    return;                                                                 \
  } else {                                                                  \
    DCHECK(impl_);                                                          \
  }

// Ensure that we are on the WebModule thread where we can dereference impl_.
// Post a task to the given function if we are not.
#define POST_TO_ENSURE_IMPL_ON_THREAD0(function) \
  TASK_TO_ENSURE_IMPL_ON_THREAD0(PostTask, function)

// Ensure that we are on the WebModule thread where we can dereference impl_.
// Post a task to the given function if we are not.
#define POST_TO_ENSURE_IMPL_ON_THREAD(function, ...) \
  TASK_TO_ENSURE_IMPL_ON_THREAD(PostTask, function, ##__VA_ARGS__)

// Ensure that we are on the WebModule thread where we can dereference impl_.
// Post a blocking task to the given function if we are not.
#define POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD0(function) \
  TASK_TO_ENSURE_IMPL_ON_THREAD0(PostBlockingTask, function)

// Ensure that we are on the WebModule thread where we can dereference impl_.
// Post a blocking task to the given function if we are not.
#define POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(function, ...) \
  TASK_TO_ENSURE_IMPL_ON_THREAD(PostBlockingTask, function, ##__VA_ARGS__)

void WebModule::InjectOnScreenKeyboardInputEvent(
    base_token::Token type, const dom::InputEventInit& event) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardInputEvent()", "type",
               TRACE_STR_COPY(type.c_str()));
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectOnScreenKeyboardInputEvent, type, event);
  impl_->InjectOnScreenKeyboardInputEvent(type, event);
}

void WebModule::InjectOnScreenKeyboardShownEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardShownEvent()", "ticket",
               ticket);
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectOnScreenKeyboardShownEvent, ticket);
  impl_->InjectOnScreenKeyboardShownEvent(ticket);
}

void WebModule::InjectOnScreenKeyboardHiddenEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardHiddenEvent()", "ticket",
               ticket);
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectOnScreenKeyboardHiddenEvent, ticket);
  impl_->InjectOnScreenKeyboardHiddenEvent(ticket);
}

void WebModule::InjectOnScreenKeyboardFocusedEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardFocusedEvent()", "ticket",
               ticket);
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectOnScreenKeyboardFocusedEvent, ticket);
  impl_->InjectOnScreenKeyboardFocusedEvent(ticket);
}

void WebModule::InjectOnScreenKeyboardBlurredEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardBlurredEvent()", "ticket",
               ticket);
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectOnScreenKeyboardBlurredEvent, ticket);
  impl_->InjectOnScreenKeyboardBlurredEvent(ticket);
}

void WebModule::InjectOnScreenKeyboardSuggestionsUpdatedEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardSuggestionsUpdatedEvent()",
               "ticket", ticket);
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectOnScreenKeyboardSuggestionsUpdatedEvent,
                                ticket);
  impl_->InjectOnScreenKeyboardSuggestionsUpdatedEvent(ticket);
}


void WebModule::InjectKeyboardEvent(base_token::Token type,
                                    const dom::KeyboardEventInit& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectKeyboardEvent()", "type",
               TRACE_STR_COPY(type.c_str()));
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectKeyboardEvent, type, event);
  impl_->InjectKeyboardEvent(type, event);
}

void WebModule::InjectPointerEvent(base_token::Token type,
                                   const dom::PointerEventInit& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectPointerEvent()", "type",
               TRACE_STR_COPY(type.c_str()));
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectPointerEvent, type, event);
  impl_->InjectPointerEvent(type, event);
}

void WebModule::InjectWheelEvent(base_token::Token type,
                                 const dom::WheelEventInit& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectWheelEvent()", "type",
               TRACE_STR_COPY(type.c_str()));
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectWheelEvent, type, event);
  impl_->InjectWheelEvent(type, event);
}

void WebModule::InjectBeforeUnloadEvent() {
  TRACE_EVENT0("cobalt::browser", "WebModule::InjectBeforeUnloadEvent()");
  POST_TO_ENSURE_IMPL_ON_THREAD0(InjectBeforeUnloadEvent);
  impl_->InjectBeforeUnloadEvent();
}

void WebModule::InjectCaptionSettingsChangedEvent() {
  TRACE_EVENT0("cobalt::browser",
               "WebModule::InjectCaptionSettingsChangedEvent()");
  POST_TO_ENSURE_IMPL_ON_THREAD0(InjectCaptionSettingsChangedEvent);
  impl_->InjectCaptionSettingsChangedEvent();
}

void WebModule::InjectWindowOnOnlineEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser", "WebModule::InjectWindowOnOnlineEvent()");
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectWindowOnOnlineEvent, event);
  impl_->InjectWindowOnOnlineEvent();
}

void WebModule::InjectWindowOnOfflineEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser", "WebModule::InjectWindowOnOfflineEvent()");
  POST_TO_ENSURE_IMPL_ON_THREAD(InjectWindowOnOfflineEvent, event);
  impl_->InjectWindowOnOfflineEvent();
}

void WebModule::UpdateDateTimeConfiguration() {
  TRACE_EVENT0("cobalt::browser", "WebModule::UpdateDateTimeConfiguration()");
  POST_TO_ENSURE_IMPL_ON_THREAD0(UpdateDateTimeConfiguration);
  impl_->UpdateDateTimeConfiguration();
}

void WebModule::ExecuteJavascript(const std::string& script_utf8,
                                  const base::SourceLocation& script_location,
                                  std::string* out_result,
                                  bool* out_succeeded) {
  TRACE_EVENT0("cobalt::browser", "WebModule::ExecuteJavascript()");
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(ExecuteJavascript, script_utf8,
                                          script_location, out_result,
                                          out_succeeded);
  impl_->ExecuteJavascript(script_utf8, script_location, out_result,
                           out_succeeded);
}

void WebModule::ClearAllIntervalsAndTimeouts() {
  TRACE_EVENT0("cobalt::browser", "WebModule::ClearAllIntervalsAndTimeouts()");
  POST_TO_ENSURE_IMPL_ON_THREAD0(ClearAllIntervalsAndTimeouts);
  impl_->ClearAllIntervalsAndTimeouts();
}

#if defined(ENABLE_WEBDRIVER)
void WebModule::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id,
    std::unique_ptr<webdriver::WindowDriver>* window_driver_out) {
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(CreateWindowDriver, window_id,
                                          window_driver_out);
  impl_->CreateWindowDriver(window_id, window_driver_out);
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUGGER)
// May be called from any thread.
void WebModule::GetDebugDispatcher(
    debug::backend::DebugDispatcher** dispatcher) {
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(GetDebugDispatcher, dispatcher);
  if (dispatcher) *dispatcher = impl_->debug_dispatcher();
}

void WebModule::FreezeDebugger(
    std::unique_ptr<debug::backend::DebuggerState>* debugger_state) {
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(FreezeDebugger, debugger_state);
  impl_->FreezeDebugger(debugger_state);
}
#endif  // defined(ENABLE_DEBUGGER)

void WebModule::SetSize(const ViewportSize& viewport_size) {
  POST_TO_ENSURE_IMPL_ON_THREAD(SetSize, viewport_size);
  impl_->SetSize(viewport_size);
}

void WebModule::UpdateCamera3D(
    const scoped_refptr<input::Camera3D>& camera_3d) {
  POST_TO_ENSURE_IMPL_ON_THREAD(UpdateCamera3D, camera_3d);
  impl_->UpdateCamera3D(camera_3d);
}

void WebModule::SetMediaModule(media::MediaModule* media_module) {
  POST_TO_ENSURE_IMPL_ON_THREAD(SetMediaModule, media_module);
  impl_->SetMediaModule(media_module);
}

void WebModule::SetImageCacheCapacity(int64_t bytes) {
  POST_TO_ENSURE_IMPL_ON_THREAD(SetImageCacheCapacity, bytes);
  impl_->SetImageCacheCapacity(bytes);
}

void WebModule::SetRemoteTypefaceCacheCapacity(int64_t bytes) {
  POST_TO_ENSURE_IMPL_ON_THREAD(SetRemoteTypefaceCacheCapacity, bytes);
  impl_->SetRemoteTypefaceCacheCapacity(bytes);
}

void WebModule::Blur(SbTimeMonotonic timestamp) {
  synchronous_loader_interrupt_.Signal();
#if defined(ENABLE_DEBUGGER)
  // We normally need to block here so that the call doesn't return until the
  // web application has had a chance to process the whole event. However, our
  // message loop is blocked while waiting for the web debugger to connect, so
  // we would deadlock here if the user switches to Chrome to run devtools on
  // the same machine where Cobalt is running. Therefore, while we're still
  // waiting for the debugger we post the pause task without blocking on it,
  // letting it eventually run when the debugger connects and the message loop
  // is unblocked again.
  if (waiting_for_web_debugger_.load()) {
    POST_TO_ENSURE_IMPL_ON_THREAD(Blur, timestamp);
    impl_->Blur(timestamp);
    return;
  }
#endif  // defined(ENABLE_DEBUGGER)

  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(Blur, timestamp);
  impl_->Blur(timestamp);
}

void WebModule::Conceal(render_tree::ResourceProvider* resource_provider,
                        SbTimeMonotonic timestamp) {
  synchronous_loader_interrupt_.Signal();
  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(Conceal, resource_provider,
                                          timestamp);
  impl_->Conceal(resource_provider, timestamp);
}

void WebModule::Freeze(SbTimeMonotonic timestamp) {
  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(Freeze, timestamp);
  impl_->Freeze(timestamp);
}

void WebModule::Unfreeze(render_tree::ResourceProvider* resource_provider,
                         SbTimeMonotonic timestamp) {
  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(Unfreeze, resource_provider,
                                          timestamp);
  impl_->Unfreeze(resource_provider, timestamp);
}

void WebModule::Reveal(render_tree::ResourceProvider* resource_provider,
                       SbTimeMonotonic timestamp) {
  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(Reveal, resource_provider, timestamp);
  impl_->Reveal(resource_provider, timestamp);
}

void WebModule::Focus(SbTimeMonotonic timestamp) {
  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(Focus, timestamp);
  impl_->Focus(timestamp);
}

void WebModule::ReduceMemory() {
  synchronous_loader_interrupt_.Signal();
  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD0(ReduceMemory);
  impl_->ReduceMemory();
}

void WebModule::RequestJavaScriptHeapStatistics(
    const web::Agent::JavaScriptHeapStatisticsCallback& callback) {
  web_agent_->RequestJavaScriptHeapStatistics(callback);
}

void WebModule::GetIsReadyToFreeze(volatile bool* is_ready_to_freeze) {
  // We must block here so that the call doesn't return until the thread
  // has had a chance to fill in the return value.
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(GetIsReadyToFreeze,
                                          is_ready_to_freeze);
  impl_->IsReadyToFreeze(is_ready_to_freeze);
}

bool WebModule::IsReadyToFreeze() {
  volatile bool is_ready_to_freeze = false;
  GetIsReadyToFreeze(&is_ready_to_freeze);
  return is_ready_to_freeze;
}

void WebModule::DoSynchronousLayoutAndGetRenderTree(
    scoped_refptr<render_tree::Node>* render_tree) {
  TRACE_EVENT0("cobalt::browser",
               "WebModule::DoSynchronousLayoutAndGetRenderTree()");
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(DoSynchronousLayoutAndGetRenderTree,
                                          render_tree);
  impl_->DoSynchronousLayoutAndGetRenderTree(render_tree);
}

void WebModule::SetApplicationStartOrPreloadTimestamp(
    bool is_preload, SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser",
               "WebModule::SetApplicationStartOrPreloadTimestamp()");
  POST_TO_ENSURE_IMPL_ON_THREAD(SetApplicationStartOrPreloadTimestamp,
                                is_preload, timestamp);
  impl_->SetApplicationStartOrPreloadTimestamp(is_preload, timestamp);
}

void WebModule::SetDeepLinkTimestamp(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "WebModule::SetDeepLinkTimestamp()");
  POST_AND_BLOCK_TO_ENSURE_IMPL_ON_THREAD(SetDeepLinkTimestamp, timestamp);
  impl_->SetDeepLinkTimestamp(timestamp);
}

void WebModule::SetUnloadEventTimingInfo(base::TimeTicks start_time,
                                         base::TimeTicks end_time) {
  TRACE_EVENT0("cobalt::browser", "WebModule::SetUnloadEventTimingInfo()");
  POST_TO_ENSURE_IMPL_ON_THREAD(SetUnloadEventTimingInfo, start_time, end_time);
  impl_->SetUnloadEventTimingInfo(start_time, end_time);
}

}  // namespace browser
}  // namespace cobalt
