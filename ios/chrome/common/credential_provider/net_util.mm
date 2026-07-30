// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/net_util.h"

#import "base/strings/sys_string_conversions.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"
#import "url/gurl.h"

namespace credential_provider {

NSString* HostForIdentifier(NSString* identifier) {
  if (!identifier) {
    return nil;
  }
  GURL gurl(base::SysNSStringToUTF8(identifier));
  if (gurl.is_valid() && !gurl.host().empty()) {
    return base::SysUTF8ToNSString(gurl.host());
  }
  return identifier;
}

BOOL SecureHostsMatch(NSString* requestedHost, NSString* credentialHost) {
  if (requestedHost.length == 0 || credentialHost.length == 0) {
    return NO;
  }
  if ([requestedHost isEqualToString:credentialHost]) {
    return YES;
  }

  NSString* suffix = [@"." stringByAppendingString:credentialHost];
  if (![requestedHost hasSuffix:suffix]) {
    return NO;
  }

  std::string reqDomain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          base::SysNSStringToUTF8(requestedHost),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string credDomain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          base::SysNSStringToUTF8(credentialHost),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (reqDomain.empty() || credDomain.empty()) {
    return NO;
  }

  return reqDomain == credDomain;
}

}  // namespace credential_provider
