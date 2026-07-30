// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_NET_UTIL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_NET_UTIL_H_

#import <Foundation/Foundation.h>

namespace credential_provider {

// Returns `YES` if the requested host shares the exact same verified eTLD+1
// (domain and registry) as the target credential host, taking private
// registries into account, and verifies that `credentialHost` is a
// registrable domain suffix of (or equal to) `requestedHost`.
BOOL SecureHostsMatch(NSString* requestedHost, NSString* credentialHost);

// Returns the host of the given URL/identifier, or falls back to the identifier
// if the host is empty/invalid.
NSString* HostForIdentifier(NSString* identifier);

}  // namespace credential_provider

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_NET_UTIL_H_
