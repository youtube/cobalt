/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NETWORK_NETWORK_MODULE_H_
#define NETWORK_NETWORK_MODULE_H_

#include <string>

#include "base/message_loop_proxy.h"
#include "base/object_watcher_shell.h"
#include "base/threading/thread.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/network/cookie_jar_impl.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/url_request_context.h"
#include "cobalt/network/url_request_context_getter.h"
#include "cobalt/network/user_agent.h"
#include "googleurl/src/gurl.h"
#include "net/base/static_cookie_policy.h"

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
          require_https(true),
          preferred_language("en-US") {}
    net::StaticCookiePolicy::Type cookie_policy;
    bool require_https;
    std::string preferred_language;
  };

  // Default constructor, for use by unit tests.
  NetworkModule();
  explicit NetworkModule(const Options& options);
  // Constructor for production use.
  NetworkModule(storage::StorageManager* storage_manager,
                base::EventDispatcher* event_dispatcher,
                const Options& options);
  ~NetworkModule();

  URLRequestContext* url_request_context() const {
    return url_request_context_.get();
  }
  NetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }
  const std::string& user_agent() const { return user_agent_->user_agent(); }
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
  cookies::CookieJar* cookie_jar() const { return cookie_jar_.get(); }

 private:
  void Initialize(base::EventDispatcher* event_dispatcher);
  void OnCreate(base::WaitableEvent* creation_event);

  storage::StorageManager* storage_manager_;
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<base::ObjectWatchMultiplexer> object_watch_multiplexer_;
  scoped_ptr<URLRequestContext> url_request_context_;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<NetworkDelegate> network_delegate_;
  scoped_ptr<UserAgent> user_agent_;
  scoped_ptr<NetworkSystem> network_system_;
  scoped_ptr<cookies::CookieJar> cookie_jar_;
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(NetworkModule);
};

}  // namespace network
}  // namespace cobalt

#endif  // NETWORK_NETWORK_MODULE_H_
