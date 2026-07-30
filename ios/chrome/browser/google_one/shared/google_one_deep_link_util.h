// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_DEEP_LINK_UTIL_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_DEEP_LINK_UTIL_H_

#import <Foundation/Foundation.h>

#import "google_apis/gaia/gaia_id.h"

class GURL;
@protocol SystemIdentity;

// Returns true if `url` is a handled Google One deep link URL.
// If handled and `out_url` is non-null, populates `out_url` with the normalized
// HTTPS GURL.
bool IsGoogleOneDeepLinkURL(const GURL& url, GURL* out_url);

// Returns the account string specified in the Google One deep link `url`.
NSString* GoogleOneAccountFromURL(const GURL& url);

// Returns the SystemIdentity matching `account_param` on the device, or nil if
// no match is found.
id<SystemIdentity> FindIdentityForGoogleOneAccount(NSString* account_param);

// Returns the GaiaId matching `account_param` on the device, or an empty GaiaId
// if no match is found.
GaiaId FindGaiaIdForGoogleOneAccount(NSString* account_param);

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_SHARED_GOOGLE_ONE_DEEP_LINK_UTIL_H_
