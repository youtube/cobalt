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

#ifndef COBALT_NETWORK_URL_REQUEST_CONTEXT_H_
#define COBALT_NETWORK_URL_REQUEST_CONTEXT_H_

#include <map>
#include <memory>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "cobalt/network/custom/url_fetcher.h"
#include "cobalt/network/disk_cache/resource_type.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "net/base/http_user_agent_settings.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/console/command_manager.h"  // nogncheck
#endif                                             // ENABLE_DEBUGGER

namespace cobalt {
namespace storage {
class StorageManager;
}

namespace network {

class URLRequestContext {
 public:
  URLRequestContext(
      storage::StorageManager* storage_manager, const std::string& custom_proxy,
      bool ignore_certificate_errors,
      scoped_refptr<base::SequencedTaskRunner> network_task_runner,
      persistent_storage::PersistentSettings* persistent_settings,
      std::unique_ptr<net::HttpUserAgentSettings> http_user_agent_settings,
      std::unique_ptr<net::NetworkDelegate> network_delegate);
  ~URLRequestContext();

  std::unique_ptr<net::URLRequest> CreateRequest(
      const GURL& url, net::RequestPriority priority,
      net::URLRequest::Delegate* delegate);

  void SetProxy(const std::string& custom_proxy_rules);

  void SetEnableQuic(bool enable_quic);

  bool using_http_cache();

  void UpdateCacheSizeSetting(disk_cache::ResourceType type, uint32_t bytes);
  void ValidateCachePersistentSettings();

  void AssociateKeyWithResourceType(const std::string& key,
                                    disk_cache::ResourceType resource_type);
  disk_cache::ResourceType GetType(const std::string& key);

  net::CookieStore* cookie_store() {
    return url_request_context_->cookie_store();
  }

  net::HttpTransactionFactory* http_transaction_factory() const {
    return url_request_context_->http_transaction_factory();
  }

  net::URLRequestContext* url_request_context() {
    return url_request_context_.get();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<net::CookieMonster::PersistentCookieStore>
      persistent_cookie_store_;

  bool using_http_cache_;

#if defined(ENABLE_DEBUGGER)
  // Command handler object for toggling the input fuzzer on/off.
  debug::console::ConsoleCommandManager::CommandHandler
      quic_toggle_command_handler_;

  // Toggles the input fuzzer on/off.  Ignores the parameter.
  void OnQuicToggle(const std::string&);
#endif  // defined(ENABLE_DEBUGGER)

  // Persistent settings module for Cobalt disk cache quotas
  std::unique_ptr<cobalt::persistent_storage::PersistentSettings>
      cache_persistent_settings_;
  std::map<uint32_t, disk_cache::ResourceType> url_resource_type_map_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContext);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_URL_REQUEST_CONTEXT_H_
