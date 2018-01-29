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

#ifndef COBALT_NETWORK_URL_REQUEST_CONTEXT_H_
#define COBALT_NETWORK_URL_REQUEST_CONTEXT_H_

#include <string>

#include "net/base/net_log.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"

namespace cobalt {
namespace storage {
class StorageManager;
}

namespace network {

class URLRequestContext : public net::URLRequestContext {
 public:
  URLRequestContext(storage::StorageManager* storage_manager,
                    const std::string& custom_proxy, net::NetLog* net_log,
                    bool ignore_certificate_errors);
  ~URLRequestContext() override;

  void SetProxy(const std::string& custom_proxy_rules);

 private:
  net::URLRequestContextStorage storage_;
  scoped_refptr<net::CookieMonster::PersistentCookieStore>
      persistent_cookie_store_;
  DISALLOW_COPY_AND_ASSIGN(URLRequestContext);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_URL_REQUEST_CONTEXT_H_
