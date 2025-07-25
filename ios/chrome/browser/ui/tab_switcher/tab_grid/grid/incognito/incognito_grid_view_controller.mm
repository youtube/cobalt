// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_view_controller.h"

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_commands.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller+subclassing.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation IncognitoGridViewController {
  // A view to obscure incognito content when the user isn't authorized to
  // see it.
  IncognitoReauthView* _blockingView;
}

#pragma mark - Parent's functions

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  // Don't allow long-press previews when the incognito reauth view is blocking
  // the content.
  if (self.contentNeedsAuthentication) {
    return nil;
  }

  return [super collectionView:collectionView
      contextMenuConfigurationForItemAtIndexPath:indexPath
                                           point:point];
}

#pragma mark - IncognitoReauthConsumer

- (void)setItemsRequireAuthentication:(BOOL)require {
  self.contentNeedsAuthentication = require;
  [self.delegate gridViewController:self
      contentNeedsAuthenticationChanged:require];

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
