// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_UTIL_H_
#define NET_COOKIES_COOKIE_UTIL_H_
#pragma once

#include <string>

#include "net/base/net_export.h"

class GURL;

namespace net {
namespace cookie_util {

// Returns the effective TLD+1 for a given host. This only makes sense for http
// and https schemes. For other schemes, the host will be returned unchanged
// (minus any leading period).
NET_EXPORT std::string GetEffectiveDomain(const std::string& scheme,
                                          const std::string& host);

// Determine the actual cookie domain based on the domain string passed
// (if any) and the URL from which the cookie came.
// On success returns true, and sets cookie_domain to either a
//   -host cookie domain (ex: "google.com")
//   -domain cookie domain (ex: ".google.com")
NET_EXPORT bool GetCookieDomainWithString(const GURL& url,
                                          const std::string& domain_string,
                                          std::string* result);

// Returns true if a domain string represents a host-only cookie,
// i.e. it doesn't begin with a leading '.' character.
NET_EXPORT bool DomainIsHostOnly(const std::string& domain_string);

}  // namspace cookie_util
}  // namespace net

#endif  // NET_COOKIES_COOKIE_UTIL_H_
