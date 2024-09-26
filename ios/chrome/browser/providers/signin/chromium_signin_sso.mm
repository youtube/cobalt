// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromiumSingleSignOnService : NSObject <SingleSignOnService>
@end

@implementation ChromiumSingleSignOnService
@end

namespace ios {
namespace provider {

id<SingleSignOnService> CreateSSOService() {
  return [[ChromiumSingleSignOnService alloc] init];
}

}  // namespace provider
}  // namespace ios
