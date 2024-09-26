// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeBroadcastObserverInterface::~ChromeBroadcastObserverInterface() = default;

@implementation ChromeBroadcastOberverBridge
@synthesize observer = _observer;

- (instancetype)initWithObserver:(ChromeBroadcastObserverInterface*)observer {
  if (self = [super init]) {
    _observer = observer;
    DCHECK(_observer);
  }
  return self;
}

- (void)broadcastScrollViewSize:(CGSize)scrollViewSize {
  self.observer->OnScrollViewSizeBroadcasted(scrollViewSize);
}

- (void)broadcastScrollViewContentSize:(CGSize)contentSize {
  self.observer->OnScrollViewContentSizeBroadcasted(contentSize);
}

- (void)broadcastScrollViewContentInset:(UIEdgeInsets)contentInset {
  self.observer->OnScrollViewContentInsetBroadcasted(contentInset);
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  self.observer->OnContentScrollOffsetBroadcasted(offset);
}

- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling {
  self.observer->OnScrollViewIsScrollingBroadcasted(scrolling);
}

- (void)broadcastScrollViewIsZooming:(BOOL)zooming {
  self.observer->OnScrollViewIsZoomingBroadcasted(zooming);
}

- (void)broadcastScrollViewIsDragging:(BOOL)dragging {
  self.observer->OnScrollViewIsDraggingBroadcasted(dragging);
}

- (void)broadcastCollapsedToolbarHeight:(CGFloat)height {
  self.observer->OnCollapsedToolbarHeightBroadcasted(height);
}

- (void)broadcastExpandedToolbarHeight:(CGFloat)height {
  self.observer->OnExpandedToolbarHeightBroadcasted(height);
}

- (void)broadcastBottomToolbarHeight:(CGFloat)height {
  self.observer->OnBottomToolbarHeightBroadcasted(height);
}

@end
