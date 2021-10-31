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

#ifndef COBALT_NETWORK_NETWORK_MODULE_H_
#define COBALT_NETWORK_NETWORK_MODULE_H_

#include <memory>
#include <string>

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/network/cobalt_net_log.h"
#include "cobalt/network/cookie_jar_impl.h"
#include "cobalt/network/net_poster.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/url_request_context.h"
#include "cobalt/network/url_request_context_getter.h"
#include "net/base/static_cookie_policy.h"
#include "url/gurl.h"
#if defined(DIAL_SERVER)
// Including this header causes a link error on Windows, since we
// don't have StreamListenSocket.
#include "net/dial/dial_service.h"
#endif
#include "net/url_request/http_user_agent_settings.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace cobalt {

namespace storage {
class StorageManager;
}  // namespace storage

namespace network {

class NetworkSystem;
// NetworkModule wraps various networking-related components such as
// a URL request context. This is owned by BrowserModule.
class NetworkModule {
 public:
  struct Options {
    Options()
        : cookie_policy(net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES),
          ignore_certificate_errors(false),
          https_requirement(network::kHTTPSRequired),
          preferred_language("en-US"),
          max_network_delay(0) {}
    net::StaticCookiePolicy::Type cookie_policy;
    bool ignore_certificate_errors;
    HTTPSRequirement https_requirement;
    std::string preferred_language;
    std::string custom_proxy;
    SbTime max_network_delay;
  };

  // Simple constructor intended to be used only by tests.
  explicit NetworkModule(const Options& options = Options());

  // Constructor for production use.
  NetworkModule(const std::string& user_agent_string,
                storage::StorageManager* storage_manager,
                base::EventDispatcher* event_dispatcher,
                const Options& options = Options());
  ~NetworkModule();

  URLRequestContext* url_request_context() const {
    return url_request_context_.get();
  }
  NetworkDelegate* network_delegate() const { return network_delegate_.get(); }
  std::string GetUserAgent() const;
  const std::string& preferred_language() const {
    return options_.preferred_language;
  }
  SbTime max_network_delay() const { return options_.max_network_delay; }
  scoped_refptr<URLRequestContextGetter> url_request_context_getter() const {
    return url_request_context_getter_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return thread_->task_runner();
  }
  storage::StorageManager* storage_manager() const { return storage_manager_; }
  network_bridge::CookieJar* cookie_jar() const { return cookie_jar_.get(); }
  network_bridge::PostSender GetPostSender() const;
#if defined(DIAL_SERVER)
  scoped_refptr<net::DialServiceProxy> dial_service_proxy() const {
    return dial_service_proxy_;
  }
#endif
  void SetProxy(const std::string& custom_proxy_rules);

  void SetEnableQuic(bool enable_quic);

 private:
  void Initialize(const std::string& user_agent_string,
                  base::EventDispatcher* event_dispatcher);
  void OnCreate(base::WaitableEvent* creation_event);
  std::unique_ptr<network_bridge::NetPoster> CreateNetPoster();

  storage::StorageManager* storage_manager_;
  std::unique_ptr<base::Thread> thread_;
  std::unique_ptr<URLRequestContext> url_request_context_;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;
  std::unique_ptr<NetworkDelegate> network_delegate_;
  std::unique_ptr<NetworkSystem> network_system_;
  std::unique_ptr<net::HttpUserAgentSettings> http_user_agent_settings_;
  std::unique_ptr<network_bridge::CookieJar> cookie_jar_;
#if defined(DIAL_SERVER)
  std::unique_ptr<net::DialService> dial_service_;
  scoped_refptr<net::DialServiceProxy> dial_service_proxy_;
#endif
  std::unique_ptr<network_bridge::NetPoster> net_poster_;
  std::unique_ptr<CobaltNetLog> net_log_;
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(NetworkModule);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_NETWORK_MODULE_H_
