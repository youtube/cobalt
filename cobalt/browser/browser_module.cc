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

#include "cobalt/browser/browser_module.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/time.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/tokens.h"
#include "cobalt/browser/resource_provider_array_buffer_allocator.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/storage_upgrade_handler.h"
#include "cobalt/browser/switches.h"
#include "cobalt/browser/webapi_extension.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/keycode.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/window.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/input/input_device_manager_fuzzer.h"
#include "nb/memory_scope.h"
#include "starboard/atomic.h"
#include "starboard/configuration.h"
#include "starboard/system.h"
#include "starboard/time.h"

#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#include "starboard/ps4/core_dump_handler.h"
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)

namespace cobalt {

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
namespace timestamp {
// This is a temporary workaround.
extern SbAtomic64 g_last_render_timestamp;
}  // namespace timestamp

namespace {
struct NonTrivialGlobalVariables {
  NonTrivialGlobalVariables();

  SbAtomic64* last_render_timestamp;
};

NonTrivialGlobalVariables::NonTrivialGlobalVariables() {
  last_render_timestamp = &cobalt::timestamp::g_last_render_timestamp;
  SbAtomicNoBarrier_Exchange64(last_render_timestamp,
                               static_cast<SbAtomic64>(SbTimeGetNow()));
}

base::LazyInstance<NonTrivialGlobalVariables> non_trivial_global_variables =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace
#endif

namespace browser {
namespace {

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
// Timeout for last render.
const int kLastRenderTimeoutSeconds = 15;

// Polling interval for timeout_polling_thread_.
const int kRenderTimeOutPollingDelaySeconds = 1;

// Minimum number of continuous times the timeout expirations. This is used to
// prevent unintended behavior in situations such as when returning from
// suspended state. Note that the timeout response trigger will be delayed
// after the actual timeout expiration by this value times the polling delay.
const int kMinimumContinuousRenderTimeoutExpirations = 2;

// Name for timeout_polling_thread_.
const char* kTimeoutPollingThreadName = "TimeoutPolling";

// This specifies the percentage of calls to OnRenderTimeout() that result in a
// call to OnError().
const int kRenderTimeoutErrorPercentage = 99;

#endif

// This constant defines the maximum rate at which the layout engine will
// refresh over time.  Since there is little benefit in performing a layout
// faster than the display's refresh rate, we set this to 60Hz.
const float kLayoutMaxRefreshFrequencyInHz = 60.0f;

// TODO: Subscribe to viewport size changes.

const int kMainWebModuleZIndex = 1;
const int kSplashScreenZIndex = 2;

#if defined(ENABLE_DEBUG_CONSOLE)

const int kDebugConsoleZIndex = 3;

const char kFuzzerToggleCommand[] = "fuzzer_toggle";
const char kFuzzerToggleCommandShortHelp[] = "Toggles the input fuzzer on/off.";
const char kFuzzerToggleCommandLongHelp[] =
    "Each time this is called, it will toggle whether the input fuzzer is "
    "activated or not.  While activated, input will constantly and randomly be "
    "generated and passed directly into the main web module.";

const char kSetMediaConfigCommand[] = "set_media_config";
const char kSetMediaConfigCommandShortHelp[] =
    "Sets media module configuration.";
const char kSetMediaConfigCommandLongHelp[] =
    "This can be called in the form of set_media_config('name=value'), where "
    "name is a string and value is an int.  Refer to the implementation of "
    "MediaModule::SetConfiguration() on individual platform for settings "
    "supported on the particular platform.";

#if defined(ENABLE_SCREENSHOT)
// Command to take a screenshot.
const char kScreenshotCommand[] = "screenshot";

// Help strings for the navigate command.
const char kScreenshotCommandShortHelp[] = "Takes a screenshot.";
const char kScreenshotCommandLongHelp[] =
    "Creates a screenshot of the most recent layout tree and writes it "
    "to disk. Logs the filename of the screenshot to the console when done.";
#endif  // defined(ENABLE_SCREENSHOT)

#if defined(ENABLE_SCREENSHOT)
void ScreenshotCompleteCallback(const FilePath& output_path) {
  DLOG(INFO) << "Screenshot written to " << output_path.value();
}

void OnScreenshotMessage(BrowserModule* browser_module,
                         const std::string& message) {
  UNREFERENCED_PARAMETER(message);
  FilePath dir;
  if (!PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &dir)) {
    NOTREACHED() << "Failed to get debug out directory.";
  }

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  DCHECK(exploded.HasValidValues());
  std::string screenshot_file_name =
      StringPrintf("screenshot-%04d-%02d-%02d_%02d-%02d-%02d.png",
                   exploded.year, exploded.month, exploded.day_of_month,
                   exploded.hour, exploded.minute, exploded.second);

  FilePath output_path = dir.Append(screenshot_file_name);
  browser_module->RequestScreenshotToFile(
      output_path, base::Bind(&ScreenshotCompleteCallback, output_path));
}
#endif  // defined(ENABLE_SCREENSHOT)

#endif  // defined(ENABLE_DEBUG_CONSOLE)

scoped_refptr<script::Wrappable> CreateH5VCC(
    const h5vcc::H5vcc::Settings& settings,
    const scoped_refptr<dom::Window>& window,
    dom::MutationObserverTaskManager* mutation_observer_task_manager,
    script::GlobalEnvironment* global_environment) {
  UNREFERENCED_PARAMETER(global_environment);
  return scoped_refptr<script::Wrappable>(
      new h5vcc::H5vcc(settings, window, mutation_observer_task_manager));
}

scoped_refptr<script::Wrappable> CreateExtensionInterface(
    const scoped_refptr<dom::Window>& window,
    dom::MutationObserverTaskManager* mutation_observer_task_manager,
    script::GlobalEnvironment* global_environment) {
  UNREFERENCED_PARAMETER(mutation_observer_task_manager);
  return CreateWebAPIExtensionObject(window, global_environment);
}

renderer::RendererModule::Options RendererModuleWithCameraOptions(
    renderer::RendererModule::Options options,
    scoped_refptr<input::Camera3D> camera_3d) {
  options.get_camera_transform = base::Bind(
      &input::Camera3D::GetCameraTransformAndUpdateOrientation, camera_3d);
  return options;  // Copy.
}

}  // namespace

BrowserModule::BrowserModule(const GURL& url,
                             base::ApplicationState initial_application_state,
                             base::EventDispatcher* event_dispatcher,
                             account::AccountManager* account_manager,
                             const Options& options)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      options_(options),
      self_message_loop_(MessageLoop::current()),
      event_dispatcher_(event_dispatcher),
      storage_manager_(make_scoped_ptr(new StorageUpgradeHandler(url))
                           .PassAs<storage::StorageManager::UpgradeHandler>(),
                       options_.storage_manager_options),
      is_rendered_(false),
#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
      array_buffer_allocator_(
          new ResourceProviderArrayBufferAllocator(GetResourceProvider())),
      array_buffer_cache_(new dom::ArrayBuffer::Cache(3 * 1024 * 1024)),
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
      can_play_type_handler_(media::MediaModule::CreateCanPlayTypeHandler()),
      network_module_(&storage_manager_, event_dispatcher_,
                      options_.network_module_options),
      splash_screen_cache_(new SplashScreenCache()),
      web_module_loaded_(true /* manually_reset */,
                         false /* initially_signalled */),
      web_module_recreated_callback_(options_.web_module_recreated_callback),
      navigate_count_(0),
      navigate_time_("Time.Browser.Navigate", 0,
                     "The last time a navigation occurred."),
      on_load_event_time_("Time.Browser.OnLoadEvent", 0,
                          "The last time the window.OnLoad event fired."),
#if defined(ENABLE_DEBUG_CONSOLE)
      ALLOW_THIS_IN_INITIALIZER_LIST(fuzzer_toggle_command_handler_(
          kFuzzerToggleCommand,
          base::Bind(&BrowserModule::OnFuzzerToggle, base::Unretained(this)),
          kFuzzerToggleCommandShortHelp, kFuzzerToggleCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(set_media_config_command_handler_(
          kSetMediaConfigCommand,
          base::Bind(&BrowserModule::OnSetMediaConfig, base::Unretained(this)),
          kSetMediaConfigCommandShortHelp, kSetMediaConfigCommandLongHelp)),
#if defined(ENABLE_SCREENSHOT)
      ALLOW_THIS_IN_INITIALIZER_LIST(screenshot_command_handler_(
          kScreenshotCommand,
          base::Bind(&OnScreenshotMessage, base::Unretained(this)),
          kScreenshotCommandShortHelp, kScreenshotCommandLongHelp)),
#endif  // defined(ENABLE_SCREENSHOT)
#endif  // defined(ENABLE_DEBUG_CONSOLE)
      has_resumed_(true, false),
#if defined(COBALT_CHECK_RENDER_TIMEOUT)
      timeout_polling_thread_(kTimeoutPollingThreadName),
      render_timeout_count_(0),
#endif
      on_error_retry_count_(0),
      waiting_for_error_retry_(false),
      will_quit_(false),
      application_state_(initial_application_state),
      main_web_module_generation_(0),
      next_timeline_id_(1),
      current_splash_screen_timeline_id_(-1),
      current_main_web_module_timeline_id_(-1) {
  h5vcc_url_handler_.reset(new H5vccURLHandler(this));

#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
  SbCoreDumpRegisterHandler(BrowserModule::CoreDumpHandler, this);
  on_error_triggered_count_ = 0;
#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  recovery_mechanism_triggered_count_ = 0;
  timeout_response_trigger_count_ = 0;
#endif  // defined(COBALT_CHECK_RENDER_TIMEOUT)
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  timeout_polling_thread_.Start();
  timeout_polling_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BrowserModule::OnPollForRenderTimeout, base::Unretained(this),
                 url),
      base::TimeDelta::FromSeconds(kRenderTimeOutPollingDelaySeconds));
#endif
  TRACE_EVENT0("cobalt::browser", "BrowserModule::BrowserModule()");

  // Create the main web module layer.
  main_web_module_layer_ =
      render_tree_combiner_.CreateLayer(kMainWebModuleZIndex);
  // Create the splash screen layer.
  splash_screen_layer_ = render_tree_combiner_.CreateLayer(kSplashScreenZIndex);
// Create the debug console layer.
#if defined(ENABLE_DEBUG_CONSOLE)
  debug_console_layer_ = render_tree_combiner_.CreateLayer(kDebugConsoleZIndex);
#endif

  // Setup our main web module to have the H5VCC API injected into it.
  DCHECK(!ContainsKey(options_.web_module_options.injected_window_attributes,
                      "h5vcc"));
  h5vcc::H5vcc::Settings h5vcc_settings;
  h5vcc_settings.media_module = media_module_.get();
  h5vcc_settings.network_module = &network_module_;
  h5vcc_settings.account_manager = account_manager;
  h5vcc_settings.event_dispatcher = event_dispatcher_;
  h5vcc_settings.initial_deep_link = options_.initial_deep_link;
  options_.web_module_options.injected_window_attributes["h5vcc"] =
      base::Bind(&CreateH5VCC, h5vcc_settings);

  base::optional<std::string> extension_object_name =
      GetWebAPIExtensionObjectPropertyName();
  if (extension_object_name) {
    options_.web_module_options
        .injected_window_attributes[*extension_object_name] =
        base::Bind(&CreateExtensionInterface);
  }

#if defined(ENABLE_DEBUG_CONSOLE) && defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInputFuzzer)) {
    OnFuzzerToggle(std::string());
  }
  if (command_line->HasSwitch(switches::kSuspendFuzzer)) {
    suspend_fuzzer_.emplace();
  }
#endif  // ENABLE_DEBUG_CONSOLE && ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  if (application_state_ == base::kApplicationStateStarted ||
      application_state_ == base::kApplicationStatePaused) {
    InitializeSystemWindow();
  } else if (application_state_ == base::kApplicationStatePreloading) {
#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
    // Preloading is not supported on platforms that allocate ArrayBuffers on
    // GPU memory.
    NOTREACHED();
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
    resource_provider_stub_.emplace(true /*allocate_image_data*/);
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  debug_console_.reset(new DebugConsole(
      application_state_,
      base::Bind(&BrowserModule::QueueOnDebugConsoleRenderTreeProduced,
                 base::Unretained(this)),
      &network_module_, GetViewportSize(), GetResourceProvider(),
      kLayoutMaxRefreshFrequencyInHz,
      base::Bind(&BrowserModule::GetDebugServer, base::Unretained(this)),
      options_.web_module_options.javascript_engine_options,
      base::Bind(&BrowserModule::GetSbWindow, base::Unretained(this))));
  lifecycle_observers_.AddObserver(debug_console_.get());
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  fallback_splash_screen_url_ = options.fallback_splash_screen_url;
  // Synchronously construct our WebModule object.
  Navigate(url);
  DCHECK(web_module_);
}

BrowserModule::~BrowserModule() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  on_error_retry_timer_.Stop();
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
  SbCoreDumpUnregisterHandler(BrowserModule::CoreDumpHandler, this);
#endif
}

void BrowserModule::Navigate(const GURL& url) {
  DLOG(INFO) << "In BrowserModule::Navigate " << url;
  TRACE_EVENT1("cobalt::browser", "BrowserModule::Navigate()", "url",
               url.spec());
  // Reset the waitable event regardless of the thread. This ensures that the
  // webdriver won't incorrectly believe that the webmodule has finished loading
  // when it calls Navigate() and waits for the |web_module_loaded_| signal.
  web_module_loaded_.Reset();

  // Repost to our own message loop if necessary.
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::Navigate, weak_this_, url));
    return;
  }

  // First try the registered handlers (e.g. for h5vcc://). If one of these
  // handles the URL, we don't use the web module.
  if (TryURLHandlers(url)) {
    return;
  }

  // Clear error handling once we're told to navigate, either because it's the
  // retry from the error or something decided we should navigate elsewhere.
  on_error_retry_timer_.Stop();
  waiting_for_error_retry_ = false;

  // Navigations aren't allowed if the app is suspended. If this is the case,
  // simply set the pending navigate url, which will cause the navigation to
  // occur when Cobalt resumes, and return.
  if (application_state_ == base::kApplicationStateSuspended) {
    pending_navigate_url_ = url.spec();
    return;
  }

  // Now that we know the navigation is occurring, clear out
  // |pending_navigate_url_|.
  pending_navigate_url_.clear();

  // Destroy old WebModule first, so we don't get a memory high-watermark after
  // the second WebModule's constructor runs, but before scoped_ptr::reset() is
  // run.
  if (web_module_) {
    lifecycle_observers_.RemoveObserver(web_module_.get());
  }
  web_module_.reset(NULL);

  // Increment the navigation generation so that we can attach it to event
  // callbacks as a way of identifying the new web module from the old ones.
  ++main_web_module_generation_;
  current_splash_screen_timeline_id_ = next_timeline_id_++;
  current_main_web_module_timeline_id_ = next_timeline_id_++;

  main_web_module_layer_->Reset();

  // Wait until after the old WebModule is destroyed before setting the navigate
  // time so that it won't be included in the time taken to load the URL.
  ++navigate_count_;
  navigate_time_ = base::TimeTicks::Now().ToInternalValue();

  // Show a splash screen while we're waiting for the web page to load.
  const math::Size& viewport_size = GetViewportSize();

  DestroySplashScreen(base::TimeDelta());
  if (options_.enable_splash_screen_on_reloads || navigate_count_ == 1) {
    base::optional<std::string> key = SplashScreenCache::GetKeyForStartUrl(url);
    if (fallback_splash_screen_url_ ||
        (key && splash_screen_cache_->IsSplashScreenCached(*key))) {
      splash_screen_.reset(new SplashScreen(
          application_state_,
          base::Bind(&BrowserModule::QueueOnSplashScreenRenderTreeProduced,
                     base::Unretained(this)),
          &network_module_, viewport_size, GetResourceProvider(),
          kLayoutMaxRefreshFrequencyInHz, fallback_splash_screen_url_, url,
          splash_screen_cache_.get(),
          base::Bind(&BrowserModule::DestroySplashScreen, weak_this_),
          base::Bind(&BrowserModule::GetSbWindow, base::Unretained(this))));
      lifecycle_observers_.AddObserver(splash_screen_.get());
    }
  }

  // Create new WebModule.
#if !defined(COBALT_FORCE_CSP)
  options_.web_module_options.csp_insecure_allowed_token =
      dom::CspDelegateFactory::GetInsecureAllowedToken();
#endif
  WebModule::Options options(options_.web_module_options);
  options.splash_screen_cache = splash_screen_cache_.get();
  options.navigation_callback =
      base::Bind(&BrowserModule::Navigate, base::Unretained(this));
  options.loaded_callbacks.push_back(
      base::Bind(&BrowserModule::OnLoad, base::Unretained(this)));
#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
  options.dom_settings_options.array_buffer_allocator =
      array_buffer_allocator_.get();
  options.dom_settings_options.array_buffer_cache = array_buffer_cache_.get();
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
#if defined(ENABLE_FAKE_MICROPHONE)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kFakeMicrophone) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInputFuzzer)) {
    options.dom_settings_options.microphone_options.enable_fake_microphone =
        true;
  }
#endif  // defined(ENABLE_FAKE_MICROPHONE)

  options.image_cache_capacity_multiplier_when_playing_video =
      COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO;
  if (input_device_manager_) {
    options.camera_3d = input_device_manager_->camera_3d();
  }

  float video_pixel_ratio = 1.0f;
  if (system_window_) {
    video_pixel_ratio = system_window_->GetVideoPixelRatio();
  }

  web_module_.reset(new WebModule(
      url, application_state_,
      base::Bind(&BrowserModule::QueueOnRenderTreeProduced,
                 base::Unretained(this), main_web_module_generation_),
      base::Bind(&BrowserModule::OnError, base::Unretained(this)),
      base::Bind(&BrowserModule::OnWindowClose, base::Unretained(this)),
      base::Bind(&BrowserModule::OnWindowMinimize, base::Unretained(this)),
      base::Bind(&BrowserModule::GetSbWindow, base::Unretained(this)),
      can_play_type_handler_.get(), media_module_.get(), &network_module_,
      viewport_size, video_pixel_ratio, GetResourceProvider(),
      kLayoutMaxRefreshFrequencyInHz, options));
  lifecycle_observers_.AddObserver(web_module_.get());
  if (!web_module_recreated_callback_.is_null()) {
    web_module_recreated_callback_.Run();
  }
}

void BrowserModule::Reload() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Reload()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  web_module_->ExecuteJavascript(
      "location.reload();",
      base::SourceLocation("[object BrowserModule]", 1, 1),
      NULL /* output: succeeded */);
}

#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
// static
void BrowserModule::CoreDumpHandler(void* browser_module_as_void) {
  BrowserModule* browser_module =
      static_cast<BrowserModule*>(browser_module_as_void);
  SbCoreDumpLogInteger("BrowserModule.on_error_triggered_count_",
                       browser_module->on_error_triggered_count_);
#if defined(COBALT_CHECK_RENDER_TIMEOUT)
  SbCoreDumpLogInteger("BrowserModule.recovery_mechanism_triggered_count_",
                       browser_module->recovery_mechanism_triggered_count_);
  SbCoreDumpLogInteger("BrowserModule.timeout_response_trigger_count_",
                       browser_module->timeout_response_trigger_count_);
#endif  // defined(COBALT_CHECK_RENDER_TIMEOUT)
}
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)

void BrowserModule::OnLoad() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnLoad()");
  // Repost to our own message loop if necessary. This also prevents
  // asynchronous access to this object by |web_module_| during destruction.
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnLoad, weak_this_));
    return;
  }

  // This log is relied on by the webdriver benchmark tests, so it shouldn't be
  // changed unless the corresponding benchmark logic is changed as well.
  LOG(INFO) << "Loaded WebModule";

  // Clear |on_error_retry_count_| after a successful load.
  on_error_retry_count_ = 0;

  on_load_event_time_ = base::TimeTicks::Now().ToInternalValue();
  web_module_loaded_.Signal();
}

bool BrowserModule::WaitForLoad(const base::TimeDelta& timeout) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::WaitForLoad()");
  return web_module_loaded_.TimedWait(timeout);
}

#if defined(ENABLE_SCREENSHOT)
void BrowserModule::RequestScreenshotToFile(const FilePath& path,
                                            const base::Closure& done_cb) {
  if (screen_shot_writer_) {
    screen_shot_writer_->RequestScreenshot(path, done_cb);
  }
}

void BrowserModule::RequestScreenshotToBuffer(
    const ScreenShotWriter::PNGEncodeCompleteCallback&
        encode_complete_callback) {
  if (screen_shot_writer_) {
    screen_shot_writer_->RequestScreenshotToMemory(encode_complete_callback);
  }
}
#endif

void BrowserModule::ProcessRenderTreeSubmissionQueue() {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::ProcessRenderTreeSubmissionQueue()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  render_tree_submission_queue_.ProcessAll();
}

void BrowserModule::QueueOnRenderTreeProduced(
    int main_web_module_generation,
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::QueueOnRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnRenderTreeProduced, base::Unretained(this),
                 main_web_module_generation, layout_results));
  self_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::QueueOnSplashScreenRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::QueueOnSplashScreenRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnSplashScreenRenderTreeProduced,
                 base::Unretained(this), layout_results));
  self_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnRenderTreeProduced(
    int main_web_module_generation,
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnRenderTreeProduced()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);

  if (main_web_module_generation != main_web_module_generation_) {
    // Ignore render trees produced by old stale web modules.  This might happen
    // during a navigation transition.
    return;
  }

  if (splash_screen_ && !splash_screen_->ShutdownSignaled()) {
    splash_screen_->Shutdown();
  }
  if (application_state_ == base::kApplicationStatePreloading) {
    return;
  }

  renderer::Submission renderer_submission(layout_results.render_tree,
                                           layout_results.layout_time);
  // Set the timeline id for the main web module.  The main web module is
  // assumed to be an interactive experience for which the default timeline
  // configuration is already designed for, so we don't configure anything
  // explicitly.
  renderer_submission.timeline_info.id = current_main_web_module_timeline_id_;

  renderer_submission.on_rasterized_callback = base::Bind(
      &BrowserModule::OnRendererSubmissionRasterized, base::Unretained(this));
  if (!splash_screen_) {
    render_tree_combiner_.SetTimelineLayer(main_web_module_layer_.get());
  }
  main_web_module_layer_->Submit(renderer_submission);

#if defined(ENABLE_SCREENSHOT)
  if (screen_shot_writer_) {
    screen_shot_writer_->SetLastPipelineSubmission(renderer::Submission(
        layout_results.render_tree, layout_results.layout_time));
  }
#endif
  SubmitCurrentRenderTreeToRenderer();
}

void BrowserModule::OnSplashScreenRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnSplashScreenRenderTreeProduced()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);

  if (application_state_ == base::kApplicationStatePreloading ||
      !splash_screen_) {
    return;
  }

  renderer::Submission renderer_submission(layout_results.render_tree,
                                           layout_results.layout_time);
  // We customize some of the renderer pipeline timeline behavior to cater for
  // non-interactive splash screen playback.
  renderer_submission.timeline_info.id = current_splash_screen_timeline_id_;
  // Since the splash screen is non-interactive, latency is not a concern.
  // Latency reduction implies a speedup in animation playback speed which can
  // make the splash screen play out quicker than intended.
  renderer_submission.timeline_info.allow_latency_reduction = false;
  // Increase the submission queue size to a larger value than usual.  This
  // is done because a) since we do not attempt to reduce latency, the queue
  // tends to fill up more and b) the pipeline may end up receiving a number
  // of render tree submissions caused by updated main web module render trees,
  // which can fill the submission queue.  Blowing the submission queue is
  // particularly bad for the splash screen as it results in dropping of older
  // submissions, which results in skipping forward during animations, which
  // sucks.
  renderer_submission.timeline_info.max_submission_queue_size =
      std::max(8, renderer_submission.timeline_info.max_submission_queue_size);

  renderer_submission.on_rasterized_callback = base::Bind(
      &BrowserModule::OnRendererSubmissionRasterized, base::Unretained(this));
  render_tree_combiner_.SetTimelineLayer(splash_screen_layer_.get());
  splash_screen_layer_->Submit(renderer_submission);

#if defined(ENABLE_SCREENSHOT)
// TODO: write screen shot using render_tree_combiner_ (to combine
// splash screen and main web_module). Consider when the splash
// screen is overlaid on top of the main web module render tree, and
// a screenshot is taken : there will be a race condition on which
// web module update their render tree last.
#endif

  SubmitCurrentRenderTreeToRenderer();
}

void BrowserModule::OnWindowClose(base::TimeDelta close_time) {
  UNREFERENCED_PARAMETER(close_time);
#if defined(ENABLE_DEBUG_CONSOLE)
  if (input_device_manager_fuzzer_) {
    return;
  }
#endif

  SbSystemRequestStop(0);
}

void BrowserModule::OnWindowMinimize() {
#if defined(ENABLE_DEBUG_CONSOLE)
  if (input_device_manager_fuzzer_) {
    return;
  }
#endif

  SbSystemRequestSuspend();
}

#if SB_API_VERSION >= SB_WINDOW_SIZE_CHANGED_API_VERSION
void BrowserModule::OnWindowSizeChanged(const SbWindowSize& size) {
  math::Size math_size(size.width, size.height);
  if (web_module_) {
    web_module_->SetSize(math_size, size.video_pixel_ratio);
  }
#if defined(ENABLE_DEBUG_CONSOLE)
  if (debug_console_) {
    debug_console_->web_module().SetSize(math_size, size.video_pixel_ratio);
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)
  if (splash_screen_) {
    splash_screen_->web_module().SetSize(math_size, size.video_pixel_ratio);
  }

  return;
}
#endif  // SB_API_VERSION >= SB_WINDOW_SIZE_CHANGED_API_VERSION

#if SB_HAS(ON_SCREEN_KEYBOARD)
void BrowserModule::OnOnScreenKeyboardShown() {
  // Only inject shown events to the main WebModule.
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardShownEvent();
  }
}

void BrowserModule::OnOnScreenKeyboardHidden() {
  // Only inject hidden events to the main WebModule.
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardHiddenEvent();
  }
}
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

#if defined(ENABLE_DEBUG_CONSOLE)
void BrowserModule::OnFuzzerToggle(const std::string& message) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnFuzzerToggle, weak_this_, message));
    return;
  }

  if (!input_device_manager_fuzzer_) {
    // Wire up the input fuzzer key generator to the keyboard event callback.
    input_device_manager_fuzzer_ = scoped_ptr<input::InputDeviceManager>(
        new input::InputDeviceManagerFuzzer(
            base::Bind(&BrowserModule::InjectKeyEventToMainWebModule,
                       base::Unretained(this))));
  } else {
    input_device_manager_fuzzer_.reset();
  }
}

void BrowserModule::OnSetMediaConfig(const std::string& config) {
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnSetMediaConfig, weak_this_, config));
    return;
  }

  std::vector<std::string> tokens;
  base::SplitString(config, '=', &tokens);

  int value;
  if (tokens.size() != 2 || !base::StringToInt(tokens[1], &value)) {
    LOG(WARNING) << "Media configuration '" << config << "' is not in the"
                 << " form of '<string name>=<int value>'.";
    return;
  }
  if (media_module_->SetConfiguration(tokens[0], value)) {
    LOG(INFO) << "Successfully setting " << tokens[0] << " to " << value;
  } else {
    LOG(WARNING) << "Failed to set " << tokens[0] << " to " << value;
  }
}

void BrowserModule::QueueOnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::QueueOnDebugConsoleRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnDebugConsoleRenderTreeProduced,
                 base::Unretained(this), layout_results));
  self_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnDebugConsoleRenderTreeProduced()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  if (application_state_ == base::kApplicationStatePreloading) {
    return;
  }

  if (debug_console_->GetMode() == debug::DebugHub::kDebugConsoleOff) {
    debug_console_layer_->Submit(base::nullopt);
    return;
  }

  debug_console_layer_->Submit(renderer::Submission(
      layout_results.render_tree, layout_results.layout_time));

  SubmitCurrentRenderTreeToRenderer();
}

#endif  // defined(ENABLE_DEBUG_CONSOLE)

#if SB_HAS(ON_SCREEN_KEYBOARD)
void BrowserModule::OnOnScreenKeyboardInputEventProduced(
    base::Token type, const dom::InputEventInit& event) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnOnScreenKeyboardInputEventProduced()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnOnScreenKeyboardInputEventProduced,
                   weak_this_, type, event));
    return;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  // If the debug console is fully visible, it gets the next chance to handle
  // input events.
  if (debug_console_->GetMode() >= debug::DebugHub::kDebugConsoleOn) {
    if (!debug_console_->InjectOnScreenKeyboardInputEvent(type, event)) {
      return;
    }
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  InjectOnScreenKeyboardInputEventToMainWebModule(type, event);
}
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

void BrowserModule::OnKeyEventProduced(base::Token type,
                                       const dom::KeyboardEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnKeyEventProduced()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnKeyEventProduced, weak_this_,
                              type, event));
    return;
  }

  // Filter the key event.
  if (!FilterKeyEvent(type, event)) {
    return;
  }

  InjectKeyEventToMainWebModule(type, event);
}

void BrowserModule::OnPointerEventProduced(base::Token type,
                                           const dom::PointerEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnPointerEventProduced()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnPointerEventProduced,
                              weak_this_, type, event));
    return;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  trace_manager_.OnInputEventProduced();
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  DCHECK(web_module_);
  web_module_->InjectPointerEvent(type, event);
}

void BrowserModule::OnWheelEventProduced(base::Token type,
                                         const dom::WheelEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnWheelEventProduced()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnWheelEventProduced, weak_this_,
                              type, event));
    return;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  trace_manager_.OnInputEventProduced();
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  DCHECK(web_module_);
  web_module_->InjectWheelEvent(type, event);
}

void BrowserModule::InjectKeyEventToMainWebModule(
    base::Token type, const dom::KeyboardEventInit& event) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::InjectKeyEventToMainWebModule()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::InjectKeyEventToMainWebModule,
                              weak_this_, type, event));
    return;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  trace_manager_.OnInputEventProduced();
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  DCHECK(web_module_);
  web_module_->InjectKeyboardEvent(type, event);
}

#if SB_HAS(ON_SCREEN_KEYBOARD)
void BrowserModule::InjectOnScreenKeyboardInputEventToMainWebModule(
    base::Token type, const dom::InputEventInit& event) {
  TRACE_EVENT0(
      "cobalt::browser",
      "BrowserModule::InjectOnScreenKeyboardInputEventToMainWebModule()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(
            &BrowserModule::InjectOnScreenKeyboardInputEventToMainWebModule,
            weak_this_, type, event));
    return;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  trace_manager_.OnInputEventProduced();
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  DCHECK(web_module_);
  web_module_->InjectOnScreenKeyboardInputEvent(type, event);
}
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

void BrowserModule::OnError(const GURL& url, const std::string& error) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnError()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnError, weak_this_, url, error));
    return;
  }

#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
  on_error_triggered_count_++;
#endif

  // Set |pending_navigate_url_| to the url where the error occurred. This will
  // cause the OnError callback to Navigate() to this URL if it receives a
  // positive response; otherwise, if Cobalt is currently preloaded or
  // suspended, then this is the url that Cobalt will navigate to when it starts
  // or resumes.
  pending_navigate_url_ = url.spec();

  // Start the OnErrorRetry() timer if it isn't already running.
  // The minimum delay between calls to OnErrorRetry() exponentially grows as
  // |on_error_retry_count_| increases. |on_error_retry_count_| is reset when
  // OnLoad() is called.
  if (!on_error_retry_timer_.IsRunning()) {
    const int64 kBaseRetryDelayInMilliseconds = 1000;
    // Cap the max error shift at 10 (1024 * kBaseDelayInMilliseconds)
    // This results in the minimum delay being capped at ~17 minutes.
    const int kMaxOnErrorRetryCountShift = 10;
    int64 min_delay = kBaseRetryDelayInMilliseconds << std::min(
                          kMaxOnErrorRetryCountShift, on_error_retry_count_);
    int64 required_delay = std::max(
        min_delay -
            (base::TimeTicks::Now() - on_error_retry_time_).InMilliseconds(),
        static_cast<int64>(0));

    on_error_retry_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(required_delay), this,
        &BrowserModule::OnErrorRetry);
  }
}

void BrowserModule::OnErrorRetry() {
  ++on_error_retry_count_;
  on_error_retry_time_ = base::TimeTicks::Now();
  waiting_for_error_retry_ = true;
  TryURLHandlers(
      GURL("h5vcc://network-failure?retry-url=" + pending_navigate_url_));
}

bool BrowserModule::FilterKeyEvent(base::Token type,
                                   const dom::KeyboardEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::FilterKeyEvent()");
  // Check for hotkeys first. If it is a hotkey, no more processing is needed.
  if (!FilterKeyEventForHotkeys(type, event)) {
    return false;
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  // If the debug console is fully visible, it gets the next chance to handle
  // key events.
  if (debug_console_->GetMode() >= debug::DebugHub::kDebugConsoleOn) {
    if (!debug_console_->FilterKeyEvent(type, event)) {
      return false;
    }
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  return true;
}

bool BrowserModule::FilterKeyEventForHotkeys(
    base::Token type, const dom::KeyboardEventInit& event) {
#if !defined(ENABLE_DEBUG_CONSOLE)
  UNREFERENCED_PARAMETER(type);
  UNREFERENCED_PARAMETER(event);
#else
  if (event.key_code() == dom::keycode::kF1 ||
      (event.ctrl_key() && event.key_code() == dom::keycode::kO)) {
    if (type == base::Tokens::keydown()) {
      // Ctrl+O toggles the debug console display.
      debug_console_->CycleMode();
    }
    return false;
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  return true;
}

void BrowserModule::AddURLHandler(
    const URLHandler::URLHandlerCallback& callback) {
  url_handlers_.push_back(callback);
}

void BrowserModule::RemoveURLHandler(
    const URLHandler::URLHandlerCallback& callback) {
  for (URLHandlerCollection::iterator iter = url_handlers_.begin();
       iter != url_handlers_.end(); ++iter) {
    if (iter->Equals(callback)) {
      url_handlers_.erase(iter);
      return;
    }
  }
}

bool BrowserModule::TryURLHandlers(const GURL& url) {
  for (URLHandlerCollection::const_iterator iter = url_handlers_.begin();
       iter != url_handlers_.end(); ++iter) {
    if (iter->Run(url)) {
      return true;
    }
  }

  // No registered handler handled the URL, let the caller handle it.
  return false;
}

void BrowserModule::DestroySplashScreen(base::TimeDelta close_time) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::DestroySplashScreen()");
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::DestroySplashScreen, weak_this_,
                              close_time));
    return;
  }
  if (splash_screen_) {
    lifecycle_observers_.RemoveObserver(splash_screen_.get());
    if (!close_time.is_zero() && renderer_module_) {
      // Ensure that the renderer renders each frame up until the window.close()
      // is called on the splash screen's timeline, in order to ensure that the
      // splash screen shutdown transition plays out completely.
      renderer_module_->pipeline()->TimeFence(close_time);
    }
    splash_screen_layer_->Reset();
    SubmitCurrentRenderTreeToRenderer();
    splash_screen_.reset(NULL);
  }
}

#if defined(ENABLE_WEBDRIVER)
scoped_ptr<webdriver::SessionDriver> BrowserModule::CreateSessionDriver(
    const webdriver::protocol::SessionId& session_id) {
  return make_scoped_ptr(new webdriver::SessionDriver(
      session_id,
      base::Bind(&BrowserModule::CreateWindowDriver, base::Unretained(this)),
      base::Bind(&BrowserModule::WaitForLoad, base::Unretained(this))));
}

scoped_ptr<webdriver::WindowDriver> BrowserModule::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id) {
  // Repost to our message loop to ensure synchronous access to |web_module_|.
  scoped_ptr<webdriver::WindowDriver> window_driver;
  self_message_loop_->PostBlockingTask(
      FROM_HERE, base::Bind(&BrowserModule::CreateWindowDriverInternal,
                            base::Unretained(this), window_id,
                            base::Unretained(&window_driver)));

  // This log is relied on by the webdriver benchmark tests, so it shouldn't be
  // changed unless the corresponding benchmark logic is changed as well.
  LOG(INFO) << "Created WindowDriver: ID=" << window_id.id();
  DCHECK(window_driver);
  return window_driver.Pass();
}

void BrowserModule::CreateWindowDriverInternal(
    const webdriver::protocol::WindowId& window_id,
    scoped_ptr<webdriver::WindowDriver>* out_window_driver) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  *out_window_driver = web_module_->CreateWindowDriver(window_id);
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUG_CONSOLE)
debug::DebugServer* BrowserModule::GetDebugServer() {
  // Repost to our message loop to ensure synchronous access to |web_module_|.
  debug::DebugServer* debug_server = NULL;
  self_message_loop_->PostBlockingTask(
      FROM_HERE,
      base::Bind(&BrowserModule::GetDebugServerInternal, base::Unretained(this),
                 base::Unretained(&debug_server)));
  DCHECK(debug_server);
  return debug_server;
}

void BrowserModule::GetDebugServerInternal(
    debug::DebugServer** out_debug_server) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  *out_debug_server = web_module_->GetDebugServer();
}
#endif  // ENABLE_DEBUG_CONSOLE

void BrowserModule::SetProxy(const std::string& proxy_rules) {
  // NetworkModule will ensure this happens on the correct thread.
  network_module_.SetProxy(proxy_rules);
}

void BrowserModule::Start() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Start()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStatePreloading);

  SuspendInternal(true /*is_start*/);
  StartOrResumeInternalPreStateUpdate(true /*is_start*/);

  application_state_ = base::kApplicationStateStarted;

  StartOrResumeInternalPostStateUpdate();
}

void BrowserModule::Pause() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Pause()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateStarted);
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Pause());
  application_state_ = base::kApplicationStatePaused;
}

void BrowserModule::Unpause() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Unpause()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStatePaused);
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Unpause());
  application_state_ = base::kApplicationStateStarted;
}

void BrowserModule::Suspend() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Suspend()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStatePaused ||
         application_state_ == base::kApplicationStatePreloading);

  SuspendInternal(false /*is_start*/);

  application_state_ = base::kApplicationStateSuspended;
}

void BrowserModule::Resume() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Resume()");
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateSuspended);

  StartOrResumeInternalPreStateUpdate(false /*is_start*/);

  application_state_ = base::kApplicationStatePaused;

  StartOrResumeInternalPostStateUpdate();
}

void BrowserModule::ReduceMemory() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  if (splash_screen_) {
    splash_screen_->ReduceMemory();
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  if (debug_console_) {
    debug_console_->ReduceMemory();
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  if (web_module_) {
    web_module_->ReduceMemory();
  }
}

void BrowserModule::CheckMemory(
    const int64_t& used_cpu_memory,
    const base::optional<int64_t>& used_gpu_memory) {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  if (!auto_mem_) {
    return;
  }

  memory_settings_checker_.RunChecks(*auto_mem_, used_cpu_memory,
                                     used_gpu_memory);
}

void BrowserModule::OnRendererSubmissionRasterized() {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnRendererSubmissionRasterized()");
  if (!is_rendered_) {
    // Hide the system splash screen when the first render has completed.
    is_rendered_ = true;
    SbSystemHideSplashScreen();
  }
}

#if defined(COBALT_CHECK_RENDER_TIMEOUT)
void BrowserModule::OnPollForRenderTimeout(const GURL& url) {
  SbTime last_render_timestamp = static_cast<SbTime>(SbAtomicAcquire_Load64(
      non_trivial_global_variables.Get().last_render_timestamp));
  base::Time last_render = base::Time::FromSbTime(last_render_timestamp);
  bool timeout_expiration = base::Time::Now() - base::TimeDelta::FromSeconds(
                                                    kLastRenderTimeoutSeconds) >
                            last_render;
  bool timeout_response_trigger = false;
  if (timeout_expiration) {
    // The timeout only triggers if the timeout expiration has been detected
    // without interruption at least kMinimumContinuousRenderTimeoutExpirations
    // times.
    ++render_timeout_count_;
    timeout_response_trigger =
        render_timeout_count_ >= kMinimumContinuousRenderTimeoutExpirations;
  } else {
    render_timeout_count_ = 0;
  }

  if (timeout_response_trigger) {
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
    timeout_response_trigger_count_++;
#endif
    SbAtomicNoBarrier_Exchange64(
        non_trivial_global_variables.Get().last_render_timestamp,
        static_cast<SbAtomic64>(kSbTimeMax));
    if (SbSystemGetRandomUInt64() <
        kRenderTimeoutErrorPercentage * (UINT64_MAX / 100)) {
      OnError(url, std::string("Rendering Timeout"));
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
      recovery_mechanism_triggered_count_++;
#endif
    } else {
      SB_DLOG(INFO) << "Received OnRenderTimeout, ignoring by random chance.";
    }
  } else {
    timeout_polling_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnPollForRenderTimeout,
                   base::Unretained(this), url),
        base::TimeDelta::FromSeconds(kRenderTimeOutPollingDelaySeconds));
  }
}
#endif

render_tree::ResourceProvider* BrowserModule::GetResourceProvider() {
  if (!renderer_module_) {
    if (resource_provider_stub_) {
      DCHECK(application_state_ == base::kApplicationStatePreloading);
      return &(resource_provider_stub_.value());
    }

    return NULL;
  }

  return renderer_module_->resource_provider();
}

void BrowserModule::InitializeSystemWindow() {
  resource_provider_stub_ = base::nullopt;
  DCHECK(!system_window_);
  system_window_.reset(new system_window::SystemWindow(
      event_dispatcher_, options_.requested_viewport_size));
  auto_mem_.reset(new memory_settings::AutoMem(
      GetViewportSize(), options_.command_line_auto_mem_settings,
      options_.build_auto_mem_settings));
  ApplyAutoMemSettings();

  input_device_manager_ =
      input::InputDeviceManager::CreateFromWindow(
          base::Bind(&BrowserModule::OnKeyEventProduced,
                     base::Unretained(this)),
          base::Bind(&BrowserModule::OnPointerEventProduced,
                     base::Unretained(this)),
          base::Bind(&BrowserModule::OnWheelEventProduced,
                     base::Unretained(this)),
#if SB_HAS(ON_SCREEN_KEYBOARD)
          base::Bind(&BrowserModule::OnOnScreenKeyboardInputEventProduced,
                     base::Unretained(this)),
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
          system_window_.get())
          .Pass();
  InstantiateRendererModule();

  media_module_ =
      media::MediaModule::Create(system_window_.get(), GetResourceProvider(),
                                 options_.media_module_options);
}

void BrowserModule::InstantiateRendererModule() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(system_window_);
  DCHECK(!renderer_module_);

  renderer_module_.reset(new renderer::RendererModule(
      system_window_.get(),
      RendererModuleWithCameraOptions(options_.renderer_module_options,
                                      input_device_manager_->camera_3d())));
#if defined(ENABLE_SCREENSHOT)
  screen_shot_writer_.reset(new ScreenShotWriter(renderer_module_->pipeline()));
#endif  // defined(ENABLE_SCREENSHOT)
}

void BrowserModule::DestroyRendererModule() {
  DCHECK_EQ(MessageLoop::current(), self_message_loop_);
  DCHECK(renderer_module_);

#if defined(ENABLE_SCREENSHOT)
  screen_shot_writer_.reset();
#endif  // defined(ENABLE_SCREENSHOT)
  renderer_module_.reset();
}

void BrowserModule::UpdateFromSystemWindow() {
  math::Size size = GetViewportSize();
  float video_pixel_ratio = system_window_->GetVideoPixelRatio();
#if defined(ENABLE_DEBUG_CONSOLE)
  if (debug_console_) {
    debug_console_->SetSize(size, video_pixel_ratio);
  }
#endif  // defined(ENABLE_DEBUG_CONSOLE)

  if (splash_screen_) {
    splash_screen_->SetSize(size, video_pixel_ratio);
  }

  if (web_module_) {
    web_module_->SetCamera3D(input_device_manager_->camera_3d());
    web_module_->SetWebMediaPlayerFactory(media_module_.get());
    web_module_->SetSize(size, video_pixel_ratio);
  }
}

void BrowserModule::SuspendInternal(bool is_start) {
  TRACE_EVENT1("cobalt::browser", "BrowserModule::SuspendInternal", "is_start",
               is_start ? "true" : "false");
  // First suspend all our web modules which implies that they will release
  // their resource provider and all resources created through it.
  if (is_start) {
    FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Prestart());
  } else {
    FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Suspend());
  }

  // Flush out any submitted render trees pushed since we started shutting down
  // the web modules above.
  render_tree_submission_queue_.ProcessAll();

#if defined(ENABLE_SCREENSHOT)
  // The screenshot writer may be holding on to a reference to a render tree
  // which could in turn be referencing resources like images, so clear that
  // out.
  if (screen_shot_writer_) {
    screen_shot_writer_->ClearLastPipelineSubmission();
  }
#endif

  // Clear out the render tree combiner so that it doesn't hold on to any
  // render tree resources either.
  main_web_module_layer_->Reset();
  splash_screen_layer_->Reset();
#if defined(ENABLE_DEBUG_CONSOLE)
  debug_console_layer_->Reset();
#endif  // defined(ENABLE_DEBUG_CONSOLE)

#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
  // Note that the following function call will leak the GPU memory allocated.
  // This is because after the renderer_module_ is destroyed it is no longer
  // safe to release the GPU memory allocated.
  //
  // The following code can call reset() to release the allocated memory but the
  // memory may still be used by XHR and ArrayBuffer.  As this feature is only
  // used on platform without Resume() support, it is safer to leak the memory
  // then to release it.
  dom::ArrayBuffer::Allocator* allocator = array_buffer_allocator_.release();
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)

  if (media_module_) {
    media_module_->Suspend();
  }

  if (renderer_module_) {
    // Destroy the renderer module into so that it releases all its graphical
    // resources.
    DestroyRendererModule();
  }
}

void BrowserModule::StartOrResumeInternalPreStateUpdate(bool is_start) {
  TRACE_EVENT1("cobalt::browser",
               "BrowserModule::StartOrResumeInternalPreStateUpdate", "is_start",
               is_start ? "true" : "false");
  if (!system_window_) {
    InitializeSystemWindow();
    UpdateFromSystemWindow();
  } else {
    InstantiateRendererModule();
    media_module_->Resume(GetResourceProvider());
  }

#if defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)
  // Start() and Resume() are not supported on platforms that allocate
  // ArrayBuffers in GPU memory.
  NOTREACHED();
#endif  // defined(ENABLE_GPU_ARRAY_BUFFER_ALLOCATOR)

  if (is_start) {
    FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_,
                      Start(GetResourceProvider()));
  } else {
    FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_,
                      Resume(GetResourceProvider()));
  }
}

void BrowserModule::StartOrResumeInternalPostStateUpdate() {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::StartOrResumeInternalPostStateUpdate");
  // If there's a navigation that's pending, then attempt to navigate to its
  // specified URL now, unless we're still waiting for an error retry.
  if (!pending_navigate_url_.empty() && !waiting_for_error_retry_) {
    Navigate(GURL(pending_navigate_url_));
  }
}

math::Size BrowserModule::GetViewportSize() {
  // We trust the renderer module the most, if it exists.
  if (renderer_module_) {
    return renderer_module_->render_target_size();
  }

  // If the system window exists, that's almost just as good.
  if (system_window_) {
    return system_window_->GetWindowSize();
  }

  // Otherwise, we assume we'll get the viewport size that was requested.
  if (options_.requested_viewport_size) {
    return *options_.requested_viewport_size;
  }

  // TODO: Allow platforms to define the default window size and return that
  // here.

  // No window and no viewport size was requested, so we return a conservative
  // default.
  return math::Size(1280, 720);
}

void BrowserModule::ApplyAutoMemSettings() {
  LOG(INFO) << "\n\n" << auto_mem_->ToPrettyPrintString(SbLogIsTty()) << "\n\n";

  // Web Module options.
  options_.web_module_options.image_cache_capacity =
      static_cast<int>(auto_mem_->image_cache_size_in_bytes()->value());
  options_.web_module_options.remote_typeface_cache_capacity = static_cast<int>(
      auto_mem_->remote_typeface_cache_size_in_bytes()->value());
  options_.web_module_options.javascript_engine_options.gc_threshold_bytes =
      static_cast<size_t>(
          auto_mem_->javascript_gc_threshold_in_bytes()->value());
  if (web_module_) {
    web_module_->SetImageCacheCapacity(
        auto_mem_->image_cache_size_in_bytes()->value());
    web_module_->SetRemoteTypefaceCacheCapacity(
        auto_mem_->remote_typeface_cache_size_in_bytes()->value());
    web_module_->SetJavascriptGcThreshold(
        auto_mem_->javascript_gc_threshold_in_bytes()->value());
  }

  // Renderer Module options.
  options_.renderer_module_options.skia_cache_size_in_bytes =
      static_cast<int>(auto_mem_->skia_cache_size_in_bytes()->value());
  options_.renderer_module_options.software_surface_cache_size_in_bytes =
      static_cast<int>(
          auto_mem_->software_surface_cache_size_in_bytes()->value());
  options_.renderer_module_options.offscreen_target_cache_size_in_bytes =
      static_cast<int>(
          auto_mem_->offscreen_target_cache_size_in_bytes()->value());

  const memory_settings::TextureDimensions skia_glyph_atlas_texture_dimensions =
      auto_mem_->skia_atlas_texture_dimensions()->value();
  if (skia_glyph_atlas_texture_dimensions.bytes_per_pixel() > 0) {
    // Right now the bytes_per_pixel is assumed in the engine. Any other value
    // is currently forbidden.
    DCHECK_EQ(2, skia_glyph_atlas_texture_dimensions.bytes_per_pixel());

    options_.renderer_module_options.skia_glyph_texture_atlas_dimensions =
        math::Size(skia_glyph_atlas_texture_dimensions.width(),
                   skia_glyph_atlas_texture_dimensions.height());
  }
}

void BrowserModule::SubmitCurrentRenderTreeToRenderer() {
  if (!renderer_module_) {
    return;
  }

  base::optional<renderer::Submission> submission =
      render_tree_combiner_.GetCurrentSubmission();
  if (submission) {
    renderer_module_->pipeline()->Submit(*submission);
  }
}

SbWindow BrowserModule::GetSbWindow() {
  if (!system_window_) {
    return NULL;
  }
  return system_window_->GetSbWindow();
}

}  // namespace browser
}  // namespace cobalt
