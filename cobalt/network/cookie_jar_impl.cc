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

#include "cobalt/network/cookie_jar_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"

namespace cobalt {
namespace network {

namespace {

class CookiesGetter {
 public:
  CookiesGetter(const GURL& origin, net::CookieStore* cookie_store,
                base::TaskRunner* network_task_runner)
      : event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {
    network_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &::net::CookieStore::GetCookieListWithOptionsAsync,
            base::Unretained(cookie_store), origin, net::CookieOptions(),
            net::CookiePartitionKeyCollection::ContainsAll(),
            std::move(base::BindOnce(&CookiesGetter::CompletionCallback,
                                     base::Unretained(this)))));
  }

  net::CookieList WaitForCookies() {
    event_.Wait();
    return std::move(cookies_);
  }

 private:
  void CompletionCallback(const net::CookieAccessResultList& included_cookies,
                          const net::CookieAccessResultList& excluded_list) {
    for (const auto& cookie : included_cookies) {
      cookies_.push_back(cookie.cookie);
    }
    event_.Signal();
  }

  net::CookieList cookies_;
  base::WaitableEvent event_;
};

}  // namespace

CookieJarImpl::CookieJarImpl(net::CookieStore* cookie_store,
                             base::TaskRunner* network_task_runner)
    : cookie_store_(cookie_store), network_task_runner_(network_task_runner) {
  DCHECK(cookie_store_);
  DCHECK(network_task_runner);
}

net::CookieList CookieJarImpl::GetCookies(const GURL& origin) {
  CookiesGetter cookies_getter(origin, cookie_store_, network_task_runner_);
  return cookies_getter.WaitForCookies();
}

void CookieJarImpl::SetCookie(const GURL& origin,
                              const std::string& cookie_line) {
  auto cookie = net::CanonicalCookie::Create(
      origin, cookie_line,
      /*creation_time=*/base::Time::Now(), /*server_time=*/absl::nullopt,
      /*cookie_partition_key=*/absl::nullopt);
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&net::CookieStore::SetCanonicalCookieAsync,
                                base::Unretained(cookie_store_),
                                std::move(cookie), origin, net::CookieOptions(),
                                net::CookieStore::SetCookiesCallback(),
                                /*cookie_access_result=*/absl::nullopt));
}

}  // namespace network
}  // namespace cobalt
