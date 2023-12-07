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

#include "cobalt/browser/application.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cobalt/base/accessibility_caption_settings_changed_event.h"
#include "cobalt/base/accessibility_settings_changed_event.h"
#include "cobalt/base/accessibility_text_to_speech_settings_changed_event.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/date_time_configuration_changed_event.h"
#include "cobalt/base/deep_link_event.h"
#include "cobalt/base/get_application_key.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/base/language.h"
#include "cobalt/base/localized_strings.h"
#include "cobalt/base/on_screen_keyboard_blurred_event.h"
#include "cobalt/base/on_screen_keyboard_focused_event.h"
#include "cobalt/base/on_screen_keyboard_hidden_event.h"
#include "cobalt/base/on_screen_keyboard_shown_event.h"
#include "cobalt/base/on_screen_keyboard_suggestions_updated_event.h"
#include "cobalt/base/starboard_stats_tracker.h"
#include "cobalt/base/startup_timer.h"
#include "cobalt/base/version_compatibility.h"
#include "cobalt/base/window_on_offline_event.h"
#include "cobalt/base/window_on_online_event.h"
#include "cobalt/base/window_size_changed_event.h"
#include "cobalt/browser/client_hint_headers.h"
#include "cobalt/browser/device_authentication.h"
#include "cobalt/browser/memory_settings/auto_mem_settings.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager.h"
#include "cobalt/browser/switches.h"
#include "cobalt/browser/user_agent_platform_info.h"
#include "cobalt/browser/user_agent_string.h"
#include "cobalt/cache/cache.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/h5vcc/h5vcc_crash_log.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/math/size.h"
#include "cobalt/script/javascript_engine.h"
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "cobalt/storage/savegame_fake.h"
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "cobalt/system_window/input_event.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "cobalt/watchdog/watchdog.h"
#include "components/metrics/metrics_service.h"
#include "starboard/common/device_type.h"
#include "starboard/common/metrics/stats_tracker.h"
#include "starboard/common/system_property.h"
#include "starboard/configuration.h"
#include "starboard/event.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/installation_manager.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "url/gurl.h"

#if SB_IS(EVERGREEN)
#include "cobalt/updater/utils.h"
#endif

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace browser {

namespace {
const int kStatUpdatePeriodMs = 1000;

const char kDefaultURL[] = "https://www.youtube.com/tv";

#if defined(ENABLE_ABOUT_SCHEME)
const char kAboutBlankURL[] = "about:blank";
#endif  // defined(ENABLE_ABOUT_SCHEME)

const char kPersistentSettingsJson[] = "settings.json";

// The watchdog client name used to represent Stats.
const char kWatchdogName[] = "stats";
// The watchdog time interval in microseconds allowed between pings before
// triggering violations.
const int64_t kWatchdogTimeInterval = 10000000;
// The watchdog time wait in microseconds to initially wait before triggering
// violations.
const int64_t kWatchdogTimeWait = 2000000;

bool IsStringNone(const std::string& str) {
  return !strcasecmp(str.c_str(), "none");
}

#if defined(ENABLE_WEBDRIVER) || defined(ENABLE_DEBUGGER)
std::string GetDevServersListenIp() {
  bool ip_v6;
  ip_v6 = SbSocketIsIpv6Supported();
  // Default to INADDR_ANY
  std::string listen_ip(ip_v6 ? "::" : "0.0.0.0");

#if SB_API_VERSION < 15
  // Desktop PCs default to loopback.
  if (SbSystemGetDeviceType() == kSbSystemDeviceTypeDesktopPC) {
    listen_ip = ip_v6 ? "::1" : "127.0.0.1";
  }
#else
  if (starboard::GetSystemPropertyString(kSbSystemPropertyDeviceType) ==
      starboard::kSystemDeviceTypeDesktopPC) {
    listen_ip = ip_v6 ? "::1" : "127.0.0.1";
  }
#endif

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDevServersListenIp)) {
    listen_ip =
        command_line->GetSwitchValueASCII(switches::kDevServersListenIp);
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return listen_ip;
}
#endif  // defined(ENABLE_WEBDRIVER) || defined(ENABLE_DEBUGGER)

#if defined(ENABLE_DEBUGGER)
int GetRemoteDebuggingPort() {
#if defined(SB_OVERRIDE_DEFAULT_REMOTE_DEBUGGING_PORT)
  const unsigned int kDefaultRemoteDebuggingPort =
      SB_OVERRIDE_DEFAULT_REMOTE_DEBUGGING_PORT;
#else
  const unsigned int kDefaultRemoteDebuggingPort = 9222;
#endif  // defined(SB_OVERRIDE_DEFAULT_REMOTE_DEBUGGING_PORT)
  unsigned int remote_debugging_port = kDefaultRemoteDebuggingPort;
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (!base::StringToUint(switch_value, &remote_debugging_port)) {
      DLOG(ERROR) << "Invalid port specified for remote debug server: "
                  << switch_value
                  << ". Using default port: " << kDefaultRemoteDebuggingPort;
      remote_debugging_port = kDefaultRemoteDebuggingPort;
    }
  }
  DCHECK(remote_debugging_port != 0 ||
         !command_line->HasSwitch(switches::kWaitForWebDebugger))
      << switches::kWaitForWebDebugger << " switch can't be used when "
      << switches::kRemoteDebuggingPort << " is 0 (disabled).";
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
  return uint16_t(remote_debugging_port);
}
#endif  // ENABLE_DEBUGGER

#if defined(ENABLE_WEBDRIVER)
int GetWebDriverPort() {
// The default port on which the webdriver server should listen for incoming
// connections.
#if defined(SB_OVERRIDE_DEFAULT_WEBDRIVER_PORT)
  const int kDefaultWebDriverPort = SB_OVERRIDE_DEFAULT_WEBDRIVER_PORT;
#else
  const int kDefaultWebDriverPort = 4444;
#endif  // defined(SB_OVERRIDE_DEFAULT_WEBDRIVER_PORT)
  int webdriver_port = kDefaultWebDriverPort;
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWebDriverPort)) {
    if (!base::StringToInt(
            command_line->GetSwitchValueASCII(switches::kWebDriverPort),
            &webdriver_port)) {
      DLOG(ERROR) << "Invalid port specified for WebDriver server: "
                  << command_line->GetSwitchValueASCII(switches::kWebDriverPort)
                  << ". Using default port: " << kDefaultWebDriverPort;
      webdriver_port = kDefaultWebDriverPort;
    }
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
  return webdriver_port;
}
#endif  // ENABLE_WEBDRIVER

GURL GetInitialURL(bool should_preload) {
  GURL initial_url = GURL(kDefaultURL);
  // Allow the user to override the default URL via a command line parameter.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInitialURL)) {
    std::string url_switch =
        command_line->GetSwitchValueASCII(switches::kInitialURL);
#if defined(ENABLE_ABOUT_SCHEME)
    // Check the switch itself since some non-empty strings parse to empty URLs.
    if (url_switch.empty()) {
      LOG(ERROR) << "URL from parameter is empty, using " << kAboutBlankURL;
      return GURL(kAboutBlankURL);
    }
#endif  // defined(ENABLE_ABOUT_SCHEME)
    GURL url = GURL(url_switch);
    if (url.is_valid()) {
      initial_url = url;
    } else {
      LOG(ERROR) << "URL \"" << url_switch
                 << "\" from parameter is not valid, using default URL "
                 << initial_url;
    }
  }

  if (should_preload) {
    std::string query = initial_url.query();
    if (!query.empty()) {
      query += "&";
    }
    query += "launch=preload";
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    initial_url = initial_url.ReplaceComponents(replacements);
  }

  if (!command_line->HasSwitch(
          switches::kOmitDeviceAuthenticationQueryParameters)) {
    // Append the device authentication query parameters based on the platform's
    // certification secret to the initial URL.
    std::string query = initial_url.query();
    std::string device_authentication_query_string =
        GetDeviceAuthenticationSignedURLQueryString();
    if (!query.empty() && !device_authentication_query_string.empty()) {
      query += "&";
    }
    query += device_authentication_query_string;

    if (!query.empty()) {
      GURL::Replacements replacements;
      replacements.SetQueryStr(query);
      initial_url = initial_url.ReplaceComponents(replacements);
    }
  }

  return initial_url;
}

bool ValidateSplashScreen(const base::Optional<GURL>& url) {
  if (url->is_valid() &&
      (url->SchemeIsFile() || url->SchemeIs("h5vcc-embedded"))) {
    return true;
  }
  LOG(FATAL) << "Ignoring invalid fallback splash screen: " << url->spec();
  return false;
}

base::Optional<GURL> GetFallbackSplashScreenURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string fallback_splash_screen_string;
  if (command_line->HasSwitch(switches::kFallbackSplashScreenURL)) {
    fallback_splash_screen_string =
        command_line->GetSwitchValueASCII(switches::kFallbackSplashScreenURL);
  } else {
    fallback_splash_screen_string = configuration::Configuration::GetInstance()
                                        ->CobaltFallbackSplashScreenUrl();
  }
  if (IsStringNone(fallback_splash_screen_string)) {
    return base::Optional<GURL>();
  }
  base::Optional<GURL> fallback_splash_screen_url =
      GURL(fallback_splash_screen_string);
  ValidateSplashScreen(fallback_splash_screen_url);
  return fallback_splash_screen_url;
}

// Parses the fallback_splash_screen_topics command line parameter
// and maps topics to full file url locations, if valid.
void ParseFallbackSplashScreenTopics(
    const base::Optional<GURL>& default_fallback_splash_screen_url,
    std::map<std::string, GURL>* fallback_splash_screen_topic_map) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string topics;
  if (command_line->HasSwitch(switches::kFallbackSplashScreenTopics)) {
    topics = command_line->GetSwitchValueASCII(
        switches::kFallbackSplashScreenTopics);
  } else {
    topics = configuration::Configuration::GetInstance()
                 ->CobaltFallbackSplashScreenTopics();
  }

  // Note: values in topics_map may be either file paths or filenames.
  std::map<std::string, std::string> topics_map;
  BrowserModule::GetParamMap(topics, topics_map);
  for (auto iterator = topics_map.begin(); iterator != topics_map.end();
       iterator++) {
    std::string topic = iterator->first;
    std::string location = iterator->second;
    base::Optional<GURL> topic_fallback_url = GURL(location);

    // If not a valid url, check whether it is a valid filename in the
    // same directory as the default fallback url.
    if (!topic_fallback_url->is_valid()) {
      if (default_fallback_splash_screen_url) {
        topic_fallback_url = GURL(
            default_fallback_splash_screen_url->GetWithoutFilename().spec() +
            location);
      } else {
        break;
      }
    }
    if (ValidateSplashScreen(topic_fallback_url)) {
      (*fallback_splash_screen_topic_map)[topic] = topic_fallback_url.value();
    }
  }
}

base::TimeDelta GetTimedTraceDuration() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kTimedTrace) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kTimedTrace),
          &duration_in_seconds)) {
    return base::TimeDelta::FromSeconds(duration_in_seconds);
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return base::TimeDelta();
}

base::FilePath GetExtraWebFileDir() {
  // Default is empty, command line can override.
  base::FilePath result;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kExtraWebFileDir)) {
    result = base::FilePath(
        command_line->GetSwitchValueASCII(switches::kExtraWebFileDir));
    if (!result.IsAbsolute()) {
      // Non-absolute paths are relative to the executable directory.
      base::FilePath content_path;
      base::PathService::Get(base::DIR_EXE, &content_path);
      result = content_path.DirName().DirName().Append(result);
    }
    DLOG(INFO) << "Extra web file dir: " << result.value();
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return result;
}

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
void EnableUsingStubImageDecoderIfRequired() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kStubImageDecoder)) {
    loader::image::ImageDecoder::UseStubImageDecoder();
  }
}
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

base::Optional<cssom::ViewportSize> GetRequestedViewportSize(
    base::CommandLine* command_line) {
  DCHECK(command_line);
  if (!command_line->HasSwitch(browser::switches::kViewport)) {
    return base::nullopt;
  }

  std::string switch_value =
      command_line->GetSwitchValueASCII(browser::switches::kViewport);

  std::vector<std::string> lengths = base::SplitString(
      switch_value, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (lengths.empty()) {
    DLOG(ERROR) << "Viewport " << switch_value << " is invalid.";
    return base::nullopt;
  }

  int width = 0;
  if (!base::StringToInt(lengths[0], &width)) {
    DLOG(ERROR) << "Viewport " << switch_value << " has invalid width.";
    return base::nullopt;
  }

  if (lengths.size() < 2) {
    // Allow shorthand specification of the viewport by only giving the
    // width. This calculates the height at 4:3 aspect ratio for smaller
    // viewport widths, and 16:9 for viewports 1280 pixels wide or larger.
    if (width >= 1280) {
      return ViewportSize(width, 9 * width / 16);
    }
    return ViewportSize(width, 3 * width / 4);
  }

  int height = 0;
  if (!base::StringToInt(lengths[1], &height)) {
    DLOG(ERROR) << "Viewport " << switch_value << " has invalid height.";
    return base::nullopt;
  }

  if (lengths.size() < 3) {
    return ViewportSize(width, height);
  }

  double screen_diagonal_inches = 0.0f;
  if (lengths.size() >= 3) {
    if (!base::StringToDouble(lengths[2], &screen_diagonal_inches)) {
      DLOG(ERROR) << "Viewport " << switch_value
                  << " has invalid screen_diagonal_inches.";
      return base::nullopt;
    }
  }

  double video_pixel_ratio = 1.0f;
  if (lengths.size() >= 4) {
    if (!base::StringToDouble(lengths[3], &video_pixel_ratio)) {
      DLOG(ERROR) << "Viewport " << switch_value
                  << " has invalid video_pixel_ratio.";
      return base::nullopt;
    }
  }

  return ViewportSize(width, height, static_cast<float>(video_pixel_ratio),
                      static_cast<float>(screen_diagonal_inches));
}

std::string GetMinLogLevelString() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kMinLogLevel)) {
    return command_line->GetSwitchValueASCII(switches::kMinLogLevel);
  }
#if defined(OFFICIAL_BUILD)
  return "fatal";
#else
  return "info";
#endif
}

int StringToLogLevel(const std::string& log_level) {
  if (log_level == "verbose") {
    // The lower the verbose level is, the more messages are logged.  Set it to
    // a lower enough value to ensure that all known verbose messages are
    // logged.
    return logging::LOG_VERBOSE - 15;
  } else if (log_level == "info") {
    return logging::LOG_INFO;
  } else if (log_level == "warning") {
    return logging::LOG_WARNING;
  } else if (log_level == "error") {
    return logging::LOG_ERROR;
  } else if (log_level == "fatal") {
    return logging::LOG_FATAL;
  } else {
    NOTREACHED() << "Unrecognized logging level: " << log_level;
#if defined(OFFICIAL_BUILD)
    return logging::LOG_FATAL;
#else
    return logging::LOG_INFO;
#endif
  }
}

void SetIntegerIfSwitchIsSet(const char* switch_name, int* output) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    int32 out;
    if (base::StringToInt32(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
                switch_name),
            &out)) {
      LOG(INFO) << "Command line switch '" << switch_name << "': Modifying "
                << *output << " -> " << out;
      *output = out;
    } else {
      LOG(ERROR) << "Invalid value for command line setting: " << switch_name;
    }
  }
}

void ApplyCommandLineSettingsToRendererOptions(
    renderer::RendererModule::Options* options) {
  SetIntegerIfSwitchIsSet(browser::switches::kScratchSurfaceCacheSizeInBytes,
                          &options->scratch_surface_cache_size_in_bytes);
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  auto command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kDisableRasterizerCaching) ||
      command_line->HasSwitch(
          browser::switches::kForceDeterministicRendering)) {
    options->force_deterministic_rendering = true;
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
}

struct NonTrivialStaticFields {
  NonTrivialStaticFields() : system_language(base::GetSystemLanguage()) {}

  const std::string system_language;
  std::string user_agent;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

struct SecurityFlags {
  csp::CSPHeaderPolicy csp_header_policy;
  network::HTTPSRequirement https_requirement;
  network::CORSPolicy cors_policy;
};

// |non_trivial_static_fields| will be lazily created on the first time it's
// accessed.
base::LazyInstance<NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

void AddCrashHandlerAnnotations(const UserAgentPlatformInfo& platform_info) {
  auto crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  if (!crash_handler_extension) {
    LOG(INFO) << "No crash handler extension, not sending annotations.";
    return;
  }

  std::string user_agent =
      cobalt::browser::CreateUserAgentString(platform_info);
  std::string version = "";
#if SB_IS(EVERGREEN)
  version = cobalt::updater::GetCurrentEvergreenVersion();
  std::string product = "Cobalt_Evergreen";
#else
  std::string product = "Cobalt";
#endif
  if (version.empty()) {
    base::StringAppendF(&version, "%s.%s-%s",
                        platform_info.cobalt_version().c_str(),
                        platform_info.cobalt_build_version_number().c_str(),
                        platform_info.build_configuration().c_str());
  }

  user_agent.push_back('\0');
  product.push_back('\0');
  version.push_back('\0');

  bool result = true;
  if (crash_handler_extension->version == 1) {
    CrashpadAnnotations crashpad_annotations;
    memset(&crashpad_annotations, 0, sizeof(CrashpadAnnotations));
    strncpy(crashpad_annotations.user_agent_string, user_agent.c_str(),
            USER_AGENT_STRING_MAX_SIZE);
    strncpy(crashpad_annotations.product, product.c_str(),
            CRASHPAD_ANNOTATION_DEFAULT_LENGTH);
    strncpy(crashpad_annotations.version, version.c_str(),
            CRASHPAD_ANNOTATION_DEFAULT_LENGTH);
    result = crash_handler_extension->OverrideCrashpadAnnotations(
        &crashpad_annotations);
  } else if (crash_handler_extension->version > 1) {
    // These particular annotation key names (e.g., ver) are expected by
    // Crashpad.
    // TODO(b/201538792): figure out a clean way to define these constants once
    // and use them everywhere.
    if (!crash_handler_extension->SetString("user_agent_string",
                                            user_agent.c_str())) {
      result = false;
    }
    // TODO(b/265339522): move crashpad prod and ver setter to starboard.
    if (!crash_handler_extension->SetString("prod", product.c_str())) {
      result = false;
    }
    if (!crash_handler_extension->SetString("ver", version.c_str())) {
      result = false;
    }
  } else {
    result = false;
  }  // Invalid extension version
  if (result) {
    LOG(INFO) << "Sent annotations to crash handler";
  } else {
    LOG(ERROR) << "Could not send some annotation(s) to crash handler.";
  }
}

void AddCrashLogApplicationState(base::ApplicationState state) {
  std::string application_state = std::string(GetApplicationStateString(state));
  application_state.push_back('\0');

  auto crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  if (crash_handler_extension && crash_handler_extension->version >= 2) {
    if (!crash_handler_extension->SetString("application_state",
                                            application_state.c_str())) {
      LOG(ERROR) << "Could not send application state to crash handler.";
    }
    return;
  }

  // Crash handler is not supported, fallback to crash log dictionary.
  h5vcc::CrashLogDictionary::GetInstance()->SetString("application_state",
                                                      application_state);
}

}  // namespace

// Static user logs
ssize_t Application::available_memory_ = 0;
int64 Application::lifetime_in_ms_ = 0;

Application::Application(const base::Closure& quit_closure, bool should_preload,
                         SbTimeMonotonic timestamp)
    : message_loop_(base::MessageLoop::current()), quit_closure_(quit_closure) {
  DCHECK(!quit_closure_.is_null());
  if (should_preload) {
    preload_timestamp_ = timestamp;
  } else {
    start_timestamp_ = timestamp;
  }
  // Check to see if a timed_trace has been set, indicating that we should
  // begin a timed trace upon startup.
  base::TimeDelta trace_duration = GetTimedTraceDuration();
  if (trace_duration != base::TimeDelta()) {
    trace_event::TraceToFileForDuration(
        base::FilePath(FILE_PATH_LITERAL("timed_trace.json")), trace_duration);
  }

  TRACE_EVENT0("cobalt::browser", "Application::Application()");

  DCHECK(base::MessageLoop::current());
  DCHECK_EQ(
      base::MessageLoop::TYPE_UI,
      static_cast<base::MessageLoop*>(base::MessageLoop::current())->type());

  DETACH_FROM_THREAD(network_event_thread_checker_);
  DETACH_FROM_THREAD(application_event_thread_checker_);

  // Set the minimum logging level, if specified on the command line.
  logging::SetMinLogLevel(StringToLogLevel(GetMinLogLevelString()));

  stats_update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kStatUpdatePeriodMs),
      base::Bind(&Application::UpdatePeriodicStats, base::Unretained(this)));

  // Get the initial URL.
  GURL initial_url = GetInitialURL(should_preload);
  DLOG(INFO) << "Initial URL: " << initial_url;

  // Get the fallback splash screen URL.
  base::Optional<GURL> fallback_splash_screen_url =
      GetFallbackSplashScreenURL();
  DLOG(INFO) << "Fallback splash screen URL: "
             << (fallback_splash_screen_url ? fallback_splash_screen_url->spec()
                                            : "none");

  // Get the system language and initialize our localized strings.
  std::string language = base::GetSystemLanguage();
  base::LocalizedStrings::GetInstance()->Initialize(language);

  // A one-per-process task scheduler is needed for usage of APIs in
  // base/post_task.h which will be used by some net APIs like
  // URLRequestContext;
  base::TaskScheduler::CreateAndStartWithDefaultParams("Cobalt TaskScheduler");

  starboard::StatsTrackerContainer::GetInstance()->set_stats_tracker(
      std::make_unique<StarboardStatsTracker>());

  // Initializes persistent settings.
  persistent_settings_ =
      std::make_unique<persistent_storage::PersistentSettings>(
          kPersistentSettingsJson);

  // Initialize telemetry/metrics.
  InitMetrics();

  // Initializes Watchdog.
  watchdog::Watchdog* watchdog =
      watchdog::Watchdog::CreateInstance(persistent_settings_.get());
  DCHECK(watchdog);

  // Registers Stats as a watchdog client.
  if (watchdog)
    watchdog->Register(kWatchdogName, kWatchdogName,
                       base::kApplicationStateStarted, kWatchdogTimeInterval,
                       kWatchdogTimeWait, watchdog::NONE);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::Optional<cssom::ViewportSize> requested_viewport_size =
      GetRequestedViewportSize(command_line);

  unconsumed_deep_link_ = GetInitialDeepLink();
  DLOG(INFO) << "Initial deep link: " << unconsumed_deep_link_;

  network::NetworkModule::Options network_module_options;
  // Create the main components of our browser.
  BrowserModule::Options options;
  network_module_options.preferred_language = language;
  network_module_options.persistent_settings = persistent_settings_.get();
  options.persistent_settings = persistent_settings_.get();
  options.command_line_auto_mem_settings =
      memory_settings::GetSettings(*command_line);
  options.build_auto_mem_settings = memory_settings::GetDefaultBuildSettings();
  options.fallback_splash_screen_url = fallback_splash_screen_url;

  ParseFallbackSplashScreenTopics(fallback_splash_screen_url,
                                  &options.fallback_splash_screen_topic_map);

  if (command_line->HasSwitch(browser::switches::kFPSPrint)) {
    options.renderer_module_options.enable_fps_stdout = true;
  }
  if (command_line->HasSwitch(browser::switches::kFPSOverlay)) {
    options.renderer_module_options.enable_fps_overlay = true;
  }

  ApplyCommandLineSettingsToRendererOptions(&options.renderer_module_options);

  if (command_line->HasSwitch(browser::switches::kDisableJavaScriptJit)) {
    options.web_module_options.web_options.javascript_engine_options
        .disable_jit = true;
  }

  if (command_line->HasSwitch(
          browser::switches::kRetainRemoteTypefaceCacheDuringSuspend)) {
    options.web_module_options.should_retain_remote_typeface_cache_on_freeze =
        true;
  }

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(browser::switches::kNullSavegame)) {
    network_module_options.storage_manager_options.savegame_options.factory =
        &storage::SavegameFake::Create;
  }

  if (command_line->HasSwitch(browser::switches::kDisableOnScreenKeyboard)) {
    options.enable_on_screen_keyboard = false;
  }

#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

  constexpr int kDefaultMinCompatibilityVersion = 1;
  int minimum_version = kDefaultMinCompatibilityVersion;
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (command_line->HasSwitch(browser::switches::kMinCompatibilityVersion)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kMinCompatibilityVersion);
    if (!base::StringToInt(switch_value, &minimum_version)) {
      DLOG(ERROR) << "Invalid min_compatibility_version provided.";
    }
  }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::VersionCompatibility::GetInstance()->SetMinimumVersion(minimum_version);

  base::Optional<std::string> partition_key;
  if (command_line->HasSwitch(browser::switches::kLocalStoragePartitionUrl)) {
    std::string local_storage_partition_url = command_line->GetSwitchValueASCII(
        browser::switches::kLocalStoragePartitionUrl);
    partition_key = base::GetApplicationKey(GURL(local_storage_partition_url));
    CHECK(partition_key) << "local_storage_partition_url is not a valid URL.";
  } else {
    partition_key = base::GetApplicationKey(initial_url);
  }
  network_module_options.storage_manager_options.savegame_options.id =
      partition_key;

  base::Optional<std::string> default_key =
      base::GetApplicationKey(GURL(kDefaultURL));
  if (command_line->HasSwitch(
          browser::switches::kForceMigrationForStoragePartitioning) ||
      partition_key == default_key) {
    network_module_options.storage_manager_options.savegame_options
        .fallback_to_default_id = true;
  }

  // User can specify an extra search path entry for files loaded via file://.
  options.web_module_options.web_options.extra_web_file_dir =
      GetExtraWebFileDir();
  SecurityFlags security_flags{csp::kCSPRequired, network::kHTTPSRequired,
                               network::kCORSRequired};
  // Set callback to be notified when a navigation occurs that destroys the
  // underlying WebModule.
  options.web_module_created_callback =
      base::Bind(&Application::MainWebModuleCreated, base::Unretained(this));

  // The main web module's stat tracker tracks event stats.
  options.web_module_options.track_event_stats = true;

  if (command_line->HasSwitch(browser::switches::kProxy)) {
    network_module_options.custom_proxy =
        command_line->GetSwitchValueASCII(browser::switches::kProxy);
  }

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#if defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)
  if (command_line->HasSwitch(browser::switches::kIgnoreCertificateErrors)) {
    network_module_options.ignore_certificate_errors = true;
  }
#endif  // defined(ENABLE_IGNORE_CERTIFICATE_ERRORS)

  if (!command_line->HasSwitch(switches::kRequireHTTPSLocation)) {
    security_flags.https_requirement = network::kHTTPSOptional;
  }

  if (!command_line->HasSwitch(browser::switches::kRequireCSP)) {
    security_flags.csp_header_policy = csp::kCSPOptional;
  }

  if (command_line->HasSwitch(browser::switches::kAllowAllCrossOrigin)) {
    security_flags.cors_policy = network::kCORSOptional;
  }

  if (command_line->HasSwitch(browser::switches::kProd)) {
    security_flags.https_requirement = network::kHTTPSRequired;
    security_flags.csp_header_policy = csp::kCSPRequired;
    security_flags.cors_policy = network::kCORSRequired;
  }

  if (command_line->HasSwitch(switches::kVideoPlaybackRateMultiplier)) {
    double playback_rate = 1.0;
    base::StringToDouble(command_line->GetSwitchValueASCII(
                             switches::kVideoPlaybackRateMultiplier),
                         &playback_rate);
    options.web_module_options.video_playback_rate_multiplier =
        static_cast<float>(playback_rate);
    DLOG(INFO) << "Set video playback rate multiplier to "
               << options.web_module_options.video_playback_rate_multiplier;
  }

  EnableUsingStubImageDecoderIfRequired();

  if (command_line->HasSwitch(switches::kDisableImageAnimations)) {
    options.web_module_options.enable_image_animations = false;
  }
  if (command_line->HasSwitch(switches::kDisableSplashScreenOnReloads)) {
    options.enable_splash_screen_on_reloads = false;
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

// Production-builds override all switches to the most secure configuration.
#if defined(COBALT_FORCE_HTTPS)
  security_flags.https_requirement = network::kHTTPSRequired;
#endif  // defined(COBALT_FORCE_HTTPS)

#if defined(COBALT_FORCE_CSP)
  security_flags.csp_header_policy = csp::kCSPRequired;
#endif  // defined(COBALT_FORCE_CSP)

#if defined(COBALT_FORCE_CORS)
  security_flags.cors_policy = network::kCORSRequired;
#endif  // defined(COBALT_FORCE_CORS)

  network_module_options.https_requirement = security_flags.https_requirement;
  network_module_options.cors_policy = security_flags.cors_policy;
  options.web_module_options.csp_header_policy =
      security_flags.csp_header_policy;
  options.web_module_options.csp_enforcement_type = web::kCspEnforcementEnable;

  options.requested_viewport_size = requested_viewport_size;

  // Set callback to collect unload event time before firing document's unload
  // event.
  options.web_module_options.collect_unload_event_time_callback = base::Bind(
      &Application::CollectUnloadEventTimingInfo, base::Unretained(this));

  cobalt::browser::UserAgentPlatformInfo platform_info;

  network_module_.reset(new network::NetworkModule(
      CreateUserAgentString(platform_info), GetClientHintHeaders(platform_info),
      &event_dispatcher_, network_module_options));
  // This is not necessary, but since some platforms need a lot of time to read
  // the savegame, start the storage manager as soon as possible here.
  network_module_->EnsureStorageManagerStarted();

  AddCrashHandlerAnnotations(platform_info);

#if SB_IS(EVERGREEN)
  if (SbSystemGetExtension(kCobaltExtensionInstallationManagerName) &&
      !command_line->HasSwitch(switches::kDisableUpdaterModule)) {
    uint64_t update_check_delay_sec =
        cobalt::updater::kDefaultUpdateCheckDelaySeconds;
    if (command_line->HasSwitch(browser::switches::kUpdateCheckDelaySeconds)) {
      std::string seconds_value = command_line->GetSwitchValueASCII(
          browser::switches::kUpdateCheckDelaySeconds);
      if (!base::StringToUint64(seconds_value, &update_check_delay_sec)) {
        LOG(WARNING) << "Invalid delay specified for the update check: "
                     << seconds_value << ". Using default value: "
                     << cobalt::updater::kDefaultUpdateCheckDelaySeconds;
        update_check_delay_sec =
            cobalt::updater::kDefaultUpdateCheckDelaySeconds;
      }
    }
    updater_module_.reset(new updater::UpdaterModule(network_module_.get(),
                                                     update_check_delay_sec));
  }
#endif
  browser_module_.reset(
      new BrowserModule(initial_url,
                        (should_preload ? base::kApplicationStateConcealed
                                        : base::kApplicationStateStarted),
                        &event_dispatcher_, network_module_.get(),
#if SB_IS(EVERGREEN)
                        updater_module_.get(),
#endif
                        options));

  UpdateUserAgent();

  // Register event callbacks.
  window_size_change_event_callback_ = base::Bind(
      &Application::OnWindowSizeChangedEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(base::WindowSizeChangedEvent::TypeId(),
                                     window_size_change_event_callback_);
  on_screen_keyboard_shown_event_callback_ = base::Bind(
      &Application::OnOnScreenKeyboardShownEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(base::OnScreenKeyboardShownEvent::TypeId(),
                                     on_screen_keyboard_shown_event_callback_);
  on_screen_keyboard_hidden_event_callback_ = base::Bind(
      &Application::OnOnScreenKeyboardHiddenEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(
      base::OnScreenKeyboardHiddenEvent::TypeId(),
      on_screen_keyboard_hidden_event_callback_);
  on_screen_keyboard_focused_event_callback_ = base::Bind(
      &Application::OnOnScreenKeyboardFocusedEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(
      base::OnScreenKeyboardFocusedEvent::TypeId(),
      on_screen_keyboard_focused_event_callback_);
  on_screen_keyboard_blurred_event_callback_ = base::Bind(
      &Application::OnOnScreenKeyboardBlurredEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(
      base::OnScreenKeyboardBlurredEvent::TypeId(),
      on_screen_keyboard_blurred_event_callback_);
  on_screen_keyboard_suggestions_updated_event_callback_ =
      base::Bind(&Application::OnOnScreenKeyboardSuggestionsUpdatedEvent,
                 base::Unretained(this));
  event_dispatcher_.AddEventCallback(
      base::OnScreenKeyboardSuggestionsUpdatedEvent::TypeId(),
      on_screen_keyboard_suggestions_updated_event_callback_);
  on_caption_settings_changed_event_callback_ = base::Bind(
      &Application::OnCaptionSettingsChangedEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(
      base::AccessibilityCaptionSettingsChangedEvent::TypeId(),
      on_caption_settings_changed_event_callback_);
  on_window_on_online_event_callback_ =
      base::Bind(&Application::OnWindowOnOnlineEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(base::WindowOnOnlineEvent::TypeId(),
                                     on_window_on_online_event_callback_);
  on_window_on_offline_event_callback_ =
      base::Bind(&Application::OnWindowOnOfflineEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(base::WindowOnOfflineEvent::TypeId(),
                                     on_window_on_offline_event_callback_);
  on_date_time_configuration_changed_event_callback_ =
      base::Bind(&Application::OnDateTimeConfigurationChangedEvent,
                 base::Unretained(this));
  event_dispatcher_.AddEventCallback(
      base::DateTimeConfigurationChangedEvent::TypeId(),
      on_date_time_configuration_changed_event_callback_);

#if defined(ENABLE_WEBDRIVER)
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  bool create_webdriver_module =
      !command_line->HasSwitch(switches::kDisableWebDriver);
#else
  bool create_webdriver_module = true;
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
  if (create_webdriver_module) {
    web_driver_module_.reset(new webdriver::WebDriverModule(
        GetWebDriverPort(), GetDevServersListenIp(),
        base::Bind(&BrowserModule::CreateSessionDriver,
                   base::Unretained(browser_module_.get())),
        base::Bind(&BrowserModule::RequestScreenshotToMemory,
                   base::Unretained(browser_module_.get())),
        base::Bind(&BrowserModule::SetProxy,
                   base::Unretained(browser_module_.get())),
        base::Bind(&Application::Quit, base::Unretained(this))));
  }
#endif  // ENABLE_WEBDRIVER

#if defined(ENABLE_DEBUGGER)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebDebugger)) {
    LOG(INFO) << "Remote web debugger disabled.";
  } else {
    int remote_debugging_port = GetRemoteDebuggingPort();
    if (remote_debugging_port == 0) {
      LOG(INFO) << "Remote web debugger disabled because "
                << switches::kRemoteDebuggingPort << " is 0.";
    } else {
      debug_web_server_.reset(new debug::remote::DebugWebServer(
          remote_debugging_port, GetDevServersListenIp(),
          base::Bind(&BrowserModule::CreateDebugClient,
                     base::Unretained(browser_module_.get()))));
    }
  }
#endif  // ENABLE_DEBUGGER

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  int duration_in_seconds = 0;
  if (command_line->HasSwitch(switches::kShutdownAfter) &&
      base::StringToInt(
          command_line->GetSwitchValueASCII(switches::kShutdownAfter),
          &duration_in_seconds)) {
    // If the "shutdown_after" command line option is specified, setup a delayed
    // message to quit the application after the specified number of seconds
    // have passed.
    message_loop_->task_runner()->PostDelayedTask(
        FROM_HERE, quit_closure_,
        base::TimeDelta::FromSeconds(duration_in_seconds));
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  AddCrashLogApplicationState(base::kApplicationStateStarted);
}

Application::~Application() {
  // explicitly reset here because the destruction of the object is complex
  // and involves a thread join. If this were to hang the app then having
  // the destruction at this point gives a real file-line number and a place
  // for the debugger to land.
  watchdog::Watchdog::DeleteInstance();

  // Explicitly delete the global metrics services manager here to give it
  // an opportunity to clean up late logs and persist metrics.
  metrics::CobaltMetricsServicesManager::DeleteInstance();

  // Unregister event callbacks.
  event_dispatcher_.RemoveEventCallback(base::WindowSizeChangedEvent::TypeId(),
                                        window_size_change_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::OnScreenKeyboardShownEvent::TypeId(),
      on_screen_keyboard_shown_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::OnScreenKeyboardHiddenEvent::TypeId(),
      on_screen_keyboard_hidden_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::OnScreenKeyboardFocusedEvent::TypeId(),
      on_screen_keyboard_focused_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::OnScreenKeyboardBlurredEvent::TypeId(),
      on_screen_keyboard_blurred_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::OnScreenKeyboardSuggestionsUpdatedEvent::TypeId(),
      on_screen_keyboard_suggestions_updated_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::AccessibilityCaptionSettingsChangedEvent::TypeId(),
      on_caption_settings_changed_event_callback_);
  event_dispatcher_.RemoveEventCallback(
      base::DateTimeConfigurationChangedEvent::TypeId(),
      on_date_time_configuration_changed_event_callback_);
  browser_module_.reset();
  network_module_.reset();
}

void Application::Start(SbTimeMonotonic timestamp) {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&Application::Start, base::Unretained(this), timestamp));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(application_event_thread_checker_);

  if (!start_timestamp_.has_value()) {
    start_timestamp_ = timestamp;
  }
  OnApplicationEvent(kSbEventTypeStart, timestamp);
}

void Application::Quit() {
  if (base::MessageLoop::current() != message_loop_) {
    message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&Application::Quit, base::Unretained(this)));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(application_event_thread_checker_);

  quit_closure_.Run();
}

void Application::HandleStarboardEvent(const SbEvent* starboard_event) {
  DCHECK(starboard_event);
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);

  // Forward input events to |SystemWindow|.
  if (starboard_event->type == kSbEventTypeInput) {
    system_window::HandleInputEvent(starboard_event);
    return;
  }

  // Create a Cobalt event from the Starboard event, if recognized.
  switch (starboard_event->type) {
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
    case kSbEventTypeConceal:
    case kSbEventTypeReveal:
    case kSbEventTypeFreeze:
    case kSbEventTypeUnfreeze:
    case kSbEventTypeLowMemory:
      OnApplicationEvent(starboard_event->type, starboard_event->timestamp);
      break;
    case kSbEventTypeWindowSizeChanged:
      DispatchEventInternal(new base::WindowSizeChangedEvent(
          static_cast<SbEventWindowSizeChangedData*>(starboard_event->data)
              ->window,
          static_cast<SbEventWindowSizeChangedData*>(starboard_event->data)
              ->size));
      break;
    case kSbEventTypeOnScreenKeyboardShown:
      DCHECK(starboard_event->data);
      DispatchEventInternal(new base::OnScreenKeyboardShownEvent(
          *static_cast<int*>(starboard_event->data)));
      break;
    case kSbEventTypeOnScreenKeyboardHidden:
      DispatchEventInternal(new base::OnScreenKeyboardHiddenEvent(
          *static_cast<int*>(starboard_event->data)));
      break;
    case kSbEventTypeOnScreenKeyboardFocused:
      DCHECK(starboard_event->data);
      DispatchEventInternal(new base::OnScreenKeyboardFocusedEvent(
          *static_cast<int*>(starboard_event->data)));
      break;
    case kSbEventTypeOnScreenKeyboardBlurred:
      DispatchEventInternal(new base::OnScreenKeyboardBlurredEvent(
          *static_cast<int*>(starboard_event->data)));
      break;
    case kSbEventTypeOnScreenKeyboardSuggestionsUpdated:
      DispatchEventInternal(new base::OnScreenKeyboardSuggestionsUpdatedEvent(
          *static_cast<int*>(starboard_event->data)));
      break;
    case kSbEventTypeLink: {
      DispatchDeepLink(static_cast<const char*>(starboard_event->data),
                       starboard_event->timestamp);
      break;
    }
    case kSbEventTypeAccessibilitySettingsChanged:
      DispatchEventInternal(new base::AccessibilitySettingsChangedEvent());
      break;
    case kSbEventTypeAccessibilityCaptionSettingsChanged:
      DispatchEventInternal(
          new base::AccessibilityCaptionSettingsChangedEvent());
      break;
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged:
      DispatchEventInternal(
          new base::AccessibilityTextToSpeechSettingsChangedEvent());
      break;
    case kSbEventTypeOsNetworkDisconnected:
      DispatchEventInternal(new base::WindowOnOfflineEvent());
      break;
    case kSbEventTypeOsNetworkConnected:
      DispatchEventInternal(new base::WindowOnOnlineEvent());
      break;
    case kSbEventDateTimeConfigurationChanged:
      DispatchEventInternal(new base::DateTimeConfigurationChangedEvent());
      break;
    // Explicitly list unhandled cases here so that the compiler can give a
    // warning when a value is added, but not handled.
    case kSbEventTypeInput:
    case kSbEventTypePreload:
    case kSbEventTypeScheduled:
    case kSbEventTypeStart:
    case kSbEventTypeStop:
    case kSbEventTypeUser:
    case kSbEventTypeVerticalSync:
      DLOG(WARNING) << "Unhandled Starboard event of type: "
                    << starboard_event->type;
  }
}

void Application::OnApplicationEvent(SbEventType event_type,
                                     SbTimeMonotonic timestamp) {
  TRACE_EVENT0("cobalt::browser", "Application::OnApplicationEvent()");
  DCHECK_CALLED_ON_VALID_THREAD(application_event_thread_checker_);

  watchdog::Watchdog* watchdog = watchdog::Watchdog::GetInstance();

  switch (event_type) {
    case kSbEventTypeStop:
      LOG(INFO) << "Got quit event.";
      if (watchdog) watchdog->UpdateState(base::kApplicationStateStopped);
      AddCrashLogApplicationState(base::kApplicationStateStopped);
      Quit();
      LOG(INFO) << "Finished quitting.";
      break;
    case kSbEventTypeStart:
      LOG(INFO) << "Got start event.";
      browser_module_->Reveal(timestamp);
      browser_module_->Focus(timestamp);
      LOG(INFO) << "Finished starting.";
      break;
    case kSbEventTypeBlur:
      LOG(INFO) << "Got blur event.";
      browser_module_->Blur(timestamp);
      LOG(INFO) << "Finished blurring.";
      break;
    case kSbEventTypeFocus:
      LOG(INFO) << "Got focus event.";
      browser_module_->Focus(timestamp);
      LOG(INFO) << "Finished focusing.";
      break;
    case kSbEventTypeConceal:
      LOG(INFO) << "Got conceal event.";
      browser_module_->Conceal(timestamp);
      LOG(INFO) << "Finished concealing.";
      break;
    case kSbEventTypeReveal:
      DCHECK(SbSystemSupportsResume());
      LOG(INFO) << "Got reveal event.";
      browser_module_->Reveal(timestamp);
      LOG(INFO) << "Finished revealing.";
      break;
    case kSbEventTypeFreeze:
      LOG(INFO) << "Got freeze event.";
      browser_module_->Freeze(timestamp);
#if SB_IS(EVERGREEN)
      if (updater_module_) updater_module_->Suspend();
#endif
      LOG(INFO) << "Finished freezing.";
      break;
    case kSbEventTypeUnfreeze:
      LOG(INFO) << "Got unfreeze event.";
      browser_module_->Unfreeze(timestamp);
#if SB_IS(EVERGREEN)
      if (updater_module_) updater_module_->Resume();
#endif
      LOG(INFO) << "Finished unfreezing.";
      break;
    case kSbEventTypeLowMemory:
      DLOG(INFO) << "Got low memory event.";
      browser_module_->ReduceMemory();
      DLOG(INFO) << "Finished reducing memory usage.";
      break;
    // All of the remaining event types are unexpected:
    case kSbEventTypePreload:
    case kSbEventTypeWindowSizeChanged:
    case kSbEventTypeAccessibilityCaptionSettingsChanged:
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged:
    case kSbEventTypeOnScreenKeyboardBlurred:
    case kSbEventTypeOnScreenKeyboardFocused:
    case kSbEventTypeOnScreenKeyboardHidden:
    case kSbEventTypeOnScreenKeyboardShown:
    case kSbEventTypeOnScreenKeyboardSuggestionsUpdated:
    case kSbEventTypeAccessibilitySettingsChanged:
    case kSbEventTypeInput:
    case kSbEventTypeLink:
    case kSbEventTypeScheduled:
    case kSbEventTypeUser:
    case kSbEventTypeVerticalSync:
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
    case kSbEventDateTimeConfigurationChanged:
      NOTREACHED() << "Unexpected event type: " << event_type;
      return;
  }
  if (watchdog) watchdog->UpdateState(browser_module_->GetApplicationState());
  AddCrashLogApplicationState(browser_module_->GetApplicationState());
}

void Application::OnWindowSizeChangedEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser", "Application::OnWindowSizeChangedEvent()");
  const base::WindowSizeChangedEvent* window_size_change_event =
      base::polymorphic_downcast<const base::WindowSizeChangedEvent*>(event);
  const auto& size = window_size_change_event->size();
  float diagonal =
      SbWindowGetDiagonalSizeInInches(window_size_change_event->window());

  // A value of 0.0 for the video pixel ratio means that the ratio could not be
  // determined. In that case it should be assumed to be the same as the
  // graphics resolution, which corresponds to a device pixel ratio of 1.0.
  float device_pixel_ratio =
      (size.video_pixel_ratio == 0) ? 1.0f : size.video_pixel_ratio;
  cssom::ViewportSize viewport_size(size.width, size.height, diagonal,
                                    device_pixel_ratio);
  browser_module_->OnWindowSizeChanged(viewport_size);
}

void Application::OnOnScreenKeyboardShownEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnOnScreenKeyboardShownEvent()");
  browser_module_->OnOnScreenKeyboardShown(
      base::polymorphic_downcast<const base::OnScreenKeyboardShownEvent*>(
          event));
}

void Application::OnOnScreenKeyboardHiddenEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnOnScreenKeyboardHiddenEvent()");
  browser_module_->OnOnScreenKeyboardHidden(
      base::polymorphic_downcast<const base::OnScreenKeyboardHiddenEvent*>(
          event));
}

void Application::OnOnScreenKeyboardFocusedEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnOnScreenKeyboardFocusedEvent()");
  browser_module_->OnOnScreenKeyboardFocused(
      base::polymorphic_downcast<const base::OnScreenKeyboardFocusedEvent*>(
          event));
}

void Application::OnOnScreenKeyboardBlurredEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnOnScreenKeyboardBlurredEvent()");
  browser_module_->OnOnScreenKeyboardBlurred(
      base::polymorphic_downcast<const base::OnScreenKeyboardBlurredEvent*>(
          event));
}

void Application::OnOnScreenKeyboardSuggestionsUpdatedEvent(
    const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnOnScreenKeyboardSuggestionsUpdatedEvent()");
  browser_module_->OnOnScreenKeyboardSuggestionsUpdated(
      base::polymorphic_downcast<
          const base::OnScreenKeyboardSuggestionsUpdatedEvent*>(event));
}

void Application::OnCaptionSettingsChangedEvent(const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnCaptionSettingsChangedEvent()");
  browser_module_->OnCaptionSettingsChanged(
      base::polymorphic_downcast<
          const base::AccessibilityCaptionSettingsChangedEvent*>(event));
}

void Application::OnWindowOnOnlineEvent(const base::Event* event) {
  browser_module_->OnWindowOnOnlineEvent(
      base::polymorphic_downcast<const base::WindowOnOnlineEvent*>(event));
}
void Application::OnWindowOnOfflineEvent(const base::Event* event) {
  browser_module_->OnWindowOnOfflineEvent(
      base::polymorphic_downcast<const base::WindowOnOfflineEvent*>(event));
}

void Application::OnDateTimeConfigurationChangedEvent(
    const base::Event* event) {
  TRACE_EVENT0("cobalt::browser",
               "Application::OnDateTimeConfigurationChangedEvent()");
  browser_module_->OnDateTimeConfigurationChanged(
      base::polymorphic_downcast<
          const base::DateTimeConfigurationChangedEvent*>(event));
}

void Application::MainWebModuleCreated(WebModule* web_module) {
  TRACE_EVENT0("cobalt::browser", "Application::MainWebModuleCreated()");
  // Note: This is a callback function that runs in the MainWebModule thread.
  DCHECK(web_module);
  if (preload_timestamp_.has_value()) {
    web_module->SetApplicationStartOrPreloadTimestamp(
        true, preload_timestamp_.value());
  }
  if (start_timestamp_.has_value()) {
    web_module->SetApplicationStartOrPreloadTimestamp(false,
                                                      start_timestamp_.value());
  }
  DispatchDeepLinkIfNotConsumed();
#if defined(ENABLE_WEBDRIVER)
  if (web_driver_module_) {
    web_driver_module_->OnWindowRecreated();
  }
#endif
  if (!unload_event_start_time_.is_null() ||
      !unload_event_end_time_.is_null()) {
    web_module->SetUnloadEventTimingInfo(unload_event_start_time_,
                                         unload_event_end_time_);
  }
}

void Application::CollectUnloadEventTimingInfo(base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  unload_event_start_time_ = start_time;
  unload_event_end_time_ = end_time;
}

Application::CValStats::CValStats()
    : free_cpu_memory("Memory.CPU.Free", 0,
                      "Total free application CPU memory remaining."),
      used_cpu_memory("Memory.CPU.Used", 0,
                      "Total CPU memory allocated via the app's allocators."),
      app_start_time("Time.Cobalt.Start",
                     base::StartupTimer::StartTime().ToInternalValue(),
                     "Start time of the application in microseconds."),
      app_lifetime("Cobalt.Lifetime", base::TimeDelta(),
                   "Application lifetime in microseconds.") {
  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    free_gpu_memory.emplace("Memory.GPU.Free", 0,
                            "Total free application GPU memory remaining.");
    used_gpu_memory.emplace("Memory.GPU.Used", 0,
                            "Total GPU memory allocated by the application.");
  }
}

// NOTE: UserAgent registration is handled separately, as the value is not
// available when the app is first being constructed. Registration must happen
// each time the user agent is modified, because the string may be pointing
// to a new location on the heap.
void Application::UpdateUserAgent() {
  non_trivial_static_fields.Get().user_agent = browser_module_->GetUserAgent();
  LOG(INFO) << "User Agent: " << non_trivial_static_fields.Get().user_agent;
}

void Application::UpdatePeriodicStats() {
  TRACE_EVENT0("cobalt::browser", "Application::UpdatePeriodicStats()");
  c_val_stats_.app_lifetime = base::StartupTimer::TimeElapsed();

  // Pings watchdog.
  watchdog::Watchdog* watchdog = watchdog::Watchdog::GetInstance();
  if (watchdog) {
#if defined(_DEBUG)
    // Injects delay for watchdog debugging.
    watchdog->MaybeInjectDebugDelay(kWatchdogName);
#endif  // defined(_DEBUG)
    watchdog->Ping(kWatchdogName);
  }

  int64_t used_cpu_memory = SbSystemGetUsedCPUMemory();
  base::Optional<int64_t> used_gpu_memory;
  if (SbSystemHasCapability(kSbSystemCapabilityCanQueryGPUMemoryStats)) {
    used_gpu_memory = SbSystemGetUsedGPUMemory();
  }

  available_memory_ =
      static_cast<ssize_t>(SbSystemGetTotalCPUMemory() - used_cpu_memory);
  c_val_stats_.free_cpu_memory = available_memory_;
  c_val_stats_.used_cpu_memory = used_cpu_memory;

  if (used_gpu_memory) {
    *c_val_stats_.free_gpu_memory =
        SbSystemGetTotalGPUMemory() - *used_gpu_memory;
    *c_val_stats_.used_gpu_memory = *used_gpu_memory;
  }

  browser_module_->UpdateJavaScriptHeapStatistics();

  browser_module_->CheckMemory(used_cpu_memory, used_gpu_memory);
}

void Application::DispatchEventInternal(base::Event* event) {
  event_dispatcher_.DispatchEvent(std::unique_ptr<base::Event>(event));
}

// Called to handle deep link consumed events.
void Application::OnDeepLinkConsumedCallback(const std::string& link) {
  LOG(INFO) << "Got deep link consumed callback: " << link;
  base::AutoLock auto_lock(unconsumed_deep_link_lock_);
  if (link == unconsumed_deep_link_) {
    unconsumed_deep_link_.clear();
  }
}

void Application::DispatchDeepLink(const char* link,
                                   SbTimeMonotonic timestamp) {
  if (!link || *link == 0) {
    return;
  }

  std::string deep_link;
  // This block exists to ensure that the lock is held while accessing
  // unconsumed_deep_link_.
  {
    base::AutoLock auto_lock(unconsumed_deep_link_lock_);
    // Stash the deep link so that if it is not consumed, it can be dispatched
    // again after the next WebModule is created.
    unconsumed_deep_link_ = link;
    deep_link = unconsumed_deep_link_;
    deep_link_timestamp_ = timestamp;
  }

  LOG(INFO) << "Dispatching deep link: " << deep_link;
  DispatchEventInternal(new base::DeepLinkEvent(
      deep_link, base::Bind(&Application::OnDeepLinkConsumedCallback,
                            base::Unretained(this), deep_link)));
  if (browser_module_) {
    browser_module_->SetDeepLinkTimestamp(timestamp);
  }
}

void Application::DispatchDeepLinkIfNotConsumed() {
  std::string deep_link;
  SbTimeMonotonic timestamp;
  // This block exists to ensure that the lock is held while accessing
  // unconsumed_deep_link_.
  {
    base::AutoLock auto_lock(unconsumed_deep_link_lock_);
    deep_link = unconsumed_deep_link_;
    timestamp = deep_link_timestamp_;
  }

  if (!deep_link.empty()) {
    LOG(INFO) << "Dispatching deep link: " << deep_link;
    DispatchEventInternal(new base::DeepLinkEvent(
        deep_link, base::Bind(&Application::OnDeepLinkConsumedCallback,
                              base::Unretained(this), deep_link)));
  }
  if (browser_module_) {
    browser_module_->SetDeepLinkTimestamp(timestamp);
  }
}

void Application::InitMetrics() {
  // Must be called early as it initializes global state which is then read by
  // all threads without synchronization.
  // RecordAction task runner must be called before metric initialization.
  base::SetRecordActionTaskRunner(base::ThreadTaskRunnerHandle::Get());
  metrics_services_manager_ =
      metrics::CobaltMetricsServicesManager::GetInstance();
  // Before initializing metrics manager, set any persisted settings like if
  // it's enabled or upload interval.
  bool is_metrics_enabled = persistent_settings_->GetPersistentSettingAsBool(
      metrics::kMetricEnabledSettingName, false);
  auto metric_event_interval = persistent_settings_->GetPersistentSettingAsInt(
      metrics::kMetricEventIntervalSettingName, 300);
  metrics_services_manager_->SetEventDispatcher(&event_dispatcher_);
  metrics_services_manager_->SetUploadInterval(metric_event_interval);
  metrics_services_manager_->ToggleMetricsEnabled(is_metrics_enabled);
  // Metric recording state initialization _must_ happen before we bootstrap
  // otherwise we crash.
  metrics_services_manager_->GetMetricsService()
      ->InitializeMetricsRecordingState();
  // UpdateUploadPermissions bootstraps the whole metric reporting, scheduling,
  // and uploading cycle.
  metrics_services_manager_->UpdateUploadPermissions(is_metrics_enabled);
  LOG(INFO)
      << "Cobalt Telemetry initialized with settings: is_metrics_enabled: "
      << is_metrics_enabled
      << ", metric_event_interval: " << metric_event_interval;
}

}  // namespace browser
}  // namespace cobalt

const char* GetCobaltUserAgentString() {
  static std::string ua = cobalt::browser::CreateUserAgentString(
      cobalt::browser::GetUserAgentPlatformInfoFromSystem());
  return ua.c_str();
}
