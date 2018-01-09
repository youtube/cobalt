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

#include "cobalt/browser/web_module.h"

#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/stringprintf.h"
#include "cobalt/base/language.h"
#include "cobalt/base/startup_timer.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/type_id.h"
#include "cobalt/browser/splash_screen_cache.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/browser/web_module_stat_tracker.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/debug/debug_server_module.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/input_event.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/pointer_event.h"
#include "cobalt/dom/storage.h"
#include "cobalt/dom/ui_event.h"
#include "cobalt/dom/url.h"
#include "cobalt/dom/wheel_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/layout/topmost_event_target.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/page_visibility/visibility_state.h"
#include "cobalt/script/error_report.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/storage/storage_manager.h"
#include "starboard/accessibility.h"
#include "starboard/log.h"

namespace cobalt {
namespace browser {

namespace {

// The maximum number of element depth in the DOM tree. Elements at a level
// deeper than this could be discarded, and will not be rendered.
const int kDOMMaxElementDepth = 32;

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
// Help string for the 'partial_layout' command.
const char kPartialLayoutCommandShortHelp[] =
    "Controls partial layout: on | off | wipe | wipe,off";
const char kPartialLayoutCommandLongHelp[] =
    "Controls partial layout.\n"
    "\n"
    "Syntax:\n"
    "  debug.partial_layout('CMD [, CMD ...]')\n"
    "\n"
    "Where CMD can be:\n"
    "  on   : turn partial layout on.\n"
    "  off  : turn partial layout off.\n"
    "  wipe : wipe the box tree.\n"
    "\n"
    "Example:\n"
    "  debug.partial_layout('off,wipe')\n"
    "\n"
    "To wipe the box tree and turn partial layout off.";
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

bool CacheUrlContent(SplashScreenCache* splash_screen_cache, const GURL& url,
                     const std::string& content) {
  base::optional<std::string> key = SplashScreenCache::GetKeyForStartUrl(url);
  if (key) {
    return splash_screen_cache->SplashScreenCache::CacheSplashScreen(*key,
                                                                     content);
  }
  return false;
}

base::Callback<bool(const GURL&, const std::string&)> CacheUrlContentCallback(
    SplashScreenCache* splash_screen_cache) {
  // This callback takes in first the url, then the content string.
  if (splash_screen_cache) {
    return base::Bind(CacheUrlContent, base::Unretained(splash_screen_cache));
  } else {
    return base::Callback<bool(const GURL&, const std::string&)>();
  }
}

}  // namespace

// Private WebModule implementation. Each WebModule owns a single instance of
// this class, which performs all the actual work. All functions of this class
// must be called on the message loop of the WebModule thread, so they
// execute synchronously with respect to one another.
class WebModule::Impl {
 public:
  explicit Impl(const ConstructionData& data);
  ~Impl();

#if defined(ENABLE_DEBUG_CONSOLE)
  debug::DebugServer* debug_server() const {
    return debug_server_module_->debug_server();
  }
#endif  // ENABLE_DEBUG_CONSOLE

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // Injects an on screen keyboard input event into the web module. Event is
  // directed at a specific element if the element is non-null. Otherwise, the
  // currently focused element receives the event. If element is specified, we
  // must be on the WebModule's message loop.
  void InjectOnScreenKeyboardInputEvent(scoped_refptr<dom::Element> element,
                                        base::Token type,
                                        const dom::InputEventInit& event);
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

#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

  // Injects a keyboard event into the web module. Event is directed at a
  // specific element if the element is non-null. Otherwise, the currently
  // focused element receives the event. If element is specified, we must be
  // on the WebModule's message loop
  void InjectKeyboardEvent(scoped_refptr<dom::Element> element,
                           base::Token type,
                           const dom::KeyboardEventInit& event);

  // Injects a pointer event into the web module. Event is directed at a
  // specific element if the element is non-null. Otherwise, the currently
  // focused element receives the event. If element is specified, we must be
  // on the WebModule's message loop
  void InjectPointerEvent(scoped_refptr<dom::Element> element, base::Token type,
                          const dom::PointerEventInit& event);

  // Injects a wheel event into the web module. Event is directed at a
  // specific element if the element is non-null. Otherwise, the currently
  // focused element receives the event. If element is specified, we must be
  // on the WebModule's message loop
  void InjectWheelEvent(scoped_refptr<dom::Element> element, base::Token type,
                        const dom::WheelEventInit& event);

  // Injects a beforeunload event into the web module. If this event is not
  // handled by the web application, |on_before_unload_fired_but_not_handled_|
  // will be called. The event is not directed at a specific element.
  void InjectBeforeUnloadEvent();

  // Executes JavaScript in this WebModule. Sets the |result| output parameter
  // and signals |got_result|.
  void ExecuteJavascript(const std::string& script_utf8,
                         const base::SourceLocation& script_location,
                         base::WaitableEvent* got_result, std::string* result,
                         bool* out_succeeded);

  // Clears disables timer related objects
  // so that the message loop can easily exit
  void ClearAllIntervalsAndTimeouts();

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  void OnPartialLayoutConsoleCommandReceived(const std::string& message);
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

#if defined(ENABLE_WEBDRIVER)
  // Creates a new webdriver::WindowDriver that interacts with the Window that
  // is owned by this WebModule instance.
  void CreateWindowDriver(
      const webdriver::protocol::WindowId& window_id,
      scoped_ptr<webdriver::WindowDriver>* window_driver_out);
#endif

#if defined(ENABLE_DEBUG_CONSOLE)
  void CreateDebugServerIfNull();
#endif  // ENABLE_DEBUG_CONSOLE

  void SetSize(math::Size window_dimensions, float video_pixel_ratio);
  void SetCamera3D(const scoped_refptr<input::Camera3D>& camera_3d);
  void SetWebMediaPlayerFactory(
      media::WebMediaPlayerFactory* web_media_player_factory);
  void SetImageCacheCapacity(int64_t bytes);
  void SetRemoteTypefaceCacheCapacity(int64_t bytes);
  void SetJavascriptGcThreshold(int64_t bytes);

  // Sets the application state, asserts preconditions to transition to that
  // state, and dispatches any precipitate web events.
  void SetApplicationState(base::ApplicationState state);

  // Suspension of the WebModule is a two-part process since a message loop
  // gap is needed in order to give a chance to handle loader callbacks
  // that were initiated from a loader thread.
  //
  // If |update_application_state| is false, then SetApplicationState will not
  // be called, and no state transition events will be generated.
  void SuspendLoaders(bool update_application_state);
  void FinishSuspend();

  // See LifecycleObserver. These functions do not implement the interface, but
  // have the same basic function.
  void Start(render_tree::ResourceProvider* resource_provider);
  void Pause();
  void Unpause();
  void Resume(render_tree::ResourceProvider* resource_provider);

  void ReduceMemory();

  void LogScriptError(const base::SourceLocation& source_location,
                      const std::string& error_message);

 private:
  class DocumentLoadedObserver;

  // Purge all resource caches owned by the WebModule.
  void PurgeResourceCaches(bool should_retain_remote_typeface_cache);

  // Disable callbacks in all resource caches owned by the WebModule.
  void DisableCallbacksInResourceCaches();

  // Injects a list of custom window attributes into the WebModule's window
  // object.
  void InjectCustomWindowAttributes(
      const Options::InjectedWindowAttributes& attributes);

  // Called by |layout_mananger_| after it runs the animation frame callbacks.
  void OnRanAnimationFrameCallbacks();

  // Called by |layout_mananger_| when it produces a render tree. May modify
  // the render tree (e.g. to add a debug overlay), then runs the callback
  // specified in the constructor, |render_tree_produced_callback_|.
  void OnRenderTreeProduced(const LayoutResults& layout_results);

  // Called by the Renderer on the Renderer thread when it rasterizes a render
  // tree with this callback attached.
  void OnRenderTreeRasterized(
      scoped_refptr<base::MessageLoopProxy> web_module_message_loop);

  // WebModule thread handling of the OnRenderTreeRasterized() callback. It
  // includes the time that the rasterization callback was initially received.
  void ProcessOnRenderTreeRasterized(const base::TimeTicks& on_rasterize_time);

  void OnCspPolicyChanged();

  scoped_refptr<script::GlobalEnvironment> global_environment() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return global_environment_;
  }

  void OnError(const std::string& error) {
    error_callback_.Run(window_->location()->url(), error);
  }

  // Report an error encountered while running JS.
  // Returns whether or not the error was handled.
  bool ReportScriptError(const script::ErrorReport& error_report);

  // Inject the DOM event object into the window or the element.
  void InjectInputEvent(scoped_refptr<dom::Element> element,
                        const scoped_refptr<dom::Event>& event);

  // Handle queued pointer events. Called by LayoutManager on_layout callback.
  void HandlePointerEvents();

  // Initializes the ResourceProvider and dependent resources.
  void SetResourceProvider(render_tree::ResourceProvider* resource_provider);

  // Thread checker ensures all calls to the WebModule are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;

  std::string name_;

  // Simple flag used for basic error checking.
  bool is_running_;

  // Object that provides renderer resources like images and fonts.
  render_tree::ResourceProvider* resource_provider_;
  // The type id of resource provider being used by the WebModule. Whenever this
  // changes, the caches may have obsolete data and must be blown away.
  base::TypeId resource_provider_type_id_;

  // CSS parser.
  scoped_ptr<css_parser::Parser> css_parser_;

  // DOM (HTML / XML) parser.
  scoped_ptr<dom_parser::Parser> dom_parser_;

  // FetcherFactory that is used to create a fetcher according to URL.
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;

  // LoaderFactory that is used to acquire references to resources from a
  // URL.
  scoped_ptr<loader::LoaderFactory> loader_factory_;

  scoped_ptr<loader::image::AnimatedImageTracker> animated_image_tracker_;

  // ImageCache that is used to manage image cache logic.
  scoped_ptr<loader::image::ImageCache> image_cache_;

  // The reduced cache capacity manager can be used to force a reduced image
  // cache over periods of time where memory is known to be restricted, such
  // as when a video is playing.
  scoped_ptr<loader::image::ReducedCacheCapacityManager>
      reduced_image_cache_capacity_manager_;

  // RemoteTypefaceCache that is used to manage loading and caching typefaces
  // from URLs.
  scoped_ptr<loader::font::RemoteTypefaceCache> remote_typeface_cache_;

  // MeshCache that is used to manage mesh cache logic.
  scoped_ptr<loader::mesh::MeshCache> mesh_cache_;

  // Interface between LocalStorage and the Storage Manager.
  scoped_ptr<dom::LocalStorageDatabase> local_storage_database_;

  // Stats for the web module. Both the dom stat tracker and layout stat
  // tracker are contained within it.
  scoped_ptr<browser::WebModuleStatTracker> web_module_stat_tracker_;

  // Post and run tasks to notify MutationObservers.
  dom::MutationObserverTaskManager mutation_observer_task_manager_;

  // JavaScript engine for the browser.
  scoped_ptr<script::JavaScriptEngine> javascript_engine_;

  // JavaScript Global Object for the browser. There should be one per window,
  // but since there is only one window, we can have one per browser.
  scoped_refptr<script::GlobalEnvironment> global_environment_;

  // Used by |Console| to obtain a JavaScript stack trace.
  scoped_ptr<script::ExecutionState> execution_state_;

  // Interface for the document to execute JavaScript code.
  scoped_ptr<script::ScriptRunner> script_runner_;

  // Object to register and retrieve MediaSource object with a string key.
  scoped_ptr<dom::MediaSource::Registry> media_source_registry_;

  // Object to register and retrieve Blob objects with a string key.
  scoped_ptr<dom::Blob::Registry> blob_registry_;

  // The Window object wraps all DOM-related components.
  scoped_refptr<dom::Window> window_;

  // Cache a WeakPtr in the WebModule that is bound to the Window's message loop
  // so we can ensure that all subsequently created WeakPtr's are also bound to
  // the same loop.
  // See the documentation in base/memory/weak_ptr.h for details.
  base::WeakPtr<dom::Window> window_weak_;

  // Environment Settings object
  scoped_ptr<dom::DOMSettings> environment_settings_;

  // Called by |OnRenderTreeProduced|.
  OnRenderTreeProducedCallback render_tree_produced_callback_;

  // Called by |OnError|.
  OnErrorCallback error_callback_;

  // Triggers layout whenever the document changes.
  scoped_ptr<layout::LayoutManager> layout_manager_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Allows the debugger to add render components to the web module.
  // Used for DOM node highlighting and overlay messages.
  scoped_ptr<debug::RenderOverlay> debug_overlay_;

  // The core of the debugging system.
  // Created lazily when accessed via |GetDebugServer|.
  scoped_ptr<debug::DebugServerModule> debug_server_module_;
#endif  // ENABLE_DEBUG_CONSOLE

  // DocumentObserver that observes the loading document.
  scoped_ptr<DocumentLoadedObserver> document_load_observer_;

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  // Handles the 'partial_layout' command.
  scoped_ptr<base::ConsoleCommandManager::CommandHandler>
      partial_layout_command_handler_;
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  scoped_ptr<media_session::MediaSessionClient> media_session_client_;

  scoped_ptr<layout::TopmostEventTarget> topmost_event_target_;

  base::Closure on_before_unload_fired_but_not_handled_;

  bool should_retain_remote_typeface_cache_on_suspend_;
};

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

  void OnMutation() override{};
  void OnFocusChanged() override{};

 private:
  ClosureVector loaded_callbacks_;
};

WebModule::Impl::Impl(const ConstructionData& data)
    : name_(data.options.name),
      is_running_(false),
      resource_provider_(data.resource_provider),
      resource_provider_type_id_(data.resource_provider->GetTypeId()) {
  // Currently we rely on a platform to explicitly specify that it supports
  // the map-to-mesh filter via the ENABLE_MAP_TO_MESH define (and the
  // 'enable_map_to_mesh' gyp variable).  When we have better support for
  // checking for decode to texture support, it would be nice to switch this
  // logic to something like:
  //
  //   supports_map_to_mesh =
  //      (resource_provider_->Supports3D() && SbPlayerSupportsDecodeToTexture()
  //           ? css_parser::Parser::kSupportsMapToMesh
  //           : css_parser::Parser::kDoesNotSupportMapToMesh);
  //
  // Note that it is important that we do not parse map-to-mesh filters if we
  // cannot render them, since web apps may check for map-to-mesh support by
  // testing whether it parses or not via the CSS.supports() Web API.
  css_parser::Parser::SupportsMapToMeshFlag supports_map_to_mesh =
#if defined(ENABLE_MAP_TO_MESH)
      css_parser::Parser::kSupportsMapToMesh;
#else
      css_parser::Parser::kDoesNotSupportMapToMesh;
#endif

  css_parser_ = css_parser::Parser::Create(supports_map_to_mesh);
  DCHECK(css_parser_);

  dom_parser_.reset(new dom_parser::Parser(
      kDOMMaxElementDepth,
      base::Bind(&WebModule::Impl::OnError, base::Unretained(this)),
      data.options.require_csp));
  DCHECK(dom_parser_);

  blob_registry_.reset(new dom::Blob::Registry);

  base::Callback<int(const std::string&, scoped_array<char>*)>
      read_cache_callback;
  if (data.options.can_fetch_cache) {
    read_cache_callback =
        base::Bind(&browser::SplashScreenCache::ReadCachedSplashScreen,
                   base::Unretained(data.options.splash_screen_cache));
  }

  on_before_unload_fired_but_not_handled_ =
      data.options.on_before_unload_fired_but_not_handled;

  should_retain_remote_typeface_cache_on_suspend_ =
      data.options.should_retain_remote_typeface_cache_on_suspend;

  fetcher_factory_.reset(new loader::FetcherFactory(
      data.network_module, data.options.extra_web_file_dir,
      dom::URL::MakeBlobResolverCallback(blob_registry_.get()),
      read_cache_callback));
  DCHECK(fetcher_factory_);

  loader_factory_.reset(
      new loader::LoaderFactory(fetcher_factory_.get(), resource_provider_,
                                data.options.loader_thread_priority));

  animated_image_tracker_.reset(new loader::image::AnimatedImageTracker(
      data.options.animated_image_decode_thread_priority));

  DCHECK_LE(0, data.options.image_cache_capacity);
  image_cache_ = loader::image::CreateImageCache(
      base::StringPrintf("%s.ImageCache", name_.c_str()),
      static_cast<uint32>(data.options.image_cache_capacity),
      loader_factory_.get());
  DCHECK(image_cache_);

  reduced_image_cache_capacity_manager_.reset(
      new loader::image::ReducedCacheCapacityManager(
          image_cache_.get(),
          data.options.image_cache_capacity_multiplier_when_playing_video));

  DCHECK_LE(0, data.options.remote_typeface_cache_capacity);
  remote_typeface_cache_ = loader::font::CreateRemoteTypefaceCache(
      base::StringPrintf("%s.RemoteTypefaceCache", name_.c_str()),
      static_cast<uint32>(data.options.remote_typeface_cache_capacity),
      loader_factory_.get());
  DCHECK(remote_typeface_cache_);

  DCHECK_LE(0, data.options.mesh_cache_capacity);
  mesh_cache_ = loader::mesh::CreateMeshCache(
      base::StringPrintf("%s.MeshCache", name_.c_str()),
      static_cast<uint32>(data.options.mesh_cache_capacity),
      loader_factory_.get());
  DCHECK(mesh_cache_);

  local_storage_database_.reset(
      new dom::LocalStorageDatabase(data.network_module->storage_manager()));
  DCHECK(local_storage_database_);

  web_module_stat_tracker_.reset(
      new browser::WebModuleStatTracker(name_, data.options.track_event_stats));
  DCHECK(web_module_stat_tracker_);

  javascript_engine_ = script::JavaScriptEngine::CreateEngine(
      data.options.javascript_engine_options);
  DCHECK(javascript_engine_);

#if defined(COBALT_ENABLE_JAVASCRIPT_ERROR_LOGGING)
  script::JavaScriptEngine::ErrorHandler error_handler =
      base::Bind(&WebModule::Impl::LogScriptError, base::Unretained(this));
  javascript_engine_->RegisterErrorHandler(error_handler);
#endif

  global_environment_ = javascript_engine_->CreateGlobalEnvironment();
  DCHECK(global_environment_);

  execution_state_ =
      script::ExecutionState::CreateExecutionState(global_environment_);
  DCHECK(execution_state_);

  script_runner_ =
      script::ScriptRunner::CreateScriptRunner(global_environment_);
  DCHECK(script_runner_);

  media_source_registry_.reset(new dom::MediaSource::Registry);

  media_session_client_ = media_session::MediaSessionClient::Create();

  dom::Window::CacheCallback splash_screen_cache_callback =
      CacheUrlContentCallback(data.options.splash_screen_cache);

  window_ = new dom::Window(
      data.window_dimensions.width(), data.window_dimensions.height(),
      data.video_pixel_ratio, data.initial_application_state, css_parser_.get(),
      dom_parser_.get(), fetcher_factory_.get(), &resource_provider_,
      animated_image_tracker_.get(), image_cache_.get(),
      reduced_image_cache_capacity_manager_.get(), remote_typeface_cache_.get(),
      mesh_cache_.get(), local_storage_database_.get(),
      data.can_play_type_handler, data.web_media_player_factory,
      execution_state_.get(), script_runner_.get(),
      global_environment_->script_value_factory(), media_source_registry_.get(),
      web_module_stat_tracker_->dom_stat_tracker(), data.initial_url,
      data.network_module->GetUserAgent(),
      data.network_module->preferred_language(),
      data.options.font_language_script_override.empty()
          ? base::GetSystemLanguageScript()
          : data.options.font_language_script_override,
      data.options.navigation_callback,
      base::Bind(&WebModule::Impl::OnError, base::Unretained(this)),
      data.network_module->cookie_jar(), data.network_module->GetPostSender(),
      data.options.require_csp, data.options.csp_enforcement_mode,
      base::Bind(&WebModule::Impl::OnCspPolicyChanged, base::Unretained(this)),
      base::Bind(&WebModule::Impl::OnRanAnimationFrameCallbacks,
                 base::Unretained(this)),
      data.window_close_callback, data.window_minimize_callback,
      data.get_sb_window_callback, data.options.camera_3d,
      media_session_client_->GetMediaSession(),
      data.options.csp_insecure_allowed_token, data.dom_max_element_depth,
      data.options.video_playback_rate_multiplier,
#if defined(ENABLE_TEST_RUNNER)
      data.options.layout_trigger == layout::LayoutManager::kTestRunnerMode
          ? dom::Window::kClockTypeTestRunner
          : dom::Window::kClockTypeSystemTime,
#else
      dom::Window::kClockTypeSystemTime,
#endif
      splash_screen_cache_callback);
  DCHECK(window_);

  window_weak_ = base::AsWeakPtr(window_.get());
  DCHECK(window_weak_);

  environment_settings_.reset(new dom::DOMSettings(
      kDOMMaxElementDepth, fetcher_factory_.get(), data.network_module, window_,
      media_source_registry_.get(), blob_registry_.get(),
      data.can_play_type_handler, javascript_engine_.get(),
      global_environment_.get(), &mutation_observer_task_manager_,
      data.options.dom_settings_options));
  DCHECK(environment_settings_);

  global_environment_->CreateGlobalObject(window_, environment_settings_.get());

  render_tree_produced_callback_ = data.render_tree_produced_callback;
  DCHECK(!render_tree_produced_callback_.is_null());

  error_callback_ = data.error_callback;
  DCHECK(!error_callback_.is_null());

  layout_manager_.reset(new layout::LayoutManager(
      name_, window_.get(),
      base::Bind(&WebModule::Impl::OnRenderTreeProduced,
                 base::Unretained(this)),
      base::Bind(&WebModule::Impl::HandlePointerEvents, base::Unretained(this)),
      data.options.layout_trigger, data.dom_max_element_depth,
      data.layout_refresh_rate, data.network_module->preferred_language(),
      data.options.enable_image_animations,
      web_module_stat_tracker_->layout_stat_tracker()));
  DCHECK(layout_manager_);

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_overlay_.reset(
      new debug::RenderOverlay(data.render_tree_produced_callback));
#endif  // ENABLE_DEBUG_CONSOLE

#if !defined(COBALT_FORCE_CSP)
  if (data.options.csp_enforcement_mode == dom::kCspEnforcementDisable) {
    // If CSP is disabled, enable eval(). Otherwise, it will be enabled by
    // a CSP directive.
    global_environment_->EnableEval();
  }
#endif

  global_environment_->SetReportEvalCallback(
      base::Bind(&dom::CspDelegate::ReportEval,
                 base::Unretained(window_->document()->csp_delegate())));

  global_environment_->SetReportErrorCallback(
      base::Bind(&WebModule::Impl::ReportScriptError, base::Unretained(this)));

  InjectCustomWindowAttributes(data.options.injected_window_attributes);

  if (!data.options.loaded_callbacks.empty()) {
    document_load_observer_.reset(
        new DocumentLoadedObserver(data.options.loaded_callbacks));
    window_->document()->AddObserver(document_load_observer_.get());
  }

  is_running_ = true;
}

WebModule::Impl::~Impl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  is_running_ = false;
  global_environment_->SetReportEvalCallback(base::Closure());
  global_environment_->SetReportErrorCallback(
      script::GlobalEnvironment::ReportErrorCallback());
  window_->DispatchEvent(new dom::Event(base::Tokens::unload()));
  document_load_observer_.reset();
  media_session_client_.reset();

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_overlay_.reset();
  debug_server_module_.reset();
#endif  // ENABLE_DEBUG_CONSOLE

  // Disable callbacks for the resource caches. Otherwise, it is possible for a
  // callback to occur into a DOM object that is being kept alive by a JS engine
  // reference even after the DOM tree has been destroyed. This can result in a
  // crash when the callback attempts to access a stale Document pointer.
  DisableCallbacksInResourceCaches();

  topmost_event_target_.reset();
  layout_manager_.reset();
  environment_settings_.reset();
  window_weak_.reset();
  window_ = NULL;
  media_source_registry_.reset();
  blob_registry_.reset();
  script_runner_.reset();
  execution_state_.reset();
  global_environment_ = NULL;
  javascript_engine_.reset();
  web_module_stat_tracker_.reset();
  local_storage_database_.reset();
  mesh_cache_.reset();
  remote_typeface_cache_.reset();
  image_cache_.reset();
  animated_image_tracker_.reset();
  fetcher_factory_.reset();
  dom_parser_.reset();
  css_parser_.reset();
}

void WebModule::Impl::InjectInputEvent(scoped_refptr<dom::Element> element,
                                       const scoped_refptr<dom::Event>& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);

  web_module_stat_tracker_->OnStartInjectEvent(event);

  if (element) {
    element->DispatchEvent(event);
  } else {
    window_->InjectEvent(event);
  }

  web_module_stat_tracker_->OnEndInjectEvent(
      window_->HasPendingAnimationFrameCallbacks(),
      layout_manager_->IsRenderTreePending());
}

#if SB_HAS(ON_SCREEN_KEYBOARD)
void WebModule::Impl::InjectOnScreenKeyboardInputEvent(
    scoped_refptr<dom::Element> element, base::Token type,
    const dom::InputEventInit& event) {
  scoped_refptr<dom::InputEvent> input_event(
      new dom::InputEvent(type, window_, event));
  InjectInputEvent(element, input_event);
}

void WebModule::Impl::InjectOnScreenKeyboardShownEvent(int ticket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchShowEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardHiddenEvent(int ticket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchHideEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardFocusedEvent(int ticket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchFocusEvent(ticket);
}

void WebModule::Impl::InjectOnScreenKeyboardBlurredEvent(int ticket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->on_screen_keyboard());

  window_->on_screen_keyboard()->DispatchBlurEvent(ticket);
}

#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

void WebModule::Impl::InjectKeyboardEvent(scoped_refptr<dom::Element> element,
                                          base::Token type,
                                          const dom::KeyboardEventInit& event) {
  scoped_refptr<dom::KeyboardEvent> keyboard_event(
      new dom::KeyboardEvent(type, window_, event));
  InjectInputEvent(element, keyboard_event);
}

void WebModule::Impl::InjectPointerEvent(scoped_refptr<dom::Element> element,
                                         base::Token type,
                                         const dom::PointerEventInit& event) {
  scoped_refptr<dom::PointerEvent> pointer_event(
      new dom::PointerEvent(type, window_, event));
  InjectInputEvent(element, pointer_event);
}

void WebModule::Impl::InjectWheelEvent(scoped_refptr<dom::Element> element,
                                       base::Token type,
                                       const dom::WheelEventInit& event) {
  scoped_refptr<dom::WheelEvent> wheel_event(
      new dom::WheelEvent(type, window_, event));
  InjectInputEvent(element, wheel_event);
}

void WebModule::Impl::ExecuteJavascript(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    base::WaitableEvent* got_result, std::string* result, bool* out_succeeded) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(script_runner_);

  // JavaScript is being run. Track it in the global stats.
  dom::GlobalStats::GetInstance()->StartJavaScriptEvent();

  // This should only be called for Cobalt JavaScript, error reports are
  // allowed.
  *result = script_runner_->Execute(script_utf8, script_location,
                                    false /*mute_errors*/, out_succeeded);

  // JavaScript is done running. Stop tracking it in the global stats.
  dom::GlobalStats::GetInstance()->StopJavaScriptEvent();

  got_result->Signal();
}

void WebModule::Impl::ClearAllIntervalsAndTimeouts() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(window_);
  window_->DestroyTimers();
}

void WebModule::Impl::OnRanAnimationFrameCallbacks() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  // Notify the stat tracker that the animation frame callbacks have finished.
  // This may end the current event being tracked.
  web_module_stat_tracker_->OnRanAnimationFrameCallbacks(
      layout_manager_->IsRenderTreePending());
}

void WebModule::Impl::OnRenderTreeProduced(
    const LayoutResults& layout_results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);

  LayoutResults layout_results_with_callback(layout_results.render_tree,
                                             layout_results.layout_time);

  // Notify the stat tracker that a render tree has been produced.
  web_module_stat_tracker_->OnRenderTreeProduced();

  // If the stat tracker is expecting to be notified of the rasterization time
  // of the next render tree, then set a callback for this layout.
  if (web_module_stat_tracker_->IsRenderTreeRasterizationPending()) {
    layout_results_with_callback.on_rasterized_callback =
        base::Bind(&WebModule::Impl::OnRenderTreeRasterized,
                   base::Unretained(this), base::MessageLoopProxy::current());
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_overlay_->OnRenderTreeProduced(layout_results_with_callback);
#else  // ENABLE_DEBUG_CONSOLE
  render_tree_produced_callback_.Run(layout_results_with_callback);
#endif  // ENABLE_DEBUG_CONSOLE
}

void WebModule::Impl::OnRenderTreeRasterized(
    scoped_refptr<base::MessageLoopProxy> web_module_message_loop) {
  web_module_message_loop->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::ProcessOnRenderTreeRasterized,
                            base::Unretained(this), base::TimeTicks::Now()));
}

void WebModule::Impl::ProcessOnRenderTreeRasterized(
    const base::TimeTicks& on_rasterize_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  web_module_stat_tracker_->OnRenderTreeRasterized(on_rasterize_time);
}

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
void WebModule::Impl::OnPartialLayoutConsoleCommandReceived(
    const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->document());
  window_->document()->SetPartialLayout(message);
}
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

void WebModule::Impl::OnCspPolicyChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_->document());
  DCHECK(window_->document()->csp_delegate());

  std::string eval_disabled_message;
  bool allow_eval =
      window_->document()->csp_delegate()->AllowEval(&eval_disabled_message);
  if (allow_eval) {
    global_environment_->EnableEval();
  } else {
    global_environment_->DisableEval(eval_disabled_message);
  }
}

bool WebModule::Impl::ReportScriptError(
    const script::ErrorReport& error_report) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  return window_->ReportScriptError(error_report);
}

#if defined(ENABLE_WEBDRIVER)
void WebModule::Impl::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id,
    scoped_ptr<webdriver::WindowDriver>* window_driver_out) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(window_weak_);
  DCHECK(window_->document());
  DCHECK(global_environment_);

  window_driver_out->reset(new webdriver::WindowDriver(
      window_id, window_weak_,
      base::Bind(&WebModule::Impl::global_environment, base::Unretained(this)),
      base::Bind(&WebModule::Impl::InjectKeyboardEvent, base::Unretained(this)),
      base::Bind(&WebModule::Impl::InjectPointerEvent, base::Unretained(this)),
      base::MessageLoopProxy::current()));
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUG_CONSOLE)
void WebModule::Impl::CreateDebugServerIfNull() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);
  DCHECK(global_environment_);
  DCHECK(resource_provider_);

  if (debug_server_module_) {
    return;
  }

  debug_server_module_.reset(new debug::DebugServerModule(
      window_->console(), global_environment_, debug_overlay_.get(),
      resource_provider_, window_));
}
#endif  // defined(ENABLE_DEBUG_CONSOLE)

void WebModule::Impl::InjectCustomWindowAttributes(
    const Options::InjectedWindowAttributes& attributes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(global_environment_);

  for (Options::InjectedWindowAttributes::const_iterator iter =
           attributes.begin();
       iter != attributes.end(); ++iter) {
    global_environment_->Bind(
        iter->first, iter->second.Run(window_, &mutation_observer_task_manager_,
                                      global_environment_.get()));
  }
}

void WebModule::Impl::SetImageCacheCapacity(int64_t bytes) {
  image_cache_->SetCapacity(static_cast<uint32>(bytes));
}

void WebModule::Impl::SetRemoteTypefaceCacheCapacity(int64_t bytes) {
  remote_typeface_cache_->SetCapacity(static_cast<uint32>(bytes));
}

void WebModule::Impl::SetJavascriptGcThreshold(int64_t bytes) {
  javascript_engine_->SetGcThreshold(bytes);
}

void WebModule::Impl::SetSize(math::Size window_dimensions,
                              float video_pixel_ratio) {
  window_->SetSize(window_dimensions.width(), window_dimensions.height(),
                   video_pixel_ratio);
}

void WebModule::Impl::SetCamera3D(
    const scoped_refptr<input::Camera3D>& camera_3d) {
  window_->SetCamera3D(camera_3d);
}

void WebModule::Impl::SetWebMediaPlayerFactory(
    media::WebMediaPlayerFactory* web_media_player_factory) {
  window_->set_web_media_player_factory(web_media_player_factory);
}

void WebModule::Impl::SetApplicationState(base::ApplicationState state) {
  window_->SetApplicationState(state);
}

void WebModule::Impl::SetResourceProvider(
    render_tree::ResourceProvider* resource_provider) {
  resource_provider_ = resource_provider;
  if (resource_provider_) {
    base::TypeId resource_provider_type_id = resource_provider_->GetTypeId();
    // Check for if the resource provider type id has changed. If it has, then
    // anything contained within the caches is invalid and must be purged.
    if (resource_provider_type_id_ != resource_provider_type_id) {
      PurgeResourceCaches(false);
    }
    resource_provider_type_id_ = resource_provider_type_id;

    loader_factory_->Resume(resource_provider_);

    // Permit render trees to be generated again.  Layout will have been
    // invalidated with the call to Suspend(), so the layout manager's first
    // task will be to perform a full re-layout.
    layout_manager_->Resume();
  }
}

void WebModule::Impl::Start(render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Start()");
  SetResourceProvider(resource_provider);
  SetApplicationState(base::kApplicationStateStarted);
}

void WebModule::Impl::Pause() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Pause()");
  SetApplicationState(base::kApplicationStatePaused);
}

void WebModule::Impl::Unpause() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Unpause()");
  SetApplicationState(base::kApplicationStateStarted);
}

void WebModule::Impl::SuspendLoaders(bool update_application_state) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::SuspendLoaders()");

  if (update_application_state) {
    SetApplicationState(base::kApplicationStateSuspended);
  }

  // Purge the resource caches before running any suspend logic. This will force
  // any pending callbacks that the caches are batching to run.
  PurgeResourceCaches(should_retain_remote_typeface_cache_on_suspend_);

  // Stop the generation of render trees.
  layout_manager_->Suspend();

  // Purge the cached resources prior to the suspend. That may cancel pending
  // loads, allowing the suspend to occur faster and preventing unnecessary
  // callbacks.
  window_->document()->PurgeCachedResources();

  // Clear out the loader factory's resource provider, possibly aborting any
  // in-progress loads.
  loader_factory_->Suspend();

  // Clear out any currently tracked animating images.
  animated_image_tracker_->Reset();
}

void WebModule::Impl::FinishSuspend() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::FinishSuspend()");
  DCHECK(resource_provider_);

  // Ensure the document is not holding onto any more image cached resources so
  // that they are eligible to be purged.
  window_->document()->PurgeCachedResources();

  // Clear out all resource caches. We need to do this after we abort all
  // in-progress loads, and after we clear all document references, or they will
  // still be referenced and won't be cleared from the cache.
  PurgeResourceCaches(should_retain_remote_typeface_cache_on_suspend_);

#if defined(ENABLE_DEBUG_CONSOLE)
  // The debug overlay may be holding onto a render tree, clear that out.
  debug_overlay_->ClearInput();
#endif

  resource_provider_ = NULL;

  // Force garbage collection in |javascript_engine_|.
  if (javascript_engine_) {
    javascript_engine_->CollectGarbage();
  }
}

void WebModule::Impl::Resume(render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Resume()");
  SetResourceProvider(resource_provider);
  SetApplicationState(base::kApplicationStatePaused);
}

void WebModule::Impl::ReduceMemory() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::ReduceMemory()");
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!is_running_) {
    return;
  }

  layout_manager_->Purge();

  // Retain the remote typeface cache when reducing memory.
  PurgeResourceCaches(true /*should_retain_remote_typeface_cache*/);
  window_->document()->PurgeCachedResources();

  // Force garbage collection in |javascript_engine_|.
  if (javascript_engine_) {
    javascript_engine_->CollectGarbage();
  }
}

void WebModule::Impl::LogScriptError(
    const base::SourceLocation& source_location,
    const std::string& error_message) {
  std::string file_name =
      FilePath(source_location.file_path).BaseName().value();

  std::stringstream ss;
  base::TimeDelta dt = base::StartupTimer::TimeElapsed();

  // Create the error output.
  // Example:
  //   JS:50250:file.js(29,80): ka(...) is not iterable
  //   JS:<time millis><js-file-name>(<line>,<column>):<message>
  ss << "JS:" << dt.InMilliseconds() << ":" << file_name << "("
     << source_location.line_number << "," << source_location.column_number
     << "): " << error_message << "\n";
  SbLogRaw(ss.str().c_str());
}

void WebModule::Impl::InjectBeforeUnloadEvent() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (window_ && window_->HasEventListener(base::Tokens::beforeunload())) {
    window_->DispatchEvent(new dom::Event(base::Tokens::beforeunload()));
  } else if (!on_before_unload_fired_but_not_handled_.is_null()) {
    on_before_unload_fired_but_not_handled_.Run();
  }
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
  scoped_refptr<dom::Event> event;
  do {
    event = document->pointer_state()->GetNextQueuedPointerEvent();
    if (event) {
      SB_DCHECK(
          window_ ==
          base::polymorphic_downcast<const dom::UIEvent* const>(event.get())
              ->view());
      if (!topmost_event_target_) {
        topmost_event_target_.reset(new layout::TopmostEventTarget());
      }
      topmost_event_target_->MaybeSendPointerEvents(event);
    }
  } while (event && !layout_manager_->IsRenderTreePending());
}

WebModule::DestructionObserver::DestructionObserver(WebModule* web_module)
    : web_module_(web_module) {}

void WebModule::DestructionObserver::WillDestroyCurrentMessageLoop() {
  web_module_->impl_.reset();
}

WebModule::Options::Options()
    : name("WebModule"),
      layout_trigger(layout::LayoutManager::kOnDocumentMutation),
      image_cache_capacity(32 * 1024 * 1024),
      remote_typeface_cache_capacity(4 * 1024 * 1024),
      mesh_cache_capacity(COBALT_MESH_CACHE_SIZE_IN_BYTES),
      csp_enforcement_mode(dom::kCspEnforcementEnable),
      csp_insecure_allowed_token(0),
      track_event_stats(false),
      image_cache_capacity_multiplier_when_playing_video(1.0f),
      thread_priority(base::kThreadPriority_Normal),
      loader_thread_priority(base::kThreadPriority_Low),
      animated_image_decode_thread_priority(base::kThreadPriority_Low),
      video_playback_rate_multiplier(1.f),
      enable_image_animations(true),
      should_retain_remote_typeface_cache_on_suspend(false),
      can_fetch_cache(false) {}

WebModule::WebModule(
    const GURL& initial_url, base::ApplicationState initial_application_state,
    const OnRenderTreeProducedCallback& render_tree_produced_callback,
    const OnErrorCallback& error_callback,
    const CloseCallback& window_close_callback,
    const base::Closure& window_minimize_callback,
    const dom::Window::GetSbWindowCallback& get_sb_window_callback,
    media::CanPlayTypeHandler* can_play_type_handler,
    media::WebMediaPlayerFactory* web_media_player_factory,
    network::NetworkModule* network_module, const math::Size& window_dimensions,
    float video_pixel_ratio, render_tree::ResourceProvider* resource_provider,
    float layout_refresh_rate, const Options& options)
    : thread_(options.name.c_str()) {
  ConstructionData construction_data(
      initial_url, initial_application_state, render_tree_produced_callback,
      error_callback, window_close_callback, window_minimize_callback,
      get_sb_window_callback, can_play_type_handler, web_media_player_factory,
      network_module, window_dimensions, video_pixel_ratio, resource_provider,
      kDOMMaxElementDepth, layout_refresh_rate, options);

  // Start the dedicated thread and create the internal implementation
  // object on that thread.
  thread_.StartWithOptions(base::Thread::Options(
      MessageLoop::TYPE_DEFAULT, cobalt::browser::kWebModuleStackSize,
      options.thread_priority));
  DCHECK(message_loop());

  // Block this thread until the initialization is complete.
  // TODO: Figure out why this is necessary.
  // It would be preferable to return immediately and let the WebModule
  // continue in its own time, but without this wait there is a race condition
  // such that inline scripts may be executed before the document elements they
  // operate on are present.
  message_loop()->PostBlockingTask(
      FROM_HERE, base::Bind(&WebModule::Initialize, base::Unretained(this),
                            construction_data));

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kPartialLayout)) {
    const std::string partial_layout_string =
        command_line->GetSwitchValueASCII(browser::switches::kPartialLayout);
    OnPartialLayoutConsoleCommandReceived(partial_layout_string);
  }
  partial_layout_command_handler_.reset(
      new base::ConsoleCommandManager::CommandHandler(
          browser::switches::kPartialLayout,
          base::Bind(&WebModule::OnPartialLayoutConsoleCommandReceived,
                     base::Unretained(this)),
          kPartialLayoutCommandShortHelp, kPartialLayoutCommandLongHelp));
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
}

WebModule::~WebModule() {
  DCHECK(message_loop());

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  partial_layout_command_handler_.reset();
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

  // Create a destruction observer to shut down the WebModule once all pending
  // tasks have been executed and the message loop is about to be destroyed.
  // This allows us to safely stop the thread, drain the task queue, then
  // destroy the internal components before the message loop is set to NULL.
  // No posted tasks will be executed once the thread is stopped.
  DestructionObserver destruction_observer(this);
  message_loop()->PostBlockingTask(
      FROM_HERE, base::Bind(&MessageLoop::AddDestructionObserver,
                            base::Unretained(message_loop()),
                            base::Unretained(&destruction_observer)));

  // This will cancel the timers for tasks, which help the thread exit
  ClearAllIntervalsAndTimeouts();

  // Stop the thread. This will cause the destruction observer to be notified.
  thread_.Stop();
}

void WebModule::Initialize(const ConstructionData& data) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  impl_.reset(new Impl(data));
}

#if SB_HAS(ON_SCREEN_KEYBOARD)

void WebModule::InjectOnScreenKeyboardInputEvent(
    base::Token type, const dom::InputEventInit& event) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardInputEvent()", "type",
               type.c_str());
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::InjectOnScreenKeyboardInputEvent,
                            base::Unretained(impl_.get()),
                            scoped_refptr<dom::Element>(), type, event));
}

void WebModule::InjectOnScreenKeyboardShownEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardShownEvent()", "ticket",
               ticket);
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::InjectOnScreenKeyboardShownEvent,
                            base::Unretained(impl_.get()), ticket));
}

void WebModule::InjectOnScreenKeyboardHiddenEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardHiddenEvent()", "ticket",
               ticket);
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::InjectOnScreenKeyboardHiddenEvent,
                            base::Unretained(impl_.get()), ticket));
}

void WebModule::InjectOnScreenKeyboardFocusedEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardFocusedEvent()", "ticket",
               ticket);
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::InjectOnScreenKeyboardFocusedEvent,
                 base::Unretained(impl_.get()), ticket));
}

void WebModule::InjectOnScreenKeyboardBlurredEvent(int ticket) {
  TRACE_EVENT1("cobalt::browser",
               "WebModule::InjectOnScreenKeyboardBlurredEvent()", "ticket",
               ticket);
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::InjectOnScreenKeyboardBlurredEvent,
                 base::Unretained(impl_.get()), ticket));
}

#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

void WebModule::InjectKeyboardEvent(base::Token type,
                                    const dom::KeyboardEventInit& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectKeyboardEvent()", "type",
               type.c_str());
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::InjectKeyboardEvent,
                            base::Unretained(impl_.get()),
                            scoped_refptr<dom::Element>(), type, event));
}

void WebModule::InjectPointerEvent(base::Token type,
                                   const dom::PointerEventInit& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectPointerEvent()", "type",
               type.c_str());
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::InjectPointerEvent,
                            base::Unretained(impl_.get()),
                            scoped_refptr<dom::Element>(), type, event));
}

void WebModule::InjectWheelEvent(base::Token type,
                                 const dom::WheelEventInit& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectWheelEvent()", "type",
               type.c_str());
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::InjectWheelEvent,
                            base::Unretained(impl_.get()),
                            scoped_refptr<dom::Element>(), type, event));
}

void WebModule::InjectBeforeUnloadEvent() {
  TRACE_EVENT0("cobalt::browser", "WebModule::InjectBeforeUnloadEvent()");
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::InjectBeforeUnloadEvent,
                                      base::Unretained(impl_.get())));
}

std::string WebModule::ExecuteJavascript(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    bool* out_succeeded) {
  TRACE_EVENT0("cobalt::browser", "WebModule::ExecuteJavascript()");
  DCHECK(message_loop());
  DCHECK(impl_);

  base::WaitableEvent got_result(true, false);
  std::string result;
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::ExecuteJavascript,
                 base::Unretained(impl_.get()), script_utf8, script_location,
                 &got_result, &result, out_succeeded));
  got_result.Wait();
  return result;
}

void WebModule::ClearAllIntervalsAndTimeouts() {
  TRACE_EVENT0("cobalt::browser", "WebModule::ClearAllIntervalsAndTimeouts()");
  DCHECK(message_loop());
  DCHECK(impl_);

  if (impl_) {
    message_loop()->PostTask(
        FROM_HERE, base::Bind(&WebModule::Impl::ClearAllIntervalsAndTimeouts,
                              base::Unretained(impl_.get())));
  }
}

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
void WebModule::OnPartialLayoutConsoleCommandReceived(
    const std::string& message) {
  DCHECK(message_loop());
  DCHECK(impl_);

  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::OnPartialLayoutConsoleCommandReceived,
                 base::Unretained(impl_.get()), message));
}
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

#if defined(ENABLE_WEBDRIVER)
scoped_ptr<webdriver::WindowDriver> WebModule::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id) {
  DCHECK(message_loop());
  DCHECK(impl_);

  scoped_ptr<webdriver::WindowDriver> window_driver;
  message_loop()->PostBlockingTask(
      FROM_HERE, base::Bind(&WebModule::Impl::CreateWindowDriver,
                            base::Unretained(impl_.get()), window_id,
                            base::Unretained(&window_driver)));

  return window_driver.Pass();
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUG_CONSOLE)
// May be called from any thread.
debug::DebugServer* WebModule::GetDebugServer() {
  DCHECK(message_loop());
  DCHECK(impl_);

  message_loop()->PostBlockingTask(
      FROM_HERE, base::Bind(&WebModule::Impl::CreateDebugServerIfNull,
                            base::Unretained(impl_.get())));

  return impl_->debug_server();
}
#endif  // defined(ENABLE_DEBUG_CONSOLE)

void WebModule::SetSize(const math::Size& window_dimensions,
                        float video_pixel_ratio) {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::SetSize, base::Unretained(impl_.get()),
                 window_dimensions, video_pixel_ratio));
}

void WebModule::SetCamera3D(const scoped_refptr<input::Camera3D>& camera_3d) {
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::SetCamera3D,
                            base::Unretained(impl_.get()), camera_3d));
}

void WebModule::SetWebMediaPlayerFactory(
    media::WebMediaPlayerFactory* web_media_player_factory) {
  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::SetWebMediaPlayerFactory,
                 base::Unretained(impl_.get()), web_media_player_factory));
}

void WebModule::SetImageCacheCapacity(int64_t bytes) {
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::SetImageCacheCapacity,
                                      base::Unretained(impl_.get()), bytes));
}

void WebModule::SetRemoteTypefaceCacheCapacity(int64_t bytes) {
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::SetRemoteTypefaceCacheCapacity,
                            base::Unretained(impl_.get()), bytes));
}

void WebModule::SetJavascriptGcThreshold(int64_t bytes) {
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::SetJavascriptGcThreshold,
                            base::Unretained(impl_.get()), bytes));
}

void WebModule::Prestart() {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  // We must block here so that we don't queue the finish until after
  // SuspendLoaders has run to completion, and therefore has already queued any
  // precipitate tasks.
  message_loop()->PostBlockingTask(
      FROM_HERE, base::Bind(&WebModule::Impl::SuspendLoaders,
                            base::Unretained(impl_.get()),
                            false /*update_application_state*/));

  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  message_loop()->PostBlockingTask(FROM_HERE,
                                   base::Bind(&WebModule::Impl::FinishSuspend,
                                              base::Unretained(impl_.get())));
}

void WebModule::Start(render_tree::ResourceProvider* resource_provider) {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::Start,
                            base::Unretained(impl_.get()), resource_provider));
}

void WebModule::Pause() {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  message_loop()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::Pause, base::Unretained(impl_.get())));
}

void WebModule::Unpause() {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WebModule::Impl::Unpause, base::Unretained(impl_.get())));
}

void WebModule::Suspend() {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  // We must block here so that we don't queue the finish until after
  // SuspendLoaders has run to completion, and therefore has already queued any
  // precipitate tasks.
  message_loop()->PostBlockingTask(
      FROM_HERE, base::Bind(&WebModule::Impl::SuspendLoaders,
                            base::Unretained(impl_.get()),
                            true /*update_application_state*/));

  // We must block here so that the call doesn't return until the web
  // application has had a chance to process the whole event.
  message_loop()->PostBlockingTask(FROM_HERE,
                                   base::Bind(&WebModule::Impl::FinishSuspend,
                                              base::Unretained(impl_.get())));
}

void WebModule::Resume(render_tree::ResourceProvider* resource_provider) {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::Resume,
                            base::Unretained(impl_.get()), resource_provider));
}

void WebModule::ReduceMemory() {
  // Must only be called by a thread external from the WebModule thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  // We block here so that we block the Low Memory event handler until we have
  // reduced our memory consumption.
  message_loop()->PostBlockingTask(FROM_HERE,
                                   base::Bind(&WebModule::Impl::ReduceMemory,
                                              base::Unretained(impl_.get())));
}

}  // namespace browser
}  // namespace cobalt
