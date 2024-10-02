// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation InfobarModalOverlayMediator

#pragma mark - InfobarModalDelegate

- (void)dismissInfobarModal:(id)infobarModal {
  base::RecordAction(base::UserMetricsAction(kInfobarModalCancelButtonTapped));
  [self.delegate stopOverlayForMediator:self];
}

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarModalMainActionResponse>()];
  [self dismissOverlay];
}

- (void)modalInfobarWasDismissed:(id)infobarModal {
  // Only needed in legacy implementation.  Dismissal completion cleanup occurs
  // in InfobarModalOverlayCoordinator.
  // TODO(crbug.com/1041917): Remove once non-overlay implementation is deleted.
}

@end
