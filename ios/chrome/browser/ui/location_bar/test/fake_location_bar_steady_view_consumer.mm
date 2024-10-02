// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/test/fake_location_bar_steady_view_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeLocationBarSteadyViewConsumer

- (void)updateLocationText:(NSString*)string clipTail:(BOOL)clipTail {
  _locationText = string;
  _clipTail = clipTail;
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  _icon = icon;
  _statusText = statusText;
}

- (void)updateLocationShareable:(BOOL)shareable {
  _locationShareable = shareable;
}

- (void)updateAfterNavigatingToNTP {
}

@end
