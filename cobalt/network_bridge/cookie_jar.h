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

#ifndef COBALT_NETWORK_BRIDGE_COOKIE_JAR_H_
#define COBALT_NETWORK_BRIDGE_COOKIE_JAR_H_

#include <string>

#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

namespace cobalt {
namespace network_bridge {

// An interface to allow interaction with the cookie store, which is owned
// by the network module, without requiring a dependency on the network module.
class CookieJar {
 public:
  CookieJar() {}
  virtual ~CookieJar() {}
  // Return all the cookies for this origin in a string, delimited by ';'
  virtual net::CookieList GetCookies(const GURL& origin) = 0;
  // Set a single cookie for the origin. Format should be as per
  // CookieStore::SetCookieWithOptionsAsync()
  virtual void SetCookie(const GURL& origin,
                         const std::string& cookie_line) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieJar);
};

}  // namespace network_bridge
}  // namespace cobalt

#endif  // COBALT_NETWORK_BRIDGE_COOKIE_JAR_H_
