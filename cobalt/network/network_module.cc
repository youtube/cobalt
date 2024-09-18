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

#include "cobalt/network/network_module.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/task_runner_util.h"
#include "cobalt/network/network_system.h"
#include "cobalt/network/switches.h"
#include "net/url_request/static_http_user_agent_settings.h"

namespace cobalt {
namespace network {

namespace {
#if defined(ENABLE_NETWORK_LOGGING)
const char kCaptureModeIncludeCookiesAndCredentials[] =
    "IncludeCookiesAndCredentials";
const char kCaptureModeIncludeSocketBytes[] = "IncludeSocketBytes";
const char kDefaultNetLogName[] = "cobalt_netlog.json";
#endif
constexpr size_t kNetworkModuleStackSize = 512 * 1024;
}  // namespace

NetworkModule::NetworkModule(const Options& options) : options_(options) {
  Initialize("Null user agent string.", NULL);
}

NetworkModule::NetworkModule(
    const std::string& user_agent_string,
    const std::vector<std::string>& client_hint_headers,
    base::EventDispatcher* event_dispatcher, const Options& options)
    : client_hint_headers_(client_hint_headers), options_(options) {
  Initialize(user_agent_string, event_dispatcher);
}

void NetworkModule::WillDestroyCurrentMessageLoop() {
#if defined(DIAL_SERVER)
  dial_service_proxy_ = nullptr;
  dial_service_.reset();
#endif

  cookie_jar_.reset();
  net_poster_.reset();
  url_request_context_.reset();
}

NetworkModule::~NetworkModule() {
  // Order of destruction is important here.
  // URLRequestContext and NetworkDelegate must be destroyed on the IO thread.
  // The ObjectWatchMultiplexer must be destroyed last.  (The sockets owned
  // by URLRequestContext will destroy their ObjectWatchers, which need the
  // multiplexer.)
  url_request_context_getter_ = nullptr;

  if (thread_) {
    // Wait for all previously posted tasks to finish.
    base::task_runner_util::WaitForFence(thread_->task_runner(), FROM_HERE);
    // This will trigger a call to WillDestroyCurrentMessageLoop in the thread
    // and wait for it to finish.
    thread_.reset();
  }
#if !defined(STARBOARD)
  object_watch_multiplexer_.reset();
#endif
  network_system_.reset();
}

std::string NetworkModule::GetUserAgent() const {
  auto* http_user_agent_settings =
      url_request_context_->url_request_context()->http_user_agent_settings();
  DCHECK(http_user_agent_settings);
  return http_user_agent_settings->GetUserAgent();
}

network_bridge::PostSender NetworkModule::GetPostSender() const {
  return base::Bind(&network_bridge::NetPoster::Send,
                    base::Unretained(net_poster_.get()));
}

void NetworkModule::SetProxy(const std::string& custom_proxy_rules) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&URLRequestContext::SetProxy,
                            base::Unretained(url_request_context_.get()),
                            custom_proxy_rules));
}

void NetworkModule::SetEnableQuicFromPersistentSettings() {
  // Called on initialization and when the persistent setting is changed.
  if (options_.persistent_settings != nullptr) {
    base::Value value;
    options_.persistent_settings->Get(kQuicEnabledPersistentSettingsKey,
                                      &value);
    bool enable_quic = value.GetIfBool().value_or(false);
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&URLRequestContext::SetEnableQuic,
                   base::Unretained(url_request_context_.get()), enable_quic));
  }
}

void NetworkModule::SetEnableHttp2FromPersistentSettings() {
  // Called on initialization and when the persistent setting is changed.
  if (options_.persistent_settings != nullptr) {
    base::Value value;
    options_.persistent_settings->Get(kHttp2EnabledPersistentSettingsKey,
                                      &value);
    bool enable_http2 = value.GetIfBool().value_or(true);
    task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&URLRequestContext::SetEnableHttp2,
                   base::Unretained(url_request_context_.get()), enable_http2));
  }
}

void NetworkModule::SetEnableHttp3FromPersistentSettings() {
  // Called on initialization and when the persistent setting is changed.
  if (options_.persistent_settings != nullptr) {
    base::Value value;
    options_.persistent_settings->Get(kHttp3EnabledPersistentSettingsKey,
                                      &value);
    bool enable_http3 = value.GetIfBool().value_or(false);
    auto supported_version =
        enable_http3
            ? net::DefaultSupportedQuicVersions()
            : quic::ParsedQuicVersionVector{quic::ParsedQuicVersion::Q046()};
    task_runner()->PostTask(
        FROM_HERE, base::Bind(
                       [](URLRequestContext* url_request_context,
                          quic::ParsedQuicVersionVector supported_version) {
                         url_request_context->url_request_context()
                             ->quic_context()
                             ->params()
                             // Only allow the RFC version.
                             ->supported_versions = supported_version;
                       },
                       base::Unretained(url_request_context_.get()),
                       std::move(supported_version)));
  }
}

bool NetworkModule::SetHttpProtocolFilterPersistentSetting(
    const std::string& raw_json) {
  base::Value old_config_json;
  options_.persistent_settings->Get(network::kProtocolFilterKey,
                                    &old_config_json);

  if (raw_json.empty()) {
    if (old_config_json.is_none()) {
      return false;
    }
    options_.persistent_settings->Set(network::kProtocolFilterKey,
                                      base::Value());
    protocol_filter_update_pending_ = true;
    return true;
  }

  absl::optional<base::Value> old_config;
  if (old_config_json.is_string()) {
    old_config = base::JSONReader::Read(old_config_json.GetString());
  }
  absl::optional<base::Value> raw_config = base::JSONReader::Read(raw_json);
  if (!raw_config) return false;
  base::Value::List new_config;
  if (!raw_config->is_list()) return false;
  for (auto& filter_value : raw_config->GetList()) {
    if (!filter_value.is_dict()) continue;
    const auto& dict = filter_value.GetDict();
    const base::Value* origin = dict.Find("origin");
    const base::Value* alt_svc = dict.Find("altSvc");
    if (!origin || !alt_svc) continue;
    if (!origin->is_string() || !alt_svc->is_string()) continue;
    base::Value::Dict dest_dict;
    dest_dict.Set("origin", origin->GetString());
    dest_dict.Set("altSvc", alt_svc->GetString());
    new_config.Append(std::move(dest_dict));
  }
  if (new_config.empty()) return false;
  if (old_config && old_config->is_list() &&
      old_config->GetList() == new_config)
    return false;
  absl::optional<std::string> json = base::WriteJson(new_config);
  if (!json) return false;
  options_.persistent_settings->Set(network::kProtocolFilterKey,
                                    base::Value(*json));
  protocol_filter_update_pending_ = true;
  return true;
}

void NetworkModule::SetProtocolFilterFromPersistentSettings() {
  if (!options_.persistent_settings) return;
  if (!protocol_filter_update_pending_) return;
  protocol_filter_update_pending_ = false;

  base::Value value;
  options_.persistent_settings->Get(kProtocolFilterKey, &value);
  if (!value.is_string()) return;

  if (value.GetString().empty()) {
    task_runner()->PostTask(FROM_HERE,
                            base::Bind(
                                [](URLRequestContext* url_request_context) {
                                  url_request_context->url_request_context()
                                      ->quic_context()
                                      ->params()
                                      ->protocol_filter = absl::nullopt;
                                },
                                base::Unretained(url_request_context_.get())));
    return;
  }

  absl::optional<base::Value> config =
      base::JSONReader::Read(value.GetString());
  if (!config.has_value() || !config->is_list()) return;

  net::ProtocolFilter protocol_filter;
  for (auto& filter_value : config->GetList()) {
    if (!filter_value.is_dict()) return;
    const auto& dict = filter_value.GetDict();
    const base::Value* origin = dict.Find("origin");
    const base::Value* alt_svc = dict.Find("altSvc");
    if (!origin || !alt_svc) continue;
    if (!origin->is_string() || !alt_svc->is_string()) continue;
    net::ProtocolFilterEntry entry;
    entry.origin = origin->GetString();
    if (base::StartsWith(alt_svc->GetString(), "h3")) {
      entry.alt_svc.protocol = net::kProtoQUIC;
      if (base::StartsWith(alt_svc->GetString(), "h3-Q046")) {
        entry.alt_svc.quic_version =
            net::ProtocolFilterEntry::QuicVersion::Q046;
      } else {
        entry.alt_svc.quic_version =
            net::ProtocolFilterEntry::QuicVersion::RFC_V1;
      }
    } else {
      entry.alt_svc.protocol = net::kProtoUnknown;
    }
    protocol_filter.push_back(std::move(entry));
  }

  task_runner()->PostTask(
      FROM_HERE, base::Bind(
                     [](URLRequestContext* url_request_context,
                        net::ProtocolFilter protocol_filter) {
                       url_request_context->url_request_context()
                           ->quic_context()
                           ->params()
                           ->protocol_filter = std::move(protocol_filter);
                     },
                     base::Unretained(url_request_context_.get()),
                     std::move(protocol_filter)));
}

void NetworkModule::EnsureStorageManagerStarted() {
  DCHECK(storage_manager_);
  storage_manager_->EnsureStarted();
}

void NetworkModule::Initialize(const std::string& user_agent_string,
                               base::EventDispatcher* event_dispatcher) {
  storage_manager_.reset(
      new storage::StorageManager(options_.storage_manager_options));

  thread_.reset(new base::Thread("NetworkModule"));
#if !defined(STARBOARD)
  object_watch_multiplexer_.reset(new base::ObjectWatchMultiplexer());
#endif
  network_system_ = NetworkSystem::Create(event_dispatcher);
  auto http_user_agent_settings =
      std::make_unique<net::StaticHttpUserAgentSettings>(
          options_.preferred_language, user_agent_string);

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string custom_user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    http_user_agent_settings.reset(new net::StaticHttpUserAgentSettings(
        options_.preferred_language, custom_user_agent));
  }

  if (command_line->HasSwitch(switches::kMaxNetworkDelay)) {
    base::StringToInt64(
        command_line->GetSwitchValueASCII(switches::kMaxNetworkDelay),
        &options_.max_network_delay_usec);
  }

#if defined(ENABLE_NETWORK_LOGGING)
  base::FilePath result;
  base::PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &result);
  net_log_path_ = result.Append(kDefaultNetLogName);
  net::NetLogCaptureMode capture_mode = net::NetLogCaptureMode::kDefault;
  if (command_line->HasSwitch(switches::kNetLog)) {
    net_log_path_ = command_line->GetSwitchValuePath(switches::kNetLog);
    if (command_line->HasSwitch(switches::kNetLogCaptureMode)) {
      std::string capture_mode_string =
          command_line->GetSwitchValueASCII(switches::kNetLogCaptureMode);
      if (capture_mode_string == kCaptureModeIncludeCookiesAndCredentials) {
        capture_mode = net::NetLogCaptureMode::kIncludeSensitive;
      } else if (capture_mode_string == kCaptureModeIncludeSocketBytes) {
        capture_mode = net::NetLogCaptureMode::kEverything;
      }
    }
    net_log_.reset(new CobaltNetLog(net_log_path_, capture_mode));
    net_log_->StartObserving();
  } else {
    net_log_.reset(new CobaltNetLog(net_log_path_, capture_mode));
  }
#endif

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  // Launch the IO thread.
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  // Without setting a stack size here, the system default will be used
  // which can be quite a bit larger (e.g. 4MB on Linux)
  // Setting it manually keeps it managed.
  thread_options.stack_size = kNetworkModuleStackSize;
  thread_->StartWithOptions(std::move(thread_options));

  base::WaitableEvent creation_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  // Run Network module startup on IO thread,
  // so the network delegate and URL request context are
  // constructed on that thread.
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NetworkModule::OnCreate, base::Unretained(this),
                 &creation_event, http_user_agent_settings.release()));
  // Wait for OnCreate() to run, so we can be sure our members
  // have been constructed.
  creation_event.Wait();
  DCHECK(url_request_context_);
  url_request_context_getter_ = new network::URLRequestContextGetter(
      url_request_context_.get(), thread_.get());

  SetEnableQuicFromPersistentSettings();
  SetEnableHttp2FromPersistentSettings();
  SetEnableHttp3FromPersistentSettings();
  protocol_filter_update_pending_ = true;
  SetProtocolFilterFromPersistentSettings();
}

void NetworkModule::OnCreate(
    base::WaitableEvent* creation_event,
    net::HttpUserAgentSettings* http_user_agent_settings) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  base::CurrentThread::Get()->AddDestructionObserver(this);

  auto network_delegate = std::make_unique<NetworkDelegate>(
      options_.cookie_policy, options_.https_requirement, options_.cors_policy);
  network_delegate_ = network_delegate.get();
  url_request_context_.reset(new URLRequestContext(
      storage_manager_.get(), options_.custom_proxy,
      options_.ignore_certificate_errors, task_runner(),
      options_.persistent_settings,
      std::unique_ptr<net::HttpUserAgentSettings>(http_user_agent_settings),
      std::move(network_delegate)));
  cookie_jar_.reset(new CookieJarImpl(url_request_context_->cookie_store(),
                                      task_runner().get()));
#if defined(DIAL_SERVER)
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enable_dial_service =
      !command_line->HasSwitch(switches::kDisableInAppDial);
#else
  bool enable_dial_service = true;
#endif
  if (enable_dial_service) {
    dial_service_.reset(new DialService());
    dial_service_proxy_ = new DialServiceProxy(dial_service_->AsWeakPtr());
  }
#endif

  net_poster_.reset(new NetPoster(this));

  creation_event->Signal();
}

#if defined(DIAL_SERVER)
void NetworkModule::RestartDialService() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInAppDial)) return;
#endif
  base::WaitableEvent creation_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  // Run Network module startup on IO thread,
  // so the network delegate and URL request context are
  // constructed on that thread.
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&NetworkModule::OnRestartDialService,
                                     base::Unretained(this), &creation_event));
  // Wait for OnCreate() to run, so we can be sure our members
  // have been constructed.
  creation_event.Wait();
}

void NetworkModule::OnRestartDialService(base::WaitableEvent* creation_event) {
  // A new DialService instance cannot be created if any already exists
  // since they will use the same address and it will cause some socket errors.
  // Destroy existing service first.
  dial_service_.reset();
  // Create new dial service
  dial_service_ = std::make_unique<DialService>();
  dial_service_proxy_->ReplaceDialService(dial_service_->AsWeakPtr());
  creation_event->Signal();
}
#endif

void NetworkModule::AddClientHintHeaders(
    net::URLFetcher& url_fetcher, ClientHintHeadersCallType call_type) const {
  if (kEnabledClientHintHeaders & call_type) {
    for (const auto& header : client_hint_headers_) {
      url_fetcher.AddExtraRequestHeader(header);
    }
  }
}

void NetworkModule::StartNetLog() {
#if defined(ENABLE_NETWORK_LOGGING)
  LOG(INFO) << "Starting NetLog capture";
  net_log_->StartObserving();
#endif
}

base::FilePath NetworkModule::StopNetLog() {
#if defined(ENABLE_NETWORK_LOGGING)
  LOG(INFO) << "Stopping NetLog capture";
  net_log_->StopObserving();
#endif
  return net_log_path_;
}

}  // namespace network
}  // namespace cobalt
