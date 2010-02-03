// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/static_cookie_policy.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace net {

bool StaticCookiePolicy::CanGetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies) {
  switch (type_) {
    case StaticCookiePolicy::ALLOW_ALL_COOKIES:
      return true;
    case StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES:
      return true;
    case StaticCookiePolicy::BLOCK_ALL_COOKIES:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool StaticCookiePolicy::CanSetCookie(const GURL& url,
                                      const GURL& first_party_for_cookies) {
  switch (type_) {
    case StaticCookiePolicy::ALLOW_ALL_COOKIES:
      return true;
    case StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES:
      if (first_party_for_cookies.is_empty())
        return true;  // Empty first-party URL indicates a first-party request.
      return net::RegistryControlledDomainService::SameDomainOrHost(
          url, first_party_for_cookies);
    case StaticCookiePolicy::BLOCK_ALL_COOKIES:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace net
