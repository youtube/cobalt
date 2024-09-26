// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/infobars/sc_infobar_banner_no_modal_coordinator.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/showcase/infobars/sc_infobar_constants.h"
#import "ios/showcase/infobars/sc_infobar_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SCInfobarBannerNoModalCoordinator

@interface SCInfobarBannerNoModalCoordinator () <InfobarBannerDelegate>
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@property(nonatomic, strong) ContainerViewController* containerViewController;
@end

@implementation SCInfobarBannerNoModalCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.containerViewController = [[ContainerViewController alloc] init];
  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.containerViewController.title = @"Infobar Messages";

  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:self
         presentsModal:NO
                  type:InfobarType::kInfobarTypeConfirm];
  self.bannerViewController.titleText = kInfobarBannerTitleLabel;
  self.bannerViewController.subtitleText = kInfobarBannerSubtitleLabel;
  self.bannerViewController.buttonText = kInfobarBannerButtonLabel;
  self.containerViewController.bannerViewController = self.bannerViewController;

  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

- (void)dealloc {
  [self dismissInfobarBannerForUserInteraction:NO];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self dismissInfobarBannerForUserInteraction:NO];
}

- (void)presentInfobarModalFromBanner {
  // NO-OP.
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

- (void)infobarBannerWasDismissed {
  self.bannerViewController = nil;
}

@end
