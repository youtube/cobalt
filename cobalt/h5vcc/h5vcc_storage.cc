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

#include "cobalt/h5vcc/h5vcc_storage.h"

#include "cobalt/storage/storage_manager.h"
#include "net/url_request/url_request_context.h"

namespace cobalt {
namespace h5vcc {

H5vccStorage::H5vccStorage(network::NetworkModule* network_module)
    : network_module_(network_module) {}

void H5vccStorage::ClearCookies() {
  net::CookieStore* cookie_store =
      network_module_->url_request_context()->cookie_store();
  auto* cookie_monster = static_cast<net::CookieMonster*>(cookie_store);
  network_module_->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&net::CookieMonster::DeleteAllMatchingInfoAsync,
                 base::Unretained(cookie_monster), net::CookieDeletionInfo(),
                 base::Passed(net::CookieStore::DeleteCallback())));
}

void H5vccStorage::Flush(const base::Optional<bool>& sync) {
  if (sync.value_or(false) == true) {
    DLOG(WARNING) << "Synchronous flush is not supported.";
  }

  network_module_->storage_manager()->FlushNow(base::Closure());
}

bool H5vccStorage::GetCookiesEnabled() {
  return network_module_->network_delegate()->cookies_enabled();
}

void H5vccStorage::SetCookiesEnabled(bool enabled) {
  network_module_->network_delegate()->set_cookies_enabled(enabled);
}

}  // namespace h5vcc
}  // namespace cobalt
