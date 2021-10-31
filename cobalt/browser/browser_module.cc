// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/tokens.h"
#include "cobalt/browser/on_screen_keyboard_starboard_bridge.h"
#include "cobalt/browser/screen_shot_writer.h"
#include "cobalt/browser/switches.h"
#include "cobalt/browser/webapi_extension.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/keycode.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/navigator_ua_data.h"
#include "cobalt/dom/window.h"
#include "cobalt/extension/graphics.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/input/input_device_manager_fuzzer.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/overlay_info/overlay_info_registry.h"
#include "nb/memory_scope.h"
#include "starboard/atomic.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#include "base/memory/ptr_util.h"
#include STARBOARD_CORE_DUMP_HANDLER_INCLUDE
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)

using cobalt::cssom::ViewportSize;

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

base::LazyInstance<NonTrivialGlobalVariables>::DestructorAtExit
    non_trivial_global_variables = LAZY_INSTANCE_INITIALIZER;
}  // namespace
#endif  // defined(COBALT_CHECK_RENDER_TIMEOUT)

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
#if defined(ENABLE_DEBUGGER)
const int kDebugConsoleZIndex = 3;
#endif  // defined(ENABLE_DEBUGGER)
const int kOverlayInfoZIndex = 4;

#if defined(ENABLE_DEBUGGER)

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

const char kScreenshotCommand[] = "screenshot";
const char kScreenshotCommandShortHelp[] = "Takes a screenshot.";
const char kScreenshotCommandLongHelp[] =
    "Creates a screenshot of the most recent layout tree and writes it "
    "to disk. Logs the filename of the screenshot to the console when done.";

// Debug console command `disable_media_codecs` for disabling a list of codecs
// by treating them as unsupported
const char kDisableMediaCodecsCommand[] = "disable_media_codecs";
const char kDisableMediaCodecsCommandShortHelp[] =
    "Specify a semicolon-separated list of disabled media codecs.";
const char kDisableMediaCodecsCommandLongHelp[] =
    "Disabling Media Codecs will force the app to claim they are not "
    "supported. This "
    "is useful when trying to target testing to certain codecs, since other "
    "codecs will get picked as a fallback as a result.";

void ScreenshotCompleteCallback(const base::FilePath& output_path) {
  DLOG(INFO) << "Screenshot written to " << output_path.value();
}

void OnScreenshotMessage(BrowserModule* browser_module,
                         const std::string& message) {
  base::FilePath dir;
  if (!base::PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &dir)) {
    NOTREACHED() << "Failed to get debug out directory.";
  }

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  DCHECK(exploded.HasValidValues());
  std::string screenshot_file_name =
      base::StringPrintf("screenshot-%04d-%02d-%02d_%02d-%02d-%02d.png",
                         exploded.year, exploded.month, exploded.day_of_month,
                         exploded.hour, exploded.minute, exploded.second);

  base::FilePath output_path = dir.Append(screenshot_file_name);
  browser_module->RequestScreenshotToFile(
      output_path, loader::image::EncodedStaticImage::ImageFormat::kPNG,
      /*clip_rect=*/base::nullopt,
      base::Bind(&ScreenshotCompleteCallback, output_path));
}

#endif  // defined(ENABLE_DEBUGGER)

#if SB_API_VERSION < 12
scoped_refptr<script::Wrappable> CreateExtensionInterface(
    const scoped_refptr<dom::Window>& window,
    script::GlobalEnvironment* global_environment) {
  return CreateWebAPIExtensionObject(window, global_environment);
}
#endif

renderer::RendererModule::Options RendererModuleWithCameraOptions(
    renderer::RendererModule::Options options,
    scoped_refptr<input::Camera3D> camera_3d) {
  options.get_camera_transform = base::Bind(
      &input::Camera3D::GetCameraTransformAndUpdateOrientation, camera_3d);
  return options;  // Copy.
}

renderer::Submission CreateSubmissionFromLayoutResults(
    const browser::WebModule::LayoutResults& layout_results) {
  renderer::Submission renderer_submission(layout_results.render_tree,
                                           layout_results.layout_time);
  if (!layout_results.on_rasterized_callback.is_null()) {
    renderer_submission.on_rasterized_callbacks.push_back(
        layout_results.on_rasterized_callback);
  }
  return renderer_submission;
}

}  // namespace

BrowserModule::BrowserModule(const GURL& url,
                             base::ApplicationState initial_application_state,
                             base::EventDispatcher* event_dispatcher,
                             account::AccountManager* account_manager,
                             network::NetworkModule* network_module,
#if SB_IS(EVERGREEN)
                             updater::UpdaterModule* updater_module,
#endif
                             const Options& options)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      options_(options),
      self_message_loop_(base::MessageLoop::current()),
      event_dispatcher_(event_dispatcher),
      account_manager_(account_manager),
      is_rendered_(false),
      can_play_type_handler_(media::MediaModule::CreateCanPlayTypeHandler()),
      network_module_(network_module),
#if SB_IS(EVERGREEN)
      updater_module_(updater_module),
#endif
      splash_screen_cache_(new SplashScreenCache()),
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
      on_screen_keyboard_bridge_(
          OnScreenKeyboardStarboardBridge::IsSupported() &&
                  options.enable_on_screen_keyboard
              ? new OnScreenKeyboardStarboardBridge(base::Bind(
                    &BrowserModule::GetSbWindow, base::Unretained(this)))
              : NULL),
#endif  // SB_API_VERSION >= 12 ||
      // SB_HAS(ON_SCREEN_KEYBOARD)
      web_module_loaded_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
      web_module_created_callback_(options_.web_module_created_callback),
      navigate_time_("Time.Browser.Navigate", 0,
                     "The last time a navigation occurred."),
      on_load_event_time_("Time.Browser.OnLoadEvent", 0,
                          "The last time the window.OnLoad event fired."),
      javascript_reserved_memory_(
          "Memory.JS", 0,
          "The total memory that is reserved by the JavaScript engine, which "
          "includes both parts that have live JavaScript values, as well as "
          "preallocated space for future values."),
      disabled_media_codecs_(
          "Media.DisabledMediaCodecs", "",
          "List of codecs that should currently be reported as unsupported."),
#if defined(ENABLE_DEBUGGER)
      ALLOW_THIS_IN_INITIALIZER_LIST(fuzzer_toggle_command_handler_(
          kFuzzerToggleCommand,
          base::Bind(&BrowserModule::OnFuzzerToggle, base::Unretained(this)),
          kFuzzerToggleCommandShortHelp, kFuzzerToggleCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(set_media_config_command_handler_(
          kSetMediaConfigCommand,
          base::Bind(&BrowserModule::OnSetMediaConfig, base::Unretained(this)),
          kSetMediaConfigCommandShortHelp, kSetMediaConfigCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(screenshot_command_handler_(
          kScreenshotCommand,
          base::Bind(&OnScreenshotMessage, base::Unretained(this)),
          kScreenshotCommandShortHelp, kScreenshotCommandLongHelp)),
      ALLOW_THIS_IN_INITIALIZER_LIST(disable_media_codecs_command_handler_(
          kDisableMediaCodecsCommand,
          base::Bind(&BrowserModule::OnDisableMediaCodecs,
                     base::Unretained(this)),
          kDisableMediaCodecsCommandShortHelp,
          kDisableMediaCodecsCommandLongHelp)),
#endif  // defined(ENABLE_DEBUGGER)
      has_resumed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
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
  TRACE_EVENT0("cobalt::browser", "BrowserModule::BrowserModule()");

  // Apply platform memory setting adjustments and defaults.
  ApplyAutoMemSettings();

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
  timeout_polling_thread_.message_loop()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BrowserModule::OnPollForRenderTimeout, base::Unretained(this),
                 url),
      base::TimeDelta::FromSeconds(kRenderTimeOutPollingDelaySeconds));
#endif

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Create the main web module layer.
  main_web_module_layer_ =
      render_tree_combiner_.CreateLayer(kMainWebModuleZIndex);
  // Create the splash screen layer.
  splash_screen_layer_ = render_tree_combiner_.CreateLayer(kSplashScreenZIndex);
// Create the debug console layer.
#if defined(ENABLE_DEBUGGER)
  debug_console_layer_ = render_tree_combiner_.CreateLayer(kDebugConsoleZIndex);
#endif

  int qr_code_overlay_slots = 4;
  if (command_line->HasSwitch(switches::kQrCodeOverlay)) {
    auto slots_in_string =
        command_line->GetSwitchValueASCII(switches::kQrCodeOverlay);
    if (!slots_in_string.empty()) {
      auto result = base::StringToInt(slots_in_string, &qr_code_overlay_slots);
      DCHECK(result) << "Failed to convert value of --"
                     << switches::kQrCodeOverlay << ": "
                     << qr_code_overlay_slots << " to int.";
      DCHECK_GT(qr_code_overlay_slots, 0);
    }
    qr_overlay_info_layer_ =
        render_tree_combiner_.CreateLayer(kOverlayInfoZIndex);
  } else {
    overlay_info::OverlayInfoRegistry::Disable();
  }

  // Setup our main web module to have the H5VCC API injected into it.
  DCHECK(!ContainsKey(options_.web_module_options.injected_window_attributes,
                      "h5vcc"));
  options_.web_module_options.injected_window_attributes["h5vcc"] =
      base::Bind(&BrowserModule::CreateH5vcc, base::Unretained(this));

  if (command_line->HasSwitch(switches::kDisableTimerResolutionLimit)) {
    options_.web_module_options.limit_performance_timer_resolution = false;
  }

  options_.web_module_options.enable_inline_script_warnings =
      !command_line->HasSwitch(switches::kSilenceInlineScriptWarnings);

#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  options_.web_module_options.enable_partial_layout =
      !command_line->HasSwitch(switches::kDisablePartialLayout);
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)

#if SB_API_VERSION < 12
  base::Optional<std::string> extension_object_name =
      GetWebAPIExtensionObjectPropertyName();
  if (extension_object_name) {
    options_.web_module_options
        .injected_window_attributes[*extension_object_name] =
        base::Bind(&CreateExtensionInterface);
  }
#endif

#if defined(ENABLE_DEBUGGER) && defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(switches::kInputFuzzer)) {
    OnFuzzerToggle(std::string());
  }
  if (command_line->HasSwitch(switches::kSuspendFuzzer)) {
    suspend_fuzzer_.emplace();
  }
#endif  // ENABLE_DEBUGGER && ENABLE_DEBUG_COMMAND_LINE_SWITCHES

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(switches::kDisableMediaCodecs)) {
    std::string codecs =
        command_line->GetSwitchValueASCII(switches::kDisableMediaCodecs);
    if (!codecs.empty()) {
#if defined(ENABLE_DEBUGGER)
      OnDisableMediaCodecs(codecs);
#else   // ENABLE_DEBUGGER
      // Here command line switches are enabled but the debug console is not.
      can_play_type_handler_->SetDisabledMediaCodecs(codecs);
#endif  // ENABLE_DEBUGGER
    }
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  if (application_state_ == base::kApplicationStateStarted ||
      application_state_ == base::kApplicationStateBlurred) {
    InitializeSystemWindow();
  }

  resource_provider_stub_.emplace(true /*allocate_image_data*/);

#if defined(ENABLE_DEBUGGER)
  debug_console_.reset(new DebugConsole(
      application_state_,
      base::Bind(&BrowserModule::QueueOnDebugConsoleRenderTreeProduced,
                 base::Unretained(this)),
      network_module_, GetViewportSize(), GetResourceProvider(),
      kLayoutMaxRefreshFrequencyInHz,
      base::Bind(&BrowserModule::CreateDebugClient, base::Unretained(this)),
      base::Bind(&BrowserModule::OnMaybeFreeze, base::Unretained(this))));
  lifecycle_observers_.AddObserver(debug_console_.get());
#endif  // defined(ENABLE_DEBUGGER)

  const renderer::Pipeline* pipeline =
      renderer_module_ ? renderer_module_->pipeline() : nullptr;
  if (command_line->HasSwitch(switches::kDisableMapToMesh) ||
      !renderer::Pipeline::IsMapToMeshEnabled(pipeline)) {
    options_.web_module_options.enable_map_to_mesh = false;
  }

  if (qr_overlay_info_layer_) {
    math::Size width_height = GetViewportSize().width_height();
    qr_code_overlay_.reset(new overlay_info::QrCodeOverlay(
        width_height, qr_code_overlay_slots, GetResourceProvider(),
        base::Bind(&BrowserModule::QueueOnQrCodeOverlayRenderTreeProduced,
                   base::Unretained(this))));
  }

  // Set the fallback splash screen url to the default fallback url.
  fallback_splash_screen_url_ = options.fallback_splash_screen_url;

  // Synchronously construct our WebModule object.
  Navigate(url);
  DCHECK(web_module_);
}

BrowserModule::~BrowserModule() {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);

  // Transition into the suspended state from whichever state we happen to
  // currently be in, to prepare for shutdown.
  switch (application_state_) {
    case base::kApplicationStateStarted:
      Blur(0);
    // Intentional fall-through.
    case base::kApplicationStateBlurred:
      Conceal(0);
    case base::kApplicationStateConcealed:
      Freeze(0);
      break;
    case base::kApplicationStateStopped:
      NOTREACHED() << "BrowserModule does not support the stopped state.";
      break;
    case base::kApplicationStateFrozen:
      break;
  }

  on_error_retry_timer_.Stop();
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
  SbCoreDumpUnregisterHandler(BrowserModule::CoreDumpHandler, this);
#endif
}

void BrowserModule::Navigate(const GURL& url_reference) {
  // The argument is sometimes |pending_navigate_url_|, and Navigate can modify
  // |pending_navigate_url_|, so we want to keep a copy of the argument to
  // preserve its original value.
  GURL url = url_reference;
  DLOG(INFO) << "In BrowserModule::Navigate " << url;
  TRACE_EVENT1("cobalt::browser", "BrowserModule::Navigate()", "url",
               url.spec());
  // Reset the waitable event regardless of the thread. This ensures that the
  // webdriver won't incorrectly believe that the webmodule has finished loading
  // when it calls Navigate() and waits for the |web_module_loaded_| signal.
  web_module_loaded_.Reset();

  // Repost to our own message loop if necessary.
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::Navigate, weak_this_, url));
    return;
  }

  // First try any registered handlers. If one of these handles the URL, we
  // don't use the web module.
  if (TryURLHandlers(url)) {
    return;
  }

  // Clear error handling once we're told to navigate, either because it's the
  // retry from the error or something decided we should navigate elsewhere.
  on_error_retry_timer_.Stop();
  waiting_for_error_retry_ = false;

  // Navigations aren't allowed if the app is frozen. If this is the case,
  // simply set the pending navigate url, which will cause the navigation to
  // occur when Cobalt resumes, and return.
  if (application_state_ == base::kApplicationStateFrozen) {
    pending_navigate_url_ = url;
    return;
  }

  // Now that we know the navigation is occurring, clear out
  // |pending_navigate_url_|.
  pending_navigate_url_ = GURL::EmptyGURL();

#if defined(ENABLE_DEBUGGER)
  // Save the debugger state to be restored in the new WebModule.
  std::unique_ptr<debug::backend::DebuggerState> debugger_state;
  if (web_module_) {
    debugger_state = web_module_->FreezeDebugger();
  }
#endif  // defined(ENABLE_DEBUGGER)

  // Destroy old WebModule first, so we don't get a memory high-watermark after
  // the second WebModule's constructor runs, but before
  // std::unique_ptr::reset() is run.
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
  navigate_time_ = base::TimeTicks::Now().ToInternalValue();

  // Show a splash screen while we're waiting for the web page to load.
  const ViewportSize viewport_size = GetViewportSize();

  DestroySplashScreen(base::TimeDelta());
  if (options_.enable_splash_screen_on_reloads ||
      main_web_module_generation_ == 1) {
    base::Optional<std::string> topic = SetSplashScreenTopicFallback(url);
    splash_screen_cache_->SetUrl(url, topic);

    if (fallback_splash_screen_url_ ||
        splash_screen_cache_->IsSplashScreenCached()) {
      splash_screen_.reset(new SplashScreen(
          application_state_,
          base::Bind(&BrowserModule::QueueOnSplashScreenRenderTreeProduced,
                     base::Unretained(this)),
          network_module_, viewport_size, GetResourceProvider(),
          kLayoutMaxRefreshFrequencyInHz, fallback_splash_screen_url_,
          splash_screen_cache_.get(),
          base::Bind(&BrowserModule::DestroySplashScreen, weak_this_),
          base::Bind(&BrowserModule::OnMaybeFreeze, base::Unretained(this))));
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
#if defined(ENABLE_FAKE_MICROPHONE)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFakeMicrophone)) {
    options.dom_settings_options.microphone_options.enable_fake_microphone =
        true;
  }
#endif  // defined(ENABLE_FAKE_MICROPHONE)
  if (on_screen_keyboard_bridge_) {
    options.on_screen_keyboard_bridge = on_screen_keyboard_bridge_.get();
  }
  options.image_cache_capacity_multiplier_when_playing_video =
      configuration::Configuration::GetInstance()
          ->CobaltImageCacheCapacityMultiplierWhenPlayingVideo();
  if (input_device_manager_) {
    options.camera_3d = input_device_manager_->camera_3d();
  }

  // Make sure that automem has been run before creating the WebModule, so that
  // we use properly configured options for all parameters.
  DCHECK(auto_mem_);

  options.provide_screenshot_function =
      base::Bind(&ScreenShotWriter::RequestScreenshotToMemoryUnencoded,
                 base::Unretained(screen_shot_writer_.get()));

#if defined(ENABLE_DEBUGGER)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWaitForWebDebugger)) {
    int wait_for_generation =
        atoi(base::CommandLine::ForCurrentProcess()
                 ->GetSwitchValueASCII(switches::kWaitForWebDebugger)
                 .c_str());
    if (wait_for_generation < 1) wait_for_generation = 1;
    options.wait_for_web_debugger =
        (wait_for_generation == main_web_module_generation_);
  }

  options.debugger_state = debugger_state.get();
#endif  // ENABLE_DEBUGGER

  // Pass down this callback from to Web module.
  options.maybe_freeze_callback =
      base::Bind(&BrowserModule::OnMaybeFreeze, base::Unretained(this));

  web_module_.reset(new WebModule(
      url, application_state_,
      base::Bind(&BrowserModule::QueueOnRenderTreeProduced,
                 base::Unretained(this), main_web_module_generation_),
      base::Bind(&BrowserModule::OnError, base::Unretained(this)),
      base::Bind(&BrowserModule::OnWindowClose, base::Unretained(this)),
      base::Bind(&BrowserModule::OnWindowMinimize, base::Unretained(this)),
      can_play_type_handler_.get(), media_module_.get(), network_module_,
      viewport_size, GetResourceProvider(), kLayoutMaxRefreshFrequencyInHz,
      options));
  lifecycle_observers_.AddObserver(web_module_.get());
  if (!web_module_created_callback_.is_null()) {
    web_module_created_callback_.Run();
  }

  if (system_window_) {
    web_module_->GetUiNavRoot()->SetContainerWindow(
        system_window_->GetSbWindow());
  }
}

void BrowserModule::Reload() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Reload()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
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
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
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

void BrowserModule::RequestScreenshotToFile(
    const base::FilePath& path,
    loader::image::EncodedStaticImage::ImageFormat image_format,
    const base::Optional<math::Rect>& clip_rect,
    const base::Closure& done_callback) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::RequestScreenshotToFile()");
  DCHECK(screen_shot_writer_);

  scoped_refptr<render_tree::Node> render_tree =
      web_module_->DoSynchronousLayoutAndGetRenderTree();
  if (!render_tree) {
    LOG(WARNING) << "Unable to get animated render tree";
    return;
  }

  screen_shot_writer_->RequestScreenshotToFile(image_format, path, render_tree,
                                               clip_rect, done_callback);
}

void BrowserModule::RequestScreenshotToMemory(
    loader::image::EncodedStaticImage::ImageFormat image_format,
    const base::Optional<math::Rect>& clip_rect,
    const ScreenShotWriter::ImageEncodeCompleteCallback& screenshot_ready) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::RequestScreenshotToMemory()");
  DCHECK(screen_shot_writer_);

  scoped_refptr<render_tree::Node> render_tree =
      web_module_->DoSynchronousLayoutAndGetRenderTree();
  if (!render_tree) {
    LOG(WARNING) << "Unable to get animated render tree";
    return;
  }

  screen_shot_writer_->RequestScreenshotToMemory(image_format, render_tree,
                                                 clip_rect, screenshot_ready);
}

void BrowserModule::ProcessRenderTreeSubmissionQueue() {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::ProcessRenderTreeSubmissionQueue()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  // If the app is preloaded, clear the render tree queue to avoid unnecessary
  // rendering overhead.
  if (application_state_ == base::kApplicationStateConcealed) {
    render_tree_submission_queue_.ClearAll();
  } else {
    render_tree_submission_queue_.ProcessAll();
  }
}

void BrowserModule::QueueOnRenderTreeProduced(
    int main_web_module_generation,
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::QueueOnRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnRenderTreeProduced, base::Unretained(this),
                 main_web_module_generation, layout_results));
  self_message_loop_->task_runner()->PostTask(
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
  self_message_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnRenderTreeProduced(
    int main_web_module_generation,
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnRenderTreeProduced()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);

  if (main_web_module_generation != main_web_module_generation_) {
    // Ignore render trees produced by old stale web modules.  This might happen
    // during a navigation transition.
    return;
  }

  if (splash_screen_) {
    if (on_screen_keyboard_show_called_) {
      // Hide the splash screen as quickly as possible.
      DestroySplashScreen(base::TimeDelta());
    } else if (!splash_screen_->ShutdownSignaled()) {
      splash_screen_->Shutdown();
    }
  }
  if (application_state_ == base::kApplicationStateConcealed) {
    layout_results.on_rasterized_callback.Run();
    return;
  }

  renderer::Submission renderer_submission(
      CreateSubmissionFromLayoutResults(layout_results));

  // Set the timeline id for the main web module.  The main web module is
  // assumed to be an interactive experience for which the default timeline
  // configuration is already designed for, so we don't configure anything
  // explicitly.
  renderer_submission.timeline_info.id = current_main_web_module_timeline_id_;

  renderer_submission.on_rasterized_callbacks.push_back(base::Bind(
      &BrowserModule::OnRendererSubmissionRasterized, base::Unretained(this)));

  if (!splash_screen_) {
    render_tree_combiner_.SetTimelineLayer(main_web_module_layer_.get());
  }
  main_web_module_layer_->Submit(renderer_submission);

  SubmitCurrentRenderTreeToRenderer();
}

void BrowserModule::OnSplashScreenRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnSplashScreenRenderTreeProduced()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);

  if (application_state_ == base::kApplicationStateConcealed ||
      !splash_screen_) {
    return;
  }

  renderer::Submission renderer_submission(
      CreateSubmissionFromLayoutResults(layout_results));

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

  renderer_submission.on_rasterized_callbacks.push_back(base::Bind(
      &BrowserModule::OnRendererSubmissionRasterized, base::Unretained(this)));

  render_tree_combiner_.SetTimelineLayer(splash_screen_layer_.get());
  splash_screen_layer_->Submit(renderer_submission);

  // TODO: write screen shot using render_tree_combiner_ (to combine
  // splash screen and main web_module). Consider when the splash
  // screen is overlaid on top of the main web module render tree, and
  // a screenshot is taken : there will be a race condition on which
  // web module update their render tree last.

  SubmitCurrentRenderTreeToRenderer();
}

void BrowserModule::QueueOnQrCodeOverlayRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::QueueOnQrCodeOverlayRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnQrCodeOverlayRenderTreeProduced,
                 base::Unretained(this), render_tree));
  self_message_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnQrCodeOverlayRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnQrCodeOverlayRenderTreeProduced()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(qr_overlay_info_layer_);

  if (application_state_ == base::kApplicationStateConcealed) {
    return;
  }

  qr_overlay_info_layer_->Submit(renderer::Submission(render_tree));

  SubmitCurrentRenderTreeToRenderer();
}

void BrowserModule::OnWindowClose(base::TimeDelta close_time) {
#if defined(ENABLE_DEBUGGER)
  if (input_device_manager_fuzzer_) {
    return;
  }
#endif

  SbSystemRequestStop(0);
}

void BrowserModule::OnWindowMinimize() {
#if defined(ENABLE_DEBUGGER)
  if (input_device_manager_fuzzer_) {
    return;
  }
#endif
#if SB_API_VERSION >= 13
  SbSystemRequestConceal();
#else
  SbSystemRequestSuspend();
#endif  // SB_API_VERSION >= 13
}

void BrowserModule::OnWindowSizeChanged(const ViewportSize& viewport_size) {
  if (web_module_) {
    web_module_->SetSize(viewport_size);
  }
#if defined(ENABLE_DEBUGGER)
  if (debug_console_) {
    debug_console_->web_module().SetSize(viewport_size);
  }
#endif  // defined(ENABLE_DEBUGGER)
  if (splash_screen_) {
    splash_screen_->web_module().SetSize(viewport_size);
  }

  return;
}

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
void BrowserModule::OnOnScreenKeyboardShown(
    const base::OnScreenKeyboardShownEvent* event) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  // Only inject shown events to the main WebModule.
  on_screen_keyboard_show_called_ = true;
  if (splash_screen_ && splash_screen_->ShutdownSignaled()) {
    DestroySplashScreen(base::TimeDelta());
  }
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardShownEvent(event->ticket());
  }
}

void BrowserModule::OnOnScreenKeyboardHidden(
    const base::OnScreenKeyboardHiddenEvent* event) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  // Only inject hidden events to the main WebModule.
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardHiddenEvent(event->ticket());
  }
}

void BrowserModule::OnOnScreenKeyboardFocused(
    const base::OnScreenKeyboardFocusedEvent* event) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  // Only inject focused events to the main WebModule.
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardFocusedEvent(event->ticket());
  }
}

void BrowserModule::OnOnScreenKeyboardBlurred(
    const base::OnScreenKeyboardBlurredEvent* event) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  // Only inject blurred events to the main WebModule.
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardBlurredEvent(event->ticket());
  }
}

void BrowserModule::OnOnScreenKeyboardSuggestionsUpdated(
    const base::OnScreenKeyboardSuggestionsUpdatedEvent* event) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  // Only inject updated suggestions events to the main WebModule.
  if (web_module_) {
    web_module_->InjectOnScreenKeyboardSuggestionsUpdatedEvent(event->ticket());
  }
}
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

#if SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
void BrowserModule::OnCaptionSettingsChanged(
    const base::AccessibilityCaptionSettingsChangedEvent* event) {
  if (web_module_) {
    web_module_->InjectCaptionSettingsChangedEvent();
  }
}
#endif  // SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)

#if SB_API_VERSION >= 13
void BrowserModule::OnDateTimeConfigurationChanged(
    const base::DateTimeConfigurationChangedEvent* event) {
  icu::TimeZone::adoptDefault(icu::TimeZone::detectHostTimeZone());
  if (web_module_) {
    web_module_->UpdateDateTimeConfiguration();
  }
}
#endif

#if defined(ENABLE_DEBUGGER)
void BrowserModule::OnFuzzerToggle(const std::string& message) {
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnFuzzerToggle, weak_this_, message));
    return;
  }

  if (!input_device_manager_fuzzer_) {
    // Wire up the input fuzzer key generator to the keyboard event callback.
    input_device_manager_fuzzer_ = std::unique_ptr<input::InputDeviceManager>(
        new input::InputDeviceManagerFuzzer(
            base::Bind(&BrowserModule::InjectKeyEventToMainWebModule,
                       base::Unretained(this))));
  } else {
    input_device_manager_fuzzer_.reset();
  }
}

void BrowserModule::OnSetMediaConfig(const std::string& config) {
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnSetMediaConfig, weak_this_, config));
    return;
  }

  std::vector<std::string> tokens = base::SplitString(
      config, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

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

void BrowserModule::OnDisableMediaCodecs(const std::string& codecs) {
  disabled_media_codecs_ = codecs;
  can_play_type_handler_->SetDisabledMediaCodecs(codecs);
}

void BrowserModule::QueueOnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::QueueOnDebugConsoleRenderTreeProduced()");
  render_tree_submission_queue_.AddMessage(
      base::Bind(&BrowserModule::OnDebugConsoleRenderTreeProduced,
                 base::Unretained(this), layout_results));
  self_message_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserModule::ProcessRenderTreeSubmissionQueue, weak_this_));
}

void BrowserModule::OnDebugConsoleRenderTreeProduced(
    const browser::WebModule::LayoutResults& layout_results) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnDebugConsoleRenderTreeProduced()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  if (application_state_ == base::kApplicationStateConcealed) {
    return;
  }

  if (!debug_console_->IsVisible()) {
    // If the layer already has no render tree then simply return. In that case
    // nothing is changing.
    if (!debug_console_layer_->HasRenderTree()) {
      return;
    }
    debug_console_layer_->Submit(base::nullopt);
  } else {
    debug_console_layer_->Submit(
        CreateSubmissionFromLayoutResults(layout_results));
  }

  SubmitCurrentRenderTreeToRenderer();
}

#endif  // defined(ENABLE_DEBUGGER)

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
void BrowserModule::OnOnScreenKeyboardInputEventProduced(
    base::Token type, const dom::InputEventInit& event) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::OnOnScreenKeyboardInputEventProduced()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnOnScreenKeyboardInputEventProduced,
                   weak_this_, type, event));
    return;
  }

#if defined(ENABLE_DEBUGGER)
  if (!debug_console_->FilterOnScreenKeyboardInputEvent(type, event)) {
    return;
  }
#endif  // defined(ENABLE_DEBUGGER)

  InjectOnScreenKeyboardInputEventToMainWebModule(type, event);
}
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

void BrowserModule::OnKeyEventProduced(base::Token type,
                                       const dom::KeyboardEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnKeyEventProduced()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
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
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnPointerEventProduced,
                              weak_this_, type, event));
    return;
  }

#if defined(ENABLE_DEBUGGER)
  if (!debug_console_->FilterPointerEvent(type, event)) {
    return;
  }
#endif  // defined(ENABLE_DEBUGGER)

  DCHECK(web_module_);
  web_module_->InjectPointerEvent(type, event);
}

void BrowserModule::OnWheelEventProduced(base::Token type,
                                         const dom::WheelEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnWheelEventProduced()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::OnWheelEventProduced, weak_this_,
                              type, event));
    return;
  }

#if defined(ENABLE_DEBUGGER)
  if (!debug_console_->FilterWheelEvent(type, event)) {
    return;
  }
#endif  // defined(ENABLE_DEBUGGER)

  DCHECK(web_module_);
  web_module_->InjectWheelEvent(type, event);
}

void BrowserModule::OnWindowOnOnlineEvent(const base::Event* event) {
  if (web_module_) {
    web_module_->InjectWindowOnOnlineEvent(event);
  }
}

void BrowserModule::OnWindowOnOfflineEvent(const base::Event* event) {
  if (web_module_) {
    web_module_->InjectWindowOnOfflineEvent(event);
  }
}

void BrowserModule::InjectKeyEventToMainWebModule(
    base::Token type, const dom::KeyboardEventInit& event) {
  TRACE_EVENT0("cobalt::browser",
               "BrowserModule::InjectKeyEventToMainWebModule()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::InjectKeyEventToMainWebModule,
                              weak_this_, type, event));
    return;
  }

  DCHECK(web_module_);
  web_module_->InjectKeyboardEvent(type, event);
}

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
void BrowserModule::InjectOnScreenKeyboardInputEventToMainWebModule(
    base::Token type, const dom::InputEventInit& event) {
  TRACE_EVENT0(
      "cobalt::browser",
      "BrowserModule::InjectOnScreenKeyboardInputEventToMainWebModule()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(
            &BrowserModule::InjectOnScreenKeyboardInputEventToMainWebModule,
            weak_this_, type, event));
    return;
  }

  DCHECK(web_module_);
  web_module_->InjectOnScreenKeyboardInputEvent(type, event);
}
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

void BrowserModule::OnError(const GURL& url, const std::string& error) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnError()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
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
  pending_navigate_url_ = url;

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

  SystemPlatformErrorHandler::SystemPlatformErrorOptions options;
  options.error_type = kSbSystemPlatformErrorTypeConnectionError;
  options.callback =
      base::Bind(&BrowserModule::OnNetworkFailureSystemPlatformResponse,
                 base::Unretained(this));
  system_platform_error_handler_.RaiseSystemPlatformError(options);
}

void BrowserModule::OnNetworkFailureSystemPlatformResponse(
    SbSystemPlatformErrorResponse response) {
  // A positive response means we should retry, anything else we stop.
  if (response == kSbSystemPlatformErrorResponsePositive) {
    // We shouldn't be here if we don't have a pending URL from an error.
    DCHECK(pending_navigate_url_.is_valid());
    if (pending_navigate_url_.is_valid()) {
      Navigate(pending_navigate_url_);
    }
  } else {
    LOG(ERROR) << "Stop after network error";
    SbSystemRequestStop(0);
  }
}

bool BrowserModule::FilterKeyEvent(base::Token type,
                                   const dom::KeyboardEventInit& event) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::FilterKeyEvent()");
  // Check for hotkeys first. If it is a hotkey, no more processing is needed.
  // TODO: Let WebModule handle the event first, and let the web app prevent
  // this default behavior of the user agent.
  // https://developer.mozilla.org/en-US/docs/Web/API/Event/preventDefault
  if (!FilterKeyEventForHotkeys(type, event)) {
    return false;
  }

#if defined(ENABLE_DEBUGGER)
  if (!debug_console_->FilterKeyEvent(type, event)) {
    return false;
  }
#endif  // defined(ENABLE_DEBUGGER)

  return true;
}

bool BrowserModule::FilterKeyEventForHotkeys(
    base::Token type, const dom::KeyboardEventInit& event) {
#if defined(ENABLE_DEBUGGER)
  if (event.key_code() == dom::keycode::kF1 ||
      (event.ctrl_key() && event.key_code() == dom::keycode::kO)) {
    if (type == base::Tokens::keydown()) {
      // F1 or Ctrl+O cycles the debug console display.
      debug_console_->CycleMode();
    }
    return false;
  } else if (event.key_code() == dom::keycode::kF5) {
    if (type == base::Tokens::keydown()) {
      // F5 reloads the page.
      Reload();
    }
  } else if (event.ctrl_key() && event.key_code() == dom::keycode::kS) {
    if (type == base::Tokens::keydown()) {
#if SB_API_VERSION >= 13
      SbSystemRequestConceal();
#else
      // Ctrl+S suspends Cobalt.
      SbSystemRequestSuspend();
#endif  // SB_API_VERSION >= 13
    }
    return false;
  }
#endif  // defined(ENABLE_DEBUGGER)

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
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
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
std::unique_ptr<webdriver::SessionDriver> BrowserModule::CreateSessionDriver(
    const webdriver::protocol::SessionId& session_id) {
  return base::WrapUnique(new webdriver::SessionDriver(
      session_id,
      base::Bind(&BrowserModule::CreateWindowDriver, base::Unretained(this)),
      base::Bind(&BrowserModule::WaitForLoad, base::Unretained(this))));
}

std::unique_ptr<webdriver::WindowDriver> BrowserModule::CreateWindowDriver(
    const webdriver::protocol::WindowId& window_id) {
  // Repost to our message loop to ensure synchronous access to |web_module_|.
  std::unique_ptr<webdriver::WindowDriver> window_driver;
  self_message_loop_->task_runner()->PostBlockingTask(
      FROM_HERE, base::Bind(&BrowserModule::CreateWindowDriverInternal,
                            base::Unretained(this), window_id,
                            base::Unretained(&window_driver)));

  // This log is relied on by the webdriver benchmark tests, so it shouldn't be
  // changed unless the corresponding benchmark logic is changed as well.
  LOG(INFO) << "Created WindowDriver: ID=" << window_id.id();
  DCHECK(window_driver);
  return window_driver;
}

void BrowserModule::CreateWindowDriverInternal(
    const webdriver::protocol::WindowId& window_id,
    std::unique_ptr<webdriver::WindowDriver>* out_window_driver) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  *out_window_driver = web_module_->CreateWindowDriver(window_id);
}
#endif  // defined(ENABLE_WEBDRIVER)

#if defined(ENABLE_DEBUGGER)
std::unique_ptr<debug::DebugClient> BrowserModule::CreateDebugClient(
    debug::DebugClient::Delegate* delegate) {
  // Repost to our message loop to ensure synchronous access to |web_module_|.
  debug::backend::DebugDispatcher* debug_dispatcher = NULL;
  self_message_loop_->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&BrowserModule::GetDebugDispatcherInternal,
                 base::Unretained(this), base::Unretained(&debug_dispatcher)));
  DCHECK(debug_dispatcher);
  return std::unique_ptr<debug::DebugClient>(
      new debug::DebugClient(debug_dispatcher, delegate));
}

void BrowserModule::GetDebugDispatcherInternal(
    debug::backend::DebugDispatcher** out_debug_dispatcher) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(web_module_);
  *out_debug_dispatcher = web_module_->GetDebugDispatcher();
}
#endif  // ENABLE_DEBUGGER

void BrowserModule::SetProxy(const std::string& proxy_rules) {
  // NetworkModule will ensure this happens on the correct thread.
  network_module_->SetProxy(proxy_rules);
}

void BrowserModule::Blur(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Blur()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateStarted);
  application_state_ = base::kApplicationStateBlurred;
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Blur(timestamp));
}

void BrowserModule::Conceal(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Conceal()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateBlurred);
  application_state_ = base::kApplicationStateConcealed;
  ConcealInternal(timestamp);
}

void BrowserModule::Focus(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Focus()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateBlurred);
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Focus(timestamp));
  application_state_ = base::kApplicationStateStarted;
}

void BrowserModule::Freeze(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Freeze()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateConcealed);
  application_state_ = base::kApplicationStateFrozen;
  FreezeInternal(timestamp);
}

void BrowserModule::Reveal(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Reveal()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateConcealed);
  application_state_ = base::kApplicationStateBlurred;
  RevealInternal(timestamp);
}

void BrowserModule::Unfreeze(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::Unfreeze()");
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(application_state_ == base::kApplicationStateFrozen);
  application_state_ = base::kApplicationStateConcealed;
  UnfreezeInternal(timestamp);
  NavigatePendingURL();
}

void BrowserModule::ReduceMemory() {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  if (splash_screen_) {
    splash_screen_->ReduceMemory();
  }

#if defined(ENABLE_DEBUGGER)
  if (debug_console_) {
    debug_console_->ReduceMemory();
  }
#endif  // defined(ENABLE_DEBUGGER)

  if (web_module_) {
    web_module_->ReduceMemory();
  }
}

void BrowserModule::CheckMemory(
    const int64_t& used_cpu_memory,
    const base::Optional<int64_t>& used_gpu_memory) {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  if (!auto_mem_) {
    return;
  }

  memory_settings_checker_.RunChecks(*auto_mem_, used_cpu_memory,
                                     used_gpu_memory);
}

void BrowserModule::UpdateJavaScriptHeapStatistics() {
  web_module_->RequestJavaScriptHeapStatistics(base::Bind(
      &BrowserModule::GetHeapStatisticsCallback, base::Unretained(this)));
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
    timeout_polling_thread_.message_loop()->task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnPollForRenderTimeout,
                   base::Unretained(this), url),
        base::TimeDelta::FromSeconds(kRenderTimeOutPollingDelaySeconds));
  }
}
#endif

render_tree::ResourceProvider* BrowserModule::GetResourceProvider() {
  if (application_state_ == base::kApplicationStateConcealed) {
    DCHECK(resource_provider_stub_);
    return &(resource_provider_stub_.value());
  }

  if (renderer_module_) {
    return renderer_module_->resource_provider();
  }

  return nullptr;
}

void BrowserModule::InitializeComponents() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::InitializeComponents()");
  InstantiateRendererModule();

  if (media_module_) {
#if SB_API_VERSION >= 13
    media_module_->UpdateSystemWindowAndResourceProvider(system_window_.get(),
                                                         GetResourceProvider());
#else
    media_module_->Resume(GetResourceProvider());
#endif  // SB_API_VERSION >= 13
  } else {
    options_.media_module_options.allow_resume_after_suspend =
        SbSystemSupportsResume();
    media_module_.reset(new media::MediaModule(system_window_.get(),
                                               GetResourceProvider(),
                                               options_.media_module_options));

    if (web_module_) {
      web_module_->SetMediaModule(media_module_.get());
    }
  }

  if (web_module_) {
    web_module_->UpdateCamera3D(input_device_manager_->camera_3d());
    web_module_->GetUiNavRoot()->SetContainerWindow(
        system_window_->GetSbWindow());
  }
}

void BrowserModule::InitializeSystemWindow() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::InitializeSystemWindow()");
  DCHECK(!system_window_);
  if (media_module_ && !window_size_.IsEmpty()) {
    system_window_.reset(
        new system_window::SystemWindow(event_dispatcher_, window_size_));
  } else {
    base::Optional<math::Size> maybe_size;
    if (options_.requested_viewport_size) {
      maybe_size = options_.requested_viewport_size->width_height();
    }
    system_window_.reset(
        new system_window::SystemWindow(event_dispatcher_, maybe_size));

    // Reapply automem settings now that we may have a different viewport size.
    ApplyAutoMemSettings();
  }

  input_device_manager_ = input::InputDeviceManager::CreateFromWindow(
      base::Bind(&BrowserModule::OnKeyEventProduced, base::Unretained(this)),
      base::Bind(&BrowserModule::OnPointerEventProduced,
                 base::Unretained(this)),
      base::Bind(&BrowserModule::OnWheelEventProduced, base::Unretained(this)),
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
      base::Bind(&BrowserModule::OnOnScreenKeyboardInputEventProduced,
                 base::Unretained(this)),
#endif  // SB_API_VERSION >= 12 ||
      // SB_HAS(ON_SCREEN_KEYBOARD)
      system_window_.get());

  InitializeComponents();
}

void BrowserModule::InstantiateRendererModule() {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(system_window_);
  DCHECK(!renderer_module_);

  renderer_module_.reset(new renderer::RendererModule(
      system_window_.get(),
      RendererModuleWithCameraOptions(options_.renderer_module_options,
                                      input_device_manager_->camera_3d())));
  screen_shot_writer_.reset(new ScreenShotWriter(renderer_module_->pipeline()));
}

void BrowserModule::DestroyRendererModule() {
  DCHECK_EQ(base::MessageLoop::current(), self_message_loop_);
  DCHECK(renderer_module_);

  screen_shot_writer_.reset();
  renderer_module_.reset();
}

void BrowserModule::FreezeMediaModule() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::FreezeMediaModule()");
  if (media_module_) {
    media_module_->Suspend();
  }
}

void BrowserModule::NavigatePendingURL() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::NavigatePendingURL()");
  // If there's a navigation that's pending, then attempt to navigate to its
  // specified URL now, unless we're still waiting for an error retry.
  if (pending_navigate_url_.is_valid() && !waiting_for_error_retry_) {
    Navigate(pending_navigate_url_);
  }
}

void BrowserModule::ResetResources() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::ResetResources()");
  if (qr_code_overlay_) {
    qr_code_overlay_->SetResourceProvider(NULL);
  }

  // Flush out any submitted render trees pushed since we started shutting down
  // the web modules above.
  render_tree_submission_queue_.ProcessAll();

  // Clear out the render tree combiner so that it doesn't hold on to any
  // render tree resources either.
  main_web_module_layer_->Reset();
  splash_screen_layer_->Reset();
#if defined(ENABLE_DEBUGGER)
  debug_console_layer_->Reset();
#endif  // defined(ENABLE_DEBUGGER)
  if (qr_overlay_info_layer_) {
    qr_overlay_info_layer_->Reset();
  }
}

void BrowserModule::UpdateScreenSize() {
  ViewportSize size = GetViewportSize();
#if defined(ENABLE_DEBUGGER)
  if (debug_console_) {
    debug_console_->SetSize(size);
  }
#endif  // defined(ENABLE_DEBUGGER)

  if (splash_screen_) {
    splash_screen_->SetSize(size);
  }

  if (web_module_) {
    web_module_->SetSize(size);
  }

  if (qr_code_overlay_) {
    qr_code_overlay_->SetSize(size.width_height());
  }
}

void BrowserModule::ConcealInternal(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::ConcealInternal()");
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_,
                    Conceal(GetResourceProvider(), timestamp));

  ResetResources();

  if (renderer_module_) {
    // Destroy the renderer module into so that it releases all its graphical
    // resources.
    DestroyRendererModule();
  }

  if (media_module_) {
    DCHECK(system_window_);
    window_size_ = system_window_->GetWindowSize();
#if SB_API_VERSION >= 13
    input_device_manager_.reset();
    system_window_.reset();
    media_module_->UpdateSystemWindowAndResourceProvider(NULL,
                                                         GetResourceProvider());
#endif  // SB_API_VERSION >= 13
  }
}

void BrowserModule::FreezeInternal(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::FreezeInternal()");
  FreezeMediaModule();
  // First freeze all our web modules which implies that they will release
  // their resource provider and all resources created through it.
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_, Freeze(timestamp));
}

void BrowserModule::RevealInternal(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::RevealInternal()");
  DCHECK(!renderer_module_);
  if (!system_window_) {
    InitializeSystemWindow();
  } else {
    InitializeComponents();
  }

  DCHECK(system_window_);
  window_size_ = system_window_->GetWindowSize();

  // Propagate the current screen size.
  UpdateScreenSize();

  // Set resource provider right after render module initialized.
  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_,
                    Reveal(GetResourceProvider(), timestamp));

  if (qr_code_overlay_) {
    qr_code_overlay_->SetResourceProvider(GetResourceProvider());
  }
}

void BrowserModule::UnfreezeInternal(SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::UnfreezeInternal()");
// Set the Stub resource provider to media module and to web module
// at Concealed state.
#if SB_API_VERSION >= 13
  media_module_->Resume(GetResourceProvider());
#endif  // SB_API_VERSION >= 13

  FOR_EACH_OBSERVER(LifecycleObserver, lifecycle_observers_,
                    Unfreeze(GetResourceProvider(), timestamp));
}

void BrowserModule::OnMaybeFreeze() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::OnMaybeFreeze()");
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserModule::OnMaybeFreeze, base::Unretained(this)));
    return;
  }

  bool splash_screen_ready_to_freeze =
      splash_screen_ ? splash_screen_->IsReadyToFreeze() : true;
#if defined(ENABLE_DEBUGGER)
  bool debug_console_ready_to_freeze =
      debug_console_ ? debug_console_->IsReadyToFreeze() : true;
#endif  // defined(ENABLE_DEBUGGER)
  bool web_module_ready_to_freeze = web_module_->IsReadyToFreeze();
  if (splash_screen_ready_to_freeze &&
#if defined(ENABLE_DEBUGGER)
      debug_console_ready_to_freeze &&
#endif  // defined(ENABLE_DEBUGGER)
      web_module_ready_to_freeze &&
      application_state_ == base::kApplicationStateConcealed) {
#if SB_API_VERSION >= 13
    DLOG(INFO) << "System request to freeze the app.";
    SbSystemRequestFreeze();
#endif  // SB_API_VERSION >= 13
  }
}

ViewportSize BrowserModule::GetViewportSize() {
  // If a custom render root transform is used, report the size of the
  // transformed viewport.
  math::Matrix3F viewport_transform = math::Matrix3F::Identity();
  static const CobaltExtensionGraphicsApi* s_graphics_extension =
      static_cast<const CobaltExtensionGraphicsApi*>(
          SbSystemGetExtension(kCobaltExtensionGraphicsName));
  float m00, m01, m02, m10, m11, m12, m20, m21, m22;
  if (s_graphics_extension &&
      strcmp(s_graphics_extension->name, kCobaltExtensionGraphicsName) == 0 &&
      s_graphics_extension->version >= 5 &&
      s_graphics_extension->GetRenderRootTransform(&m00, &m01, &m02, &m10, &m11,
                                                   &m12, &m20, &m21, &m22)) {
    viewport_transform =
        math::Matrix3F::FromValues(m00, m01, m02, m10, m11, m12, m20, m21, m22);
  }

  // We trust the renderer module for width and height the most, if it exists.
  if (renderer_module_) {
    math::Size target_size = renderer_module_->render_target_size();
    // ...but get the diagonal from one of the other modules.
    float diagonal_inches = 0;
    float device_pixel_ratio = 1.0f;
    if (system_window_) {
      diagonal_inches = system_window_->GetDiagonalSizeInches();
      device_pixel_ratio = system_window_->GetDevicePixelRatio();

      // For those platforms that can have a main window size smaller than the
      // render target size, the system_window_ size (if exists) should be
      // trusted over the renderer_module_ render target size.
      math::Size window_size = system_window_->GetWindowSize();
      if (window_size.width() <= target_size.width() &&
          window_size.height() <= target_size.height()) {
        target_size = window_size;
      }
    } else if (options_.requested_viewport_size) {
      diagonal_inches = options_.requested_viewport_size->diagonal_inches();
      device_pixel_ratio =
          options_.requested_viewport_size->device_pixel_ratio();
    }

    target_size =
        math::RoundOut(viewport_transform.MapRect(math::RectF(target_size)))
            .size();
    ViewportSize v(target_size.width(), target_size.height(), diagonal_inches,
                   device_pixel_ratio);
    return v;
  }

  // If the system window exists, that's almost just as good.
  if (system_window_) {
    math::Size size = system_window_->GetWindowSize();
    size = math::RoundOut(viewport_transform.MapRect(math::RectF(size))).size();
    ViewportSize v(size.width(), size.height(),
                   system_window_->GetDiagonalSizeInches(),
                   system_window_->GetDevicePixelRatio());
    return v;
  }

  // Otherwise, we assume we'll get the viewport size that was requested.
  if (options_.requested_viewport_size) {
    math::Size requested_size =
        math::RoundOut(viewport_transform.MapRect(math::RectF(
                           options_.requested_viewport_size->width(),
                           options_.requested_viewport_size->height())))
            .size();
    return ViewportSize(requested_size.width(), requested_size.height(),
                        options_.requested_viewport_size->diagonal_inches(),
                        options_.requested_viewport_size->device_pixel_ratio());
  }

  // TODO: Allow platforms to define the default window size and return that
  // here.

  // No window and no viewport size was requested, so we return a conservative
  // default.
  math::Size default_size(1280, 720);
  default_size =
      math::RoundOut(viewport_transform.MapRect(math::RectF(default_size)))
          .size();
  ViewportSize view_size(default_size.width(), default_size.height());
  return view_size;
}

void BrowserModule::ApplyAutoMemSettings() {
  TRACE_EVENT0("cobalt::browser", "BrowserModule::ApplyAutoMemSettings()");
  auto_mem_.reset(new memory_settings::AutoMem(
      GetViewportSize().width_height(), options_.command_line_auto_mem_settings,
      options_.build_auto_mem_settings));

  LOG(INFO) << auto_mem_->ToPrettyPrintString(SbLogIsTty());

  // Web Module options.
  options_.web_module_options.encoded_image_cache_capacity =
      static_cast<int>(auto_mem_->encoded_image_cache_size_in_bytes()->value());
  options_.web_module_options.image_cache_capacity =
      static_cast<int>(auto_mem_->image_cache_size_in_bytes()->value());
  options_.web_module_options.remote_typeface_cache_capacity = static_cast<int>(
      auto_mem_->remote_typeface_cache_size_in_bytes()->value());
  if (web_module_) {
    web_module_->SetImageCacheCapacity(
        auto_mem_->image_cache_size_in_bytes()->value());
    web_module_->SetRemoteTypefaceCacheCapacity(
        auto_mem_->remote_typeface_cache_size_in_bytes()->value());
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

void BrowserModule::GetHeapStatisticsCallback(
    const script::HeapStatistics& heap_statistics) {
  if (base::MessageLoop::current() != self_message_loop_) {
    self_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&BrowserModule::GetHeapStatisticsCallback,
                              base::Unretained(this), heap_statistics));
    return;
  }
  javascript_reserved_memory_ = heap_statistics.total_heap_size;
}

void BrowserModule::SubmitCurrentRenderTreeToRenderer() {
  if (web_module_) {
    web_module_->GetUiNavRoot()->PerformQueuedUpdates();
  }

  if (!renderer_module_) {
    return;
  }

  base::Optional<renderer::Submission> submission =
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

base::Optional<std::string> BrowserModule::SetSplashScreenTopicFallback(
    const GURL& url) {
  std::map<std::string, std::string> url_param_map;
  // If this is the initial startup, use topic within deeplink, if specified.
  if (main_web_module_generation_ == 1) {
    std::string deeplink = GetInitialDeepLink();
    size_t query_pos = deeplink.find('?');
    if (query_pos != std::string::npos) {
      GetParamMap(deeplink.substr(query_pos + 1), url_param_map);
    }
  }
  // If this is not the initial startup, there was no deeplink specified, or
  // the deeplink did not have a topic, check the current url for a topic.
  if (url_param_map["topic"].empty()) {
    GetParamMap(url.query(), url_param_map);
  }
  std::string splash_topic = url_param_map["topic"];
  // If a topic was found, check whether a fallback url was specified.
  if (!splash_topic.empty()) {
    GURL splash_url = options_.fallback_splash_screen_topic_map[splash_topic];
    if (!splash_url.spec().empty()) {
      // Update fallback splash screen url to topic-specific URL.
      fallback_splash_screen_url_ = splash_url;
    }
    return base::Optional<std::string>(splash_topic);
  }
  return base::Optional<std::string>();
}

void BrowserModule::GetParamMap(const std::string& url,
                                std::map<std::string, std::string>& map) {
  bool next_is_option = true;
  bool next_is_value = false;
  std::string option = "";
  base::StringTokenizer tokenizer(url, "&=");
  tokenizer.set_options(base::StringTokenizer::RETURN_DELIMS);

  while (tokenizer.GetNext()) {
    if (tokenizer.token_is_delim()) {
      switch (*tokenizer.token_begin()) {
        case '&':
          next_is_option = true;
          break;
        case '=':
          next_is_value = true;
          break;
      }
    } else {
      std::string token = tokenizer.token();
      if (next_is_value && !option.empty()) {
        // Overwrite previous value when an option appears more than once.
        map[option] = token;
      }
      option = "";
      if (next_is_option) {
        option = token;
      }
      next_is_option = false;
      next_is_value = false;
    }
  }
}

scoped_refptr<script::Wrappable> BrowserModule::CreateH5vcc(
    const scoped_refptr<dom::Window>& window,
    script::GlobalEnvironment* global_environment) {
  h5vcc::H5vcc::Settings h5vcc_settings;
  h5vcc_settings.media_module = media_module_.get();
  h5vcc_settings.network_module = network_module_;
#if SB_IS(EVERGREEN)
  h5vcc_settings.updater_module = updater_module_;
#endif
  h5vcc_settings.account_manager = account_manager_;
  h5vcc_settings.event_dispatcher = event_dispatcher_;
  h5vcc_settings.user_agent_data = window->navigator()->user_agent_data();
  h5vcc_settings.global_environment = global_environment;

  return scoped_refptr<script::Wrappable>(new h5vcc::H5vcc(h5vcc_settings));
}

void BrowserModule::SetApplicationStartOrPreloadTimestamp(
    bool is_preload, SbTimeMonotonic timestamp) {
  DCHECK(web_module_);
  web_module_->SetApplicationStartOrPreloadTimestamp(is_preload, timestamp);
}

void BrowserModule::SetDeepLinkTimestamp(SbTimeMonotonic timestamp) {
  DCHECK(web_module_);
  web_module_->SetDeepLinkTimestamp(timestamp);
}

}  // namespace browser
}  // namespace cobalt
