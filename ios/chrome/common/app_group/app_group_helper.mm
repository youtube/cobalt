// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_helper.h"
#import "base/check.h"
#import "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppGroupHelper

+ (NSString*)applicationGroup {
  NSBundle* bundle = [NSBundle mainBundle];
  NSString* group = [bundle objectForInfoDictionaryKey:@"KSApplicationGroup"];
  if (![group length]) {
    return [NSString stringWithFormat:@"group.%s.chrome",
                                      BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX), nil];
  }
  return group;
}

+ (NSUserDefaults*)groupUserDefaults {
  NSString* applicationGroup = [AppGroupHelper applicationGroup];
  if (applicationGroup) {
    NSUserDefaults* defaults =
        [[NSUserDefaults alloc] initWithSuiteName:applicationGroup];
    if (defaults)
      return defaults;
  }

  // On a device, the entitlements should always provide an application group to
  // the application. This is not the case on simulator.
  DCHECK(TARGET_IPHONE_SIMULATOR);
  return [NSUserDefaults standardUserDefaults];
}

@end
