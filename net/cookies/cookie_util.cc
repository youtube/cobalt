// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_util.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

namespace net {
namespace cookie_util {

bool DomainIsHostOnly(const std::string& domain_string) {
  return (domain_string.empty() || domain_string[0] != '.');
}

std::string GetEffectiveDomain(const std::string& scheme,
                               const std::string& host) {
  if (scheme == "http" || scheme == "https")
    return RegistryControlledDomainService::GetDomainAndRegistry(host);

  if (!DomainIsHostOnly(host))
    return host.substr(1);
  return host;
}

bool GetCookieDomainWithString(const GURL& url,
                               const std::string& domain_string,
                               std::string* result) {
  const std::string url_host(url.host());

  // If no domain was specified in the domain string, default to a host cookie.
  // We match IE/Firefox in allowing a domain=IPADDR if it matches the url
  // ip address hostname exactly.  It should be treated as a host cookie.
  if (domain_string.empty() ||
      (url.HostIsIPAddress() && url_host == domain_string)) {
    *result = url_host;
    DCHECK(DomainIsHostOnly(*result));
    return true;
  }

  // Get the normalized domain specified in cookie line.
  url_canon::CanonHostInfo ignored;
  std::string cookie_domain(CanonicalizeHost(domain_string, &ignored));
  if (cookie_domain.empty())
    return false;
  if (cookie_domain[0] != '.')
    cookie_domain = "." + cookie_domain;

  // Ensure |url| and |cookie_domain| have the same domain+registry.
  const std::string url_scheme(url.scheme());
  const std::string url_domain_and_registry(
      GetEffectiveDomain(url_scheme, url_host));
  if (url_domain_and_registry.empty())
    return false;  // IP addresses/intranet hosts can't set domain cookies.
  const std::string cookie_domain_and_registry(
      GetEffectiveDomain(url_scheme, cookie_domain));
  if (url_domain_and_registry != cookie_domain_and_registry)
    return false;  // Can't set a cookie on a different domain + registry.

  // Ensure |url_host| is |cookie_domain| or one of its subdomains.  Given that
  // we know the domain+registry are the same from the above checks, this is
  // basically a simple string suffix check.
  const bool is_suffix = (url_host.length() < cookie_domain.length()) ?
      (cookie_domain != ("." + url_host)) :
      (url_host.compare(url_host.length() - cookie_domain.length(),
                        cookie_domain.length(), cookie_domain) != 0);
  if (is_suffix)
    return false;

  *result = cookie_domain;
  return true;
}

}  // namespace cookie_utils
}  // namespace net

