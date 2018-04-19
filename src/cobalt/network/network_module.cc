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

#include "cobalt/network/network_module.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/network/network_system.h"
#include "cobalt/network/switches.h"
#include "net/url_request/static_http_user_agent_settings.h"

namespace cobalt {
namespace network {

NetworkModule::NetworkModule(const Options& options)
    : storage_manager_(NULL), options_(options) {
  Initialize("Null user agent string.", NULL);
}

NetworkModule::NetworkModule(const std::string& user_agent_string,
                             storage::StorageManager* storage_manager,
                             base::EventDispatcher* event_dispatcher,
                             const Options& options)
    : storage_manager_(storage_manager), options_(options) {
  Initialize(user_agent_string, event_dispatcher);
}

NetworkModule::~NetworkModule() {
  // Order of destruction is important here.
  // URLRequestContext and NetworkDelegate must be destroyed on the IO thread.
  // The ObjectWatchMultiplexer must be destroyed last.  (The sockets owned
  // by URLRequestContext will destroy their ObjectWatchers, which need the
  // multiplexer.)
  url_request_context_getter_ = NULL;
#if defined(DIAL_SERVER)
  dial_service_proxy_ = NULL;
  message_loop_proxy()->DeleteSoon(FROM_HERE, dial_service_.release());
#endif

  message_loop_proxy()->DeleteSoon(FROM_HERE, cookie_jar_.release());
  message_loop_proxy()->DeleteSoon(FROM_HERE, net_poster_.release());
  message_loop_proxy()->DeleteSoon(FROM_HERE, url_request_context_.release());
  message_loop_proxy()->DeleteSoon(FROM_HERE, network_delegate_.release());

  // This will run the above task, and then stop the thread.
  thread_.reset(NULL);
#if !defined(OS_STARBOARD)
  object_watch_multiplexer_.reset(NULL);
#endif
  network_system_.reset(NULL);
}

const std::string& NetworkModule::GetUserAgent() const {
  return http_user_agent_settings_->GetUserAgent();
}

network_bridge::PostSender NetworkModule::GetPostSender() const {
  return base::Bind(&network_bridge::NetPoster::Send,
                    base::Unretained(net_poster_.get()));
}

void NetworkModule::SetProxy(const std::string& custom_proxy_rules) {
  message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(&URLRequestContext::SetProxy,
                            base::Unretained(url_request_context_.get()),
                            custom_proxy_rules));
}

void NetworkModule::Initialize(const std::string& user_agent_string,
                               base::EventDispatcher* event_dispatcher) {
  thread_.reset(new base::Thread("NetworkModule"));
#if !defined(OS_STARBOARD)
  object_watch_multiplexer_.reset(new base::ObjectWatchMultiplexer());
#endif
  network_system_ = NetworkSystem::Create(event_dispatcher);
  http_user_agent_settings_.reset(new net::StaticHttpUserAgentSettings(
      options_.preferred_language, "utf-8", user_agent_string));

#if defined(ENABLE_NETWORK_LOGGING)
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNetLog)) {
    // If this is not a valid path, net logs will be sent to VLOG(1).
    FilePath net_log_path = command_line->GetSwitchValuePath(switches::kNetLog);
    net::NetLog::LogLevel net_log_level = net::NetLog::LOG_BASIC;
    if (command_line->HasSwitch(switches::kNetLogLevel)) {
      std::string level_string =
          command_line->GetSwitchValueASCII(switches::kNetLogLevel);
      int level_int = 0;
      if (base::StringToInt(level_string, &level_int) &&
          level_int >= net::NetLog::LOG_ALL &&
          level_int <= net::NetLog::LOG_BASIC) {
        net_log_level = static_cast<net::NetLog::LogLevel>(level_int);
      }
    }
    DLOG(INFO) << net_log_level;
    net_log_.reset(new CobaltNetLog(net_log_path, net_log_level));
  }

#endif

  // Launch the IO thread.
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_IO;
  thread_options.stack_size = 256 * 1024;
  // It was found that setting the thread priority here to high could result
  // in an increase in unresponsiveness and input latency on single-core
  // devices.  Keeping it at normal so that it doesn't take precedence over
  // user interaction processing.
  thread_options.priority = base::kThreadPriority_Normal;
  thread_->StartWithOptions(thread_options);

  base::WaitableEvent creation_event(true, false);
  // Run Network module startup on IO thread,
  // so the network delegate and URL request context are
  // constructed on that thread.
  message_loop_proxy()->PostTask(
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
  DCHECK(message_loop_proxy()->BelongsToCurrentThread());

  net::NetLog* net_log = NULL;
#if defined(ENABLE_NETWORK_LOGGING)
  net_log = net_log_.get();
#endif
  url_request_context_.reset(
      new URLRequestContext(storage_manager_, options_.custom_proxy, net_log,
                            options_.ignore_certificate_errors));
  network_delegate_.reset(
      new NetworkDelegate(options_.cookie_policy, options_.https_requirement));
  url_request_context_->set_http_user_agent_settings(
      http_user_agent_settings_.get());
  url_request_context_->set_network_delegate(network_delegate_.get());
  cookie_jar_.reset(new CookieJarImpl(url_request_context_->cookie_store()));
#if defined(DIAL_SERVER)
  dial_service_.reset(new net::DialService());
  dial_service_proxy_ = new net::DialServiceProxy(dial_service_->AsWeakPtr());
#endif

  net_poster_.reset(new NetPoster(this));

  creation_event->Signal();
}

}  // namespace network
}  // namespace cobalt
