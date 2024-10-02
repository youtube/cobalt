// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_adaptor.h"

#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinatee.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinatorAdaptor ()<ToolbarCommands>
@property(nonatomic, strong)
    NSMutableArray<id<NewTabPageControllerDelegate, ToolbarCommands>>*
        coordinators;
@end

@implementation ToolbarCoordinatorAdaptor

@synthesize coordinators = _coordinators;

#pragma mark - Public

- (instancetype)initWithDispatcher:(CommandDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    [dispatcher startDispatchingToTarget:self
                             forProtocol:@protocol(ToolbarCommands)];
    _coordinators = [NSMutableArray array];
  }
  return self;
}

- (void)addToolbarCoordinator:
    (id<NewTabPageControllerDelegate, ToolbarCommands>)toolbarCoordinator {
  [self.coordinators addObject:toolbarCoordinator];
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setScrollProgressForTabletOmnibox:progress];
  }
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    if (coordinator.fakeboxScribbleForwardingTarget) {
      return coordinator.fakeboxScribbleForwardingTarget;
    }
  }
  return nil;
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator triggerToolbarSlideInAnimation];
  }
}

#pragma mark - SideSwipeToolbarInteracting

- (BOOL)isInsideToolbar:(CGPoint)point {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    // The toolbar frame is inset by -1 because CGRectContainsPoint does include
    // points on the max X and Y edges, which will happen frequently with edge
    // swipes from the right side.
    CGRect toolbarFrame =
        CGRectInset([coordinator viewController].view.bounds, -1, -1);
    CGPoint pointInToolbarCoordinates =
        [[coordinator viewController].view convertPoint:point fromView:nil];
    if (CGRectContainsPoint(toolbarFrame, pointInToolbarCoordinates))
      return YES;
  }
  return NO;
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForOverflowMenuIPHDisplayed];
  }
}

- (void)updateUIForIPHDismissed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForIPHDismissed];
  }
}

@end
