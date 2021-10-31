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
        base::Bind(
            &::net::CookieStore::GetCookieListWithOptionsAsync,
            base::Unretained(cookie_store), origin, net::CookieOptions(),
            base::Passed(base::BindOnce(&CookiesGetter::CompletionCallback,
                                        base::Unretained(this)))));
  }

  net::CookieList WaitForCookies() {
    event_.Wait();
    return std::move(cookies_);
  }

 private:
  void CompletionCallback(const net::CookieList& cookies) {
    cookies_ = cookies;
    for (auto i : cookies_) {
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
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&net::CookieStore::SetCookieWithOptionsAsync,
                 base::Unretained(cookie_store_), origin, cookie_line,
                 net::CookieOptions(), base::Callback<void(bool)>()));
}

}  // namespace network
}  // namespace cobalt
