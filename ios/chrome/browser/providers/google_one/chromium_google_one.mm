// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"

namespace {
id<GoogleOneControllerFactory> g_google_one_controller_factory;
}

@implementation GoogleOneConfiguration

@end

namespace ios {
namespace provider {

id<GoogleOneController> CreateGoogleOneController(
    GoogleOneConfiguration* configuration) {
  return [g_google_one_controller_factory
      createControllerWithConfiguration:configuration];
}

void SetGoogleOneControllerFactory(id<GoogleOneControllerFactory> factory) {
  g_google_one_controller_factory = factory;
}

BOOL CanHandleGoogleOneURL(NSURL* url) {
  if ([g_google_one_controller_factory
          respondsToSelector:@selector(canHandleURL:)]) {
    return [g_google_one_controller_factory canHandleURL:url];
  }
  return NO;
}

NSString* GoogleOneEmailFromURL(NSURL* url) {
  if ([g_google_one_controller_factory
          respondsToSelector:@selector(emailFromURL:)]) {
    return [g_google_one_controller_factory emailFromURL:url];
  }
  return nil;
}

}  // namespace provider
}  // namespace ios
