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

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
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
#endif
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
  network_delegate_.reset();
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
    thread_->message_loop()->task_runner()->WaitForFence();
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
  DCHECK(http_user_agent_settings_);
  return http_user_agent_settings_->GetUserAgent();
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

void NetworkModule::SetEnableQuic(bool enable_quic) {
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestContext::SetEnableQuic,
                 base::Unretained(url_request_context_.get()), enable_quic));
}

void NetworkModule::SetEnableClientHintHeadersFlagsFromPersistentSettings() {
  // Called on initialization and when the persistent setting is changed.
  if (options_.persistent_settings != nullptr) {
    enable_client_hint_headers_flags_.store(
        options_.persistent_settings->GetPersistentSettingAsInt(
            kClientHintHeadersEnabledPersistentSettingsKey, 0));
  }
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
  http_user_agent_settings_.reset(new net::StaticHttpUserAgentSettings(
      options_.preferred_language, user_agent_string));

  SetEnableClientHintHeadersFlagsFromPersistentSettings();

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string custom_user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    http_user_agent_settings_.reset(new net::StaticHttpUserAgentSettings(
        options_.preferred_language, custom_user_agent));
  }

  if (command_line->HasSwitch(switches::kMaxNetworkDelay)) {
    base::StringToInt64(
        command_line->GetSwitchValueASCII(switches::kMaxNetworkDelay),
        &options_.max_network_delay);
  }

#if defined(ENABLE_NETWORK_LOGGING)
  if (command_line->HasSwitch(switches::kNetLog)) {
    // If this is not a valid path, net logs will be sent to VLOG(1).
    base::FilePath net_log_path =
        command_line->GetSwitchValuePath(switches::kNetLog);
    net::NetLogCaptureMode capture_mode;
    if (command_line->HasSwitch(switches::kNetLogCaptureMode)) {
      std::string capture_mode_string =
          command_line->GetSwitchValueASCII(switches::kNetLogCaptureMode);
      if (capture_mode_string == kCaptureModeIncludeCookiesAndCredentials) {
        capture_mode = net::NetLogCaptureMode::IncludeCookiesAndCredentials();
      } else if (capture_mode_string == kCaptureModeIncludeSocketBytes) {
        capture_mode = net::NetLogCaptureMode::IncludeSocketBytes();
      }
    }
    net_log_.reset(new CobaltNetLog(net_log_path, capture_mode));
  }
#endif

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  // Launch the IO thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_options.stack_size = 256 * 1024;
  thread_options.priority = base::ThreadPriority::NORMAL;
  thread_->StartWithOptions(thread_options);

  base::WaitableEvent creation_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  // Run Network module startup on IO thread,
  // so the network delegate and URL request context are
  // constructed on that thread.
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&NetworkModule::OnCreate, base::Unretained(this),
                            &creation_event));
  // Wait for OnCreate() to run, so we can be sure our members
  // have been constructed.
  creation_event.Wait();
  DCHECK(url_request_context_);
  url_request_context_getter_ = new network::URLRequestContextGetter(
      url_request_context_.get(), thread_.get());
}

void NetworkModule::OnCreate(base::WaitableEvent* creation_event) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  base::MessageLoopCurrent::Get()->AddDestructionObserver(this);

  net::NetLog* net_log = NULL;
#if defined(ENABLE_NETWORK_LOGGING)
  net_log = net_log_.get();
#endif
  url_request_context_.reset(
      new URLRequestContext(storage_manager_.get(), options_.custom_proxy,
                            net_log, options_.ignore_certificate_errors,
                            task_runner(), options_.persistent_settings));
  network_delegate_.reset(new NetworkDelegate(options_.cookie_policy,
                                              options_.https_requirement,
                                              options_.cors_policy));
  url_request_context_->set_http_user_agent_settings(
      http_user_agent_settings_.get());
  url_request_context_->set_network_delegate(network_delegate_.get());
  cookie_jar_.reset(new CookieJarImpl(url_request_context_->cookie_store(),
                                      task_runner().get()));
#if defined(DIAL_SERVER)
  dial_service_.reset(new net::DialService());
  dial_service_proxy_ = new net::DialServiceProxy(dial_service_->AsWeakPtr());
#endif

  net_poster_.reset(new NetPoster(this));

  creation_event->Signal();
}

void NetworkModule::AddClientHintHeaders(
    net::URLFetcher& url_fetcher, ClientHintHeadersCallType call_type) const {
  if (enable_client_hint_headers_flags_.load() & call_type) {
    for (const auto& header : client_hint_headers_) {
      url_fetcher.AddExtraRequestHeader(header);
    }
  }
}

}  // namespace network
}  // namespace cobalt
