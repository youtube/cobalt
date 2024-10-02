// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/translate/translate_infobar_placeholder_overlay_coordinator.h"

#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TranslateInfobarPlaceholderOverlayCoordinator ()
// The list of supported mediator classes.
@property(class, nonatomic, readonly) NSArray<Class>* supportedMediatorClasses;
@end

@implementation TranslateInfobarPlaceholderOverlayCoordinator

#pragma mark - Accessors

+ (NSArray<Class>*)supportedMediatorClasses {
  return @[];
}

+ (const OverlayRequestSupport*)requestSupport {
  return InfobarBannerPlaceholderRequestConfig::RequestSupport();
}

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  self.started = YES;
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;

  // Mark started as NO before calling dismissal callback to prevent dup
  // stopAnimated: executions.
  self.started = NO;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  self.delegate->OverlayUIDidFinishDismissal(self.request);
}

@end
