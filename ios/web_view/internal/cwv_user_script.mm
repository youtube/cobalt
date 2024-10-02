// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_user_script.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVUserScript

@synthesize source = _source;

- (nonnull instancetype)initWithSource:(nonnull NSString*)source {
  self = [super init];
  if (self) {
    _source = [source copy];
  }
  return self;
}

@end
