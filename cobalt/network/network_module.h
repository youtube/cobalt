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

#ifndef COBALT_NETWORK_NETWORK_MODULE_H_
#define COBALT_NETWORK_NETWORK_MODULE_H_

#include <string>

#include "base/message_loop_proxy.h"
#if !defined(OS_STARBOARD)
#include "base/object_watcher_shell.h"
#endif
#include "base/threading/thread.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/network/cobalt_net_log.h"
#include "cobalt/network/cookie_jar_impl.h"
#include "cobalt/network/net_poster.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/url_request_context.h"
#include "cobalt/network/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/base/static_cookie_policy.h"
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
        : cookie_policy(
              net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES),
          ignore_certificate_errors(false),
          https_requirement(network::kHTTPSRequired),
          preferred_language("en-US") {}
    net::StaticCookiePolicy::Type cookie_policy;
    bool ignore_certificate_errors;
    HTTPSRequirement https_requirement;
    std::string preferred_language;
    std::string custom_proxy;
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
  NetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }
  const std::string& GetUserAgent() const;
  const std::string& preferred_language() const {
    return options_.preferred_language;
  }
  scoped_refptr<URLRequestContextGetter> url_request_context_getter() const {
    return url_request_context_getter_;
  }
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy() const {
    return thread_->message_loop_proxy();
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

 private:
  void Initialize(const std::string& user_agent_string,
                  base::EventDispatcher* event_dispatcher);
  void OnCreate(base::WaitableEvent* creation_event);
  scoped_ptr<network_bridge::NetPoster> CreateNetPoster();

  storage::StorageManager* storage_manager_;
  scoped_ptr<base::Thread> thread_;
#if !defined(OS_STARBOARD)
  scoped_ptr<base::ObjectWatchMultiplexer> object_watch_multiplexer_;
#endif
  scoped_ptr<URLRequestContext> url_request_context_;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<NetworkDelegate> network_delegate_;
  scoped_ptr<NetworkSystem> network_system_;
  scoped_ptr<net::HttpUserAgentSettings> http_user_agent_settings_;
  scoped_ptr<network_bridge::CookieJar> cookie_jar_;
#if defined(DIAL_SERVER)
  scoped_ptr<net::DialService> dial_service_;
  scoped_refptr<net::DialServiceProxy> dial_service_proxy_;
#endif
  scoped_ptr<network_bridge::NetPoster> net_poster_;
  scoped_ptr<CobaltNetLog> net_log_;
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(NetworkModule);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_NETWORK_MODULE_H_
