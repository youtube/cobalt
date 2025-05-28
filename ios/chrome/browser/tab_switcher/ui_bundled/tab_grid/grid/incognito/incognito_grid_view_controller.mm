// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/incognito/incognito_grid_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_commands.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation IncognitoGridViewController {
  // A view to obscure incognito content when the user isn't authorized to
  // see it.
  IncognitoReauthView* _blockingView;
}

#pragma mark - Parent's functions

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemsAtIndexPaths:
        (NSArray<NSIndexPath*>*)indexPaths
                                           point:(CGPoint)point {
  // Don't allow long-press previews when the incognito reauth view is blocking
  // the content.
  if (self.contentNeedsAuthentication) {
    return nil;
  }

  return [super collectionView:collectionView
      contextMenuConfigurationForItemsAtIndexPaths:indexPaths
                                             point:point];
}

- (NSArray<UIDragItem*>*)collectionView:(UICollectionView*)collectionView
           itemsForBeginningDragSession:(id<UIDragSession>)session
                            atIndexPath:(NSIndexPath*)indexPath {
  if (self.contentNeedsAuthentication) {
    // Don't support dragging items if the drag&drop handler is not set.
    return @[];
  }
  return [super collectionView:collectionView
      itemsForBeginningDragSession:session
                       atIndexPath:indexPath];
}

#pragma mark - IncognitoReauthConsumer

- (void)setItemsRequireAuthentication:(BOOL)require {
  self.contentNeedsAuthentication = require;

  if (require) {
    if (!_blockingView) {
      _blockingView = [[IncognitoReauthView alloc] init];
      _blockingView.translatesAutoresizingMaskIntoConstraints = NO;
      _blockingView.layer.zPosition = FLT_MAX;
      // No need to show tab switcher button when already in the tab switcher.
      _blockingView.tabSwitcherButton.hidden = YES;
      // Hide the logo.
      _blockingView.logoView.hidden = YES;

      [_blockingView.authenticateButton
                 addTarget:self.reauthHandler
                    action:@selector(authenticateIncognitoContent)
          forControlEvents:UIControlEventTouchUpInside];

      if (IsIOSSoftLockEnabled()) {
        id<GridCommands> gridHandler = self.gridHandler;
        id<IncognitoReauthCommands> reauthHandler = self.reauthHandler;
        [_blockingView.exitIncognitoButton
                   addAction:[UIAction actionWithHandler:^(UIAction* action) {
                     base::UmaHistogramEnumeration(
                         kIncognitoLockOverlayInteractionHistogram,
                         IncognitoLockOverlayInteraction::
                             kCloseIncognitoTabsButtonClicked);
                     base::RecordAction(base::UserMetricsAction(
                         "IOS.IncognitoLock.Overlay.CloseIncognitoTabs"));
                     [gridHandler closeAllItems];
                     [reauthHandler manualAuthenticationOverride];
                   }]
            forControlEvents:UIControlEventTouchUpInside];
      }
    }

    [self.view addSubview:_blockingView];
    _blockingView.alpha = 1;
    AddSameConstraints(self.collectionView.frameLayoutGuide, _blockingView);
  } else {
    __weak IncognitoGridViewController* weakSelf = self;
    [UIView animateWithDuration:kMaterialDuration1
        animations:^{
          [weakSelf hideBlockingView];
        }
        completion:^(BOOL finished) {
          [weakSelf blockingViewDidHide:finished];
        }];
  }
}

- (void)setItemsRequireAuthentication:(BOOL)require
                withPrimaryButtonText:(NSString*)text
                   accessibilityLabel:(NSString*)accessibilityLabel {
  [self setItemsRequireAuthentication:require];
  if (require) {
    [_blockingView setAuthenticateButtonText:text
                          accessibilityLabel:accessibilityLabel];
  } else {
    // No primary button text or accessibility label should be set when
    // authentication is not required.
    CHECK(!text);
    CHECK(!accessibilityLabel);
  }
}

#pragma mark - Private

// Sets properties that should be animated to remove the blocking view.
- (void)hideBlockingView {
  _blockingView.alpha = 0;
}

// Cleans up after blocking view animation completed.
- (void)blockingViewDidHide:(BOOL)finished {
  if (self.contentNeedsAuthentication) {
    _blockingView.alpha = 1;
  } else {
    [_blockingView removeFromSuperview];
  }
}

@end
