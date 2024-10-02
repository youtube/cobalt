// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/fake_observable_boolean.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeObservableBoolean

@synthesize value = _value;
@synthesize observer = _observer;

- (void)setValue:(BOOL)value {
  bool changed = value != _value;
  _value = value;
  if (changed)
    [self.observer booleanDidChange:self];
}

@end

@implementation TestBooleanObserver

@synthesize updateCount = _updateCount;

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  self.updateCount++;
}

@end
