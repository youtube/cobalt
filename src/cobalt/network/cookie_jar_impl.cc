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
  CookiesGetter() : event_(true, false) {}

  const std::string& WaitForCookies() {
    event_.Wait();
    return cookies_;
  }

  void GetCookiesAsync(const std::string& cookies) {
    cookies_ = cookies;
    event_.Signal();
  }

 private:
  std::string cookies_;
  base::WaitableEvent event_;
};

}  // namespace

CookieJarImpl::CookieJarImpl(net::CookieStore* cookie_store)
    : cookie_store_(cookie_store) {
  DCHECK(cookie_store_);
}

std::string CookieJarImpl::GetCookies(const GURL& origin) {
  CookiesGetter cookies_getter;
  cookie_store_->GetCookiesWithOptionsAsync(
      origin, net::CookieOptions(),
      base::Bind(&CookiesGetter::GetCookiesAsync,
                 base::Unretained(&cookies_getter)));
  return cookies_getter.WaitForCookies();
}

void CookieJarImpl::SetCookie(const GURL& origin,
                              const std::string& cookie_line) {
  cookie_store_->SetCookieWithOptionsAsync(
      origin, cookie_line, net::CookieOptions(), base::Callback<void(bool)>());
}

}  // namespace network
}  // namespace cobalt
