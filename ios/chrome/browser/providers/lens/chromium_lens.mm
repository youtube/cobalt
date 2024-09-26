// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <ostream>

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {
namespace {

// The domain for NSErrors.
NSErrorDomain const kChromiumLensProviderErrorDomain =
    @"kChromiumLensProviderErrorDomain";

// The error codes for kChromiumLensProviderErrorDomain.
enum ChromiumLensProviderErrors : NSInteger {
  kChromiumLensProviderErrorNotImplemented,
};

}

using LensWebParamsCallback =
    base::OnceCallback<void(web::NavigationManager::WebLoadParams)>;

id<ChromeLensController> NewChromeLensController(LensConfiguration* config) {
  // Lens is not supported in Chromium.
  return nil;
}

bool IsLensSupported() {
  // Lens is not supported in Chromium.
  return false;
}

bool IsLensWebResultsURL(const GURL& url) {
  // Lens is not supported in Chromium.
  return false;
}

absl::optional<LensEntrypoint> GetLensEntryPointFromURL(const GURL& url) {
  return absl::nullopt;
}

void GenerateLensLoadParamsAsync(LensQuery* query,
                                 LensWebParamsCallback completion) {
  NOTREACHED() << "Lens is not supported.";
}

}  // namespace provider
}  // namespace ios
