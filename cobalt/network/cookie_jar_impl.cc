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

#include "cobalt/network/cookie_jar_impl.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"

namespace cobalt {
namespace network {

namespace {

class CookiesGetter {
 public:
  CookiesGetter(const GURL& origin, net::CookieStore* cookie_store,
                MessageLoop* get_cookies_message_loop)
      : event_(true, false) {
    get_cookies_message_loop->PostTask(
        FROM_HERE, base::Bind(&net::CookieStore::GetCookiesWithOptionsAsync,
                              cookie_store, origin, net::CookieOptions(),
                              base::Bind(&CookiesGetter::CompletionCallback,
                                         base::Unretained(this))));
  }

  const std::string& WaitForCookies() {
    event_.Wait();
    return cookies_;
  }

 private:
  void CompletionCallback(const std::string& cookies) {
    cookies_ = cookies;
    event_.Signal();
  }

  std::string cookies_;
  base::WaitableEvent event_;
};

}  // namespace

CookieJarImpl::CookieJarImpl(net::CookieStore* cookie_store)
    : get_cookies_thread_("CookiesGetter"), cookie_store_(cookie_store) {
  DCHECK(cookie_store_);
  get_cookies_thread_.Start();
}

std::string CookieJarImpl::GetCookies(const GURL& origin) {
  CookiesGetter cookies_getter(
      origin, cookie_store_, get_cookies_thread_.message_loop());
  return cookies_getter.WaitForCookies();
}

void CookieJarImpl::SetCookie(const GURL& origin,
                              const std::string& cookie_line) {
  cookie_store_->SetCookieWithOptionsAsync(
      origin, cookie_line, net::CookieOptions(), base::Callback<void(bool)>());
}

}  // namespace network
}  // namespace cobalt
