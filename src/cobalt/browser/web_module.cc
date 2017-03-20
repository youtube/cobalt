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

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/stringprintf.h"
#include "cobalt/accessibility/screen_reader.h"
#include "cobalt/accessibility/starboard_tts_engine.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/accessibility/tts_logger.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/poller.h"
#include "cobalt/base/tokens.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/browser/switches.h"
#include "cobalt/browser/web_module_stat_tracker.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/debug/debug_server_module.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/storage.h"
#include "cobalt/dom/url.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/system_window/system_window.h"
#include "starboard/accessibility.h"
#include "starboard/log.h"
#include "starboard/once.h"

namespace cobalt {
namespace browser {

namespace {

#if defined(COBALT_BUILD_TYPE_GOLD)
const int kPollerPeriodMs = 2000;
#else   // #if defined(COBALT_BUILD_TYPE_GOLD)
const int kPollerPeriodMs = 20;
#endif  // #if defined(COBALT_BUILD_TYPE_GOLD)

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

class JSEngineStats {
 public:
  JSEngineStats()
      : js_reserved_memory_("Memory.JS", 0,
                            "The total memory that is reserved by the engine, "
                            "including the part that is actually occupied by "
                            "JS objects, and the part that is not yet.") {}

  static JSEngineStats* GetInstance() {
    return Singleton<JSEngineStats,
                     StaticMemorySingletonTraits<JSEngineStats> >::get();
  }

  void SetReservedMemory(size_t js_reserved_memory) {
    js_reserved_memory_ = static_cast<uint64>(js_reserved_memory);
  }

 private:
  // The total memory that is reserved by the engine, including the part that is
  // actually occupied by JS objects, and the part that is not yet.
  base::CVal<base::cval::SizeInBytes, base::CValPublic> js_reserved_memory_;
};

#if SB_HAS(SPEECH_SYNTHESIS)
bool IsTextToSpeechEnabled() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  // Check for a command-line override to enable TTS.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kUseTTS)) {
    return true;
  }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
  // Check if the tts feature is enabled in Starboard.
  SbAccessibilityTextToSpeechSettings tts_settings = {0};
  // Check platform settings.
  if (SbAccessibilityGetTextToSpeechSettings(&tts_settings)) {
    return tts_settings.has_text_to_speech_setting &&
           tts_settings.is_text_to_speech_enabled;
  }
#endif  // SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
  return false;
}
#endif  // SB_HAS(SPEECH_SYNTHESIS)

// StartupTimer is designed to measure time since the startup of the app.
// It is loader initialized to have the most accurate start time as possible.
class StartupTimer {
 public:
  static StartupTimer* Instance();
  base::TimeDelta TimeSinceStartup() const {
    return base::TimeTicks::Now() - start_time_;
  }

 private:
  StartupTimer() : start_time_(base::TimeTicks::Now()) {}
  base::TimeTicks start_time_;
};

SB_ONCE_INITIALIZE_FUNCTION(StartupTimer, StartupTimer::Instance);
StartupTimer* s_on_startup_init_dont_use = StartupTimer::Instance();
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

  // Called to inject a keyboard event into the web module.
  // Event is directed at a specific element if the element is non-null.
  // Otherwise, the currently focused element receives the event.
  // If element is specified, we must be on the WebModule's message loop
  void InjectKeyboardEvent(scoped_refptr<dom::Element> element,
                           const dom::KeyboardEvent::Data& event);

  // Called to execute JavaScript in this WebModule. Sets the |result|
  // output parameter and signals |got_result|.
  void ExecuteJavascript(const std::string& script_utf8,
                         const base::SourceLocation& script_location,
                         base::WaitableEvent* got_result, std::string* result);

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

  // Suspension of the WebModule is a two-part process since a message loop
  // gap is needed in order to give a chance to handle loader callbacks
  // that were initiated from a loader thread.
  void SuspendLoaders();
  void FinishSuspend();
  void Resume(render_tree::ResourceProvider* resource_provider);

  void ReportScriptError(const base::SourceLocation& source_location,
                         const std::string& error_message);

 private:
  class DocumentLoadedObserver;

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

  void OnCspPolicyChanged();

  scoped_refptr<script::GlobalEnvironment> global_environment() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return global_environment_;
  }

  void OnError(const std::string& error) {
    error_callback_.Run(window_->location()->url(), error);
  }

  void UpdateJavaScriptEngineStats() {
    if (javascript_engine_) {
      JSEngineStats::GetInstance()->SetReservedMemory(
          javascript_engine_->UpdateMemoryStatsAndReturnReserved());
    }
  }

  // Thread checker ensures all calls to the WebModule are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;

  std::string name_;

  // Simple flag used for basic error checking.
  bool is_running_;

  // Object that provides renderer resources like images and fonts.
  render_tree::ResourceProvider* resource_provider_;

  // CSS parser.
  scoped_ptr<css_parser::Parser> css_parser_;

  // DOM (HTML / XML) parser.
  scoped_ptr<dom_parser::Parser> dom_parser_;

  // FetcherFactory that is used to create a fetcher according to URL.
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;

  // LoaderFactory that is used to acquire references to resources from a
  // URL.
  scoped_ptr<loader::LoaderFactory> loader_factory_;

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

  // Poller that updates javascript engine stats.
  scoped_ptr<base::PollerWithThread> javascript_engine_poller_;

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

  // TTSEngine that the ScreenReader speaks to.
  scoped_ptr<accessibility::TTSEngine> tts_engine_;

  // Utters the text contents of the focused element.
  scoped_ptr<accessibility::ScreenReader> screen_reader_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Allows the debugger to add render components to the web module.
  // Used for DOM node highlighting and overlay messages.
  scoped_ptr<debug::RenderOverlay> debug_overlay_;

  // The core of the debugging system, described here:
  // https://docs.google.com/document/d/1lZhrBTusQZJsacpt21J3kPgnkj7pyQObhFqYktvm40Y
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
};

class WebModule::Impl::DocumentLoadedObserver : public dom::DocumentObserver {
 public:
  typedef std::vector<base::Closure> ClosureVector;
  explicit DocumentLoadedObserver(const ClosureVector& loaded_callbacks)
      : loaded_callbacks_(loaded_callbacks) {}
  // Called at most once, when document and all referred resources are loaded.
  void OnLoad() OVERRIDE {
    for (size_t i = 0; i < loaded_callbacks_.size(); ++i) {
      loaded_callbacks_[i].Run();
    }
  }

  void OnMutation() OVERRIDE{};
  void OnFocusChanged() OVERRIDE{};

 private:
  ClosureVector loaded_callbacks_;
};

WebModule::Impl::Impl(const ConstructionData& data)
    : name_(data.options.name), is_running_(false) {
  resource_provider_ = data.resource_provider;

  // Currently we rely on a platform to explicitly specify that it supports
  // the map-to-mesh filter via the ENABLE_MTM define (and the 'enable_mtm' gyp
  // variable).  When we have better support for checking for decode to texture
  // support, it would be nice to switch this logic to something like:
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
#if defined(ENABLE_MTM)
      css_parser::Parser::kSupportsMapToMesh;
#else
      css_parser::Parser::kDoesNotSupportMapToMesh;
#endif

  css_parser_ = css_parser::Parser::Create(supports_map_to_mesh);
  DCHECK(css_parser_);

  dom_parser_.reset(new dom_parser::Parser(
      kDOMMaxElementDepth,
      base::Bind(&WebModule::Impl::OnError, base::Unretained(this))));
  DCHECK(dom_parser_);

  blob_registry_.reset(new dom::Blob::Registry);

  fetcher_factory_.reset(new loader::FetcherFactory(
      data.network_module, data.options.extra_web_file_dir,
      dom::URL::MakeBlobResolverCallback(blob_registry_.get())));
  DCHECK(fetcher_factory_);

  loader_factory_.reset(
      new loader::LoaderFactory(fetcher_factory_.get(), resource_provider_,
                                data.options.software_decoder_thread_priority,
                                data.options.hardware_decoder_thread_priority,
                                data.options.fetcher_lifetime_thread_priority));

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

  javascript_engine_ = script::JavaScriptEngine::CreateEngine();
  DCHECK(javascript_engine_);

#if defined(COBALT_ENABLE_JAVASCRIPT_ERROR_LOGGING)
  script::JavaScriptEngine::ErrorHandler error_handler =
      base::Bind(&WebModule::Impl::ReportScriptError, base::Unretained(this));
  javascript_engine_->RegisterErrorHandler(error_handler);
#endif

  javascript_engine_poller_.reset(new base::PollerWithThread(
      base::Bind(&WebModule::Impl::UpdateJavaScriptEngineStats,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kPollerPeriodMs)));

  global_environment_ = javascript_engine_->CreateGlobalEnvironment();
  DCHECK(global_environment_);

  execution_state_ =
      script::ExecutionState::CreateExecutionState(global_environment_);
  DCHECK(execution_state_);

  script_runner_ =
      script::ScriptRunner::CreateScriptRunner(global_environment_);
  DCHECK(script_runner_);

  media_source_registry_.reset(new dom::MediaSource::Registry);

  window_ = new dom::Window(
      data.window_dimensions.width(), data.window_dimensions.height(),
      css_parser_.get(), dom_parser_.get(), fetcher_factory_.get(),
      &resource_provider_, image_cache_.get(),
      reduced_image_cache_capacity_manager_.get(), remote_typeface_cache_.get(),
      mesh_cache_.get(), local_storage_database_.get(), data.media_module,
      data.media_module, execution_state_.get(), script_runner_.get(),
      media_source_registry_.get(),
      web_module_stat_tracker_->dom_stat_tracker(), data.initial_url,
      data.network_module->GetUserAgent(),
      data.network_module->preferred_language(),
      data.options.navigation_callback,
      base::Bind(&WebModule::Impl::OnError, base::Unretained(this)),
      data.network_module->cookie_jar(), data.network_module->GetPostSender(),
      data.options.location_policy, data.options.csp_enforcement_mode,
      base::Bind(&WebModule::Impl::OnCspPolicyChanged, base::Unretained(this)),
      base::Bind(&WebModule::Impl::OnRanAnimationFrameCallbacks,
                 base::Unretained(this)),
      data.window_close_callback, data.system_window_,
      data.options.csp_insecure_allowed_token, data.dom_max_element_depth);
  DCHECK(window_);

  window_weak_ = base::AsWeakPtr(window_.get());
  DCHECK(window_weak_);

  environment_settings_.reset(new dom::DOMSettings(
      kDOMMaxElementDepth, fetcher_factory_.get(), data.network_module,
      data.media_module, window_, media_source_registry_.get(),
      blob_registry_.get(), data.media_module, javascript_engine_.get(),
      global_environment_.get(), &mutation_observer_task_manager_,
      data.options.dom_settings_options));
  DCHECK(environment_settings_);

  global_environment_->CreateGlobalObject(window_, environment_settings_.get());

  render_tree_produced_callback_ = data.render_tree_produced_callback;
  DCHECK(!render_tree_produced_callback_.is_null());

  error_callback_ = data.error_callback;
  DCHECK(!error_callback_.is_null());

  layout_manager_.reset(new layout::LayoutManager(
      name_, window_.get(), base::Bind(&WebModule::Impl::OnRenderTreeProduced,
                                       base::Unretained(this)),
      data.options.layout_trigger, data.dom_max_element_depth,
      data.layout_refresh_rate, data.network_module->preferred_language(),
      web_module_stat_tracker_->layout_stat_tracker()));
  DCHECK(layout_manager_);

  resource_provider_ = data.resource_provider;

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

  InjectCustomWindowAttributes(data.options.injected_window_attributes);

  if (!data.options.loaded_callbacks.empty()) {
    document_load_observer_.reset(
        new DocumentLoadedObserver(data.options.loaded_callbacks));
    window_->document()->AddObserver(document_load_observer_.get());
  }

  // If a TTSEngine was provided through the options, use it.
  accessibility::TTSEngine* tts_engine = data.options.tts_engine;
  if (!tts_engine) {
#if SB_HAS(SPEECH_SYNTHESIS)
    if (IsTextToSpeechEnabled()) {
      // Create a StarboardTTSEngine if TTS is enabled.
      tts_engine_.reset(new accessibility::StarboardTTSEngine());
    }
#endif  // SB_HAS(SPEECH_SYNTHESIS)
#if !defined(COBALT_BUILD_TYPE_GOLD)
    if (!tts_engine_) {
      tts_engine_.reset(new accessibility::TTSLogger());
    }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
    tts_engine = tts_engine_.get();
  }
  if (tts_engine) {
    screen_reader_.reset(new accessibility::ScreenReader(
        window_->document(), tts_engine, &mutation_observer_task_manager_));
  }
  is_running_ = true;
}

WebModule::Impl::~Impl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  is_running_ = false;
  global_environment_->SetReportEvalCallback(base::Closure());
  window_->DispatchEvent(new dom::Event(base::Tokens::unload()));
  document_load_observer_.reset();

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_overlay_.reset();
  debug_server_module_.reset();
#endif  // ENABLE_DEBUG_CONSOLE

  // Disable callbacks for the resource caches. Otherwise, it is possible for a
  // callback to occur into a DOM object that is being kept alive by a JS engine
  // reference even after the DOM tree has been destroyed. This can result in a
  // crash when the callback attempts to access a stale Document pointer.
  remote_typeface_cache_->DisableCallbacks();
  image_cache_->DisableCallbacks();

  screen_reader_.reset();
  layout_manager_.reset();
  environment_settings_.reset();
  window_weak_.reset();
  window_ = NULL;
  media_source_registry_.reset();
  blob_registry_.reset();
  script_runner_.reset();
  execution_state_.reset();
  global_environment_ = NULL;
  javascript_engine_poller_.reset();
  javascript_engine_.reset();
  web_module_stat_tracker_.reset();
  local_storage_database_.reset();
  mesh_cache_.reset();
  remote_typeface_cache_.reset();
  image_cache_.reset();
  fetcher_factory_.reset();
  dom_parser_.reset();
  css_parser_.reset();
}

void WebModule::Impl::InjectKeyboardEvent(
    scoped_refptr<dom::Element> element,
    const dom::KeyboardEvent::Data& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(window_);

  // Construct the DOM object from the keyboard event builder and inject it
  // into the window.
  scoped_refptr<dom::KeyboardEvent> keyboard_event(
      new dom::KeyboardEvent(event));

  web_module_stat_tracker_->OnStartInjectEvent(keyboard_event);

  if (element) {
    element->DispatchEvent(keyboard_event);
  } else {
    window_->InjectEvent(keyboard_event);
  }

  web_module_stat_tracker_->OnEndInjectEvent(
      window_->HasPendingAnimationFrameCallbacks(),
      layout_manager_->IsNewRenderTreePending());
}

void WebModule::Impl::ExecuteJavascript(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    base::WaitableEvent* got_result, std::string* result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  DCHECK(script_runner_);
  *result = script_runner_->Execute(script_utf8, script_location);
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
      layout_manager_->IsNewRenderTreePending());
}

void WebModule::Impl::OnRenderTreeProduced(
    const LayoutResults& layout_results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_running_);
  // Notify the stat tracker that a render tree has been produced.
  web_module_stat_tracker_->OnRenderTreeProduced();

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_overlay_->OnRenderTreeProduced(layout_results);
#else  // ENABLE_DEBUG_CONSOLE
  render_tree_produced_callback_.Run(layout_results);
#endif  // ENABLE_DEBUG_CONSOLE
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
    global_environment_->Bind(iter->first, iter->second.Run());
  }
}

void WebModule::Impl::SuspendLoaders() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::SuspendLoaders()");

  // Stop the generation of render trees.
  layout_manager_->Suspend();

  // Clear out the loader factory's resource provider, possibly aborting any
  // in-progress loads.
  loader_factory_->Suspend();
}

void WebModule::Impl::FinishSuspend() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::FinishSuspend()");
  DCHECK(resource_provider_);

  // Ensure the document is not holding onto any more image cached resources so
  // that they are eligible to be purged.
  window_->document()->PurgeCachedResourceReferencesRecursively();

  // Clear out all image resources from the image cache. We need to do this
  // after we abort all in-progress loads, and after we clear all document
  // references, or they will still be referenced and won't be cleared from the
  // cache.
  image_cache_->Purge();
  remote_typeface_cache_->Purge();

#if defined(ENABLE_DEBUG_CONSOLE)
  // The debug overlay may be holding onto a render tree, clear that out.
  debug_overlay_->ClearInput();
#endif

  // Finally mark that we have no resource provider.
  resource_provider_ = NULL;
}

void WebModule::Impl::Resume(render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::browser", "WebModule::Impl::Resume()");
  DCHECK(resource_provider);
  DCHECK(!resource_provider_);

  resource_provider_ = resource_provider;

  loader_factory_->Resume(resource_provider_);

  // Permit render trees to be generated again.  Layout will have been
  // invalidated with the call to Suspend(), so the layout manager's first task
  // will be to perform a full re-layout.
  layout_manager_->Resume();
}

void WebModule::Impl::ReportScriptError(
    const base::SourceLocation& source_location,
    const std::string& error_message) {
  std::string file_name =
      FilePath(source_location.file_path).BaseName().value();

  std::stringstream ss;
  base::TimeDelta dt = StartupTimer::Instance()->TimeSinceStartup();

  // Create the error output.
  // Example:
  //   JS:50250:file.js(29,80): ka(...) is not iterable
  //   JS:<time millis><js-file-name>(<line>,<column>):<message>
  ss << "JS:" << dt.InMilliseconds() << ":" << file_name << "("
     << source_location.line_number << ","
     << source_location.column_number << "): " << error_message << "\n";
  SbLogRaw(ss.str().c_str());
}

WebModule::DestructionObserver::DestructionObserver(WebModule* web_module)
    : web_module_(web_module) {}

void WebModule::DestructionObserver::WillDestroyCurrentMessageLoop() {
  web_module_->impl_.reset();
}

WebModule::Options::Options()
    : name("WebModule"),
      layout_trigger(layout::LayoutManager::kOnDocumentMutation),
      image_cache_capacity(COBALT_IMAGE_CACHE_SIZE_IN_BYTES),
      remote_typeface_cache_capacity(
          COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES),
      mesh_cache_capacity(COBALT_MESH_CACHE_SIZE_IN_BYTES),
      csp_enforcement_mode(dom::kCspEnforcementEnable),
      csp_insecure_allowed_token(0),
      track_event_stats(false),
      image_cache_capacity_multiplier_when_playing_video(1.0f),
      thread_priority(base::kThreadPriority_Normal),
      software_decoder_thread_priority(base::kThreadPriority_Low),
      hardware_decoder_thread_priority(base::kThreadPriority_High),
      fetcher_lifetime_thread_priority(base::kThreadPriority_High),
      tts_engine(NULL) {}

WebModule::WebModule(
    const GURL& initial_url,
    const OnRenderTreeProducedCallback& render_tree_produced_callback,
    const OnErrorCallback& error_callback,
    const base::Closure& window_close_callback,
    media::MediaModule* media_module, network::NetworkModule* network_module,
    const math::Size& window_dimensions,
    render_tree::ResourceProvider* resource_provider,
    system_window::SystemWindow* system_window, float layout_refresh_rate,
    const Options& options)
    : thread_(options.name.c_str()) {
  ConstructionData construction_data(
      initial_url, render_tree_produced_callback, error_callback,
      window_close_callback, media_module, network_module, window_dimensions,
      resource_provider, kDOMMaxElementDepth, system_window,
      layout_refresh_rate, options);

  // Start the dedicated thread and create the internal implementation
  // object on that thread.
  thread_.StartWithOptions(base::Thread::Options(
      MessageLoop::TYPE_DEFAULT, cobalt::browser::kWebModuleStackSize,
      options.thread_priority));
  DCHECK(message_loop());

  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Initialize, base::Unretained(this),
                            construction_data));

  // Block this thread until the initialization is complete.
  // TODO: Figure out why this is necessary.
  // It would be preferable to return immediately and let the WebModule
  // continue in its own time, but without this wait there is a race condition
  // such that inline scripts may be executed before the document elements they
  // operate on are present.
  base::WaitableEvent is_initialized(true, false);
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&base::WaitableEvent::Signal,
                                      base::Unretained(&is_initialized)));
  is_initialized.Wait();

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
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&MessageLoop::AddDestructionObserver,
                                      base::Unretained(message_loop()),
                                      base::Unretained(&destruction_observer)));

  base::WaitableEvent did_register_shutdown_observer(true, false);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&did_register_shutdown_observer)));
  did_register_shutdown_observer.Wait();

  // This will cancel the timers for tasks, which help the thread exit
  ClearAllIntervalsAndTimeouts();

  // Stop the thread. This will cause the destruction observer to be notified.
  thread_.Stop();
}

void WebModule::Initialize(const ConstructionData& data) {
  DCHECK_EQ(MessageLoop::current(), message_loop());
  impl_.reset(new Impl(data));
}

void WebModule::InjectKeyboardEvent(const dom::KeyboardEvent::Data& event) {
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectKeyboardEvent()", "type",
               event.type);
  DCHECK(message_loop());
  DCHECK(impl_);
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::InjectKeyboardEvent,
                                      base::Unretained(impl_.get()),
                                      scoped_refptr<dom::Element>(), event));
}

std::string WebModule::ExecuteJavascript(
    const std::string& script_utf8,
    const base::SourceLocation& script_location) {
  TRACE_EVENT0("cobalt::browser", "WebModule::ExecuteJavascript()");
  DCHECK(message_loop());
  DCHECK(impl_);

  base::WaitableEvent got_result(true, false);
  std::string result;
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::ExecuteJavascript,
                            base::Unretained(impl_.get()), script_utf8,
                            script_location, &got_result, &result));
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
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::CreateWindowDriver,
                                      base::Unretained(impl_.get()), window_id,
                                      base::Unretained(&window_driver)));

  base::WaitableEvent window_driver_created(true, false);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                            base::Unretained(&window_driver_created)));
  window_driver_created.Wait();

  return window_driver.Pass();
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUG_CONSOLE)
// May be called from any thread.
debug::DebugServer* WebModule::GetDebugServer() {
  DCHECK(message_loop());
  DCHECK(impl_);

  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::CreateDebugServerIfNull,
                                      base::Unretained(impl_.get())));

  base::WaitableEvent debug_server_created(true, false);
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&base::WaitableEvent::Signal,
                                      base::Unretained(&debug_server_created)));

  debug_server_created.Wait();
  return impl_->debug_server();
}
#endif  // defined(ENABLE_DEBUG_CONSOLE)

void WebModule::Suspend() {
  TRACE_EVENT0("cobalt::browser", "WebModule::Suspend()");

  // Suspend() must only be called by a thread external from the WebModule
  // thread.
  DCHECK_NE(MessageLoop::current(), message_loop());

  base::WaitableEvent task_finished(false /* automatic reset */,
                                    false /* initially unsignaled */);

  // Suspension of the WebModule is orchestrated here in two phases.
  // 1) Send a signal to suspend WebModule loader activity and cancel any
  //    in-progress loads.  Since loading may occur from any thread, this may
  //    result in cancel/completion callbacks being posted to message_loop().
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::SuspendLoaders,
                                      base::Unretained(impl_.get())));

  // Wait for the suspension task to complete before proceeding.
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&base::WaitableEvent::Signal,
                                      base::Unretained(&task_finished)));
  task_finished.Wait();

  // 2) Now append to the task queue a task to complete the suspension process.
  //    Between 1 and 2, tasks may have been registered to handle resource load
  //    completion events, and so this FinishSuspend task will be executed after
  //    the load completions are all resolved.
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&WebModule::Impl::FinishSuspend,
                                      base::Unretained(impl_.get())));

  // Wait for suspension to fully complete on the WebModule thread before
  // continuing.
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&base::WaitableEvent::Signal,
                                      base::Unretained(&task_finished)));
  task_finished.Wait();
}

void WebModule::Resume(render_tree::ResourceProvider* resource_provider) {
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&WebModule::Impl::Resume,
                            base::Unretained(impl_.get()), resource_provider));
}

}  // namespace browser
}  // namespace cobalt
