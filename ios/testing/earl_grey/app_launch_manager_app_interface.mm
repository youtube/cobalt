// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/app_launch_manager_app_interface.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppLaunchManagerAppInterface

+ (int)processIdentifier {
  return NSProcessInfo.processInfo.processIdentifier;
}

@end
