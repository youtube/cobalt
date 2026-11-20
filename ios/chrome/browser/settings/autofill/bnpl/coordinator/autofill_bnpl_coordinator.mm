// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/bnpl/coordinator/autofill_bnpl_coordinator.h"

#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/settings/autofill/bnpl/coordinator/autofill_bnpl_mediator.h"
#import "ios/chrome/browser/settings/autofill/bnpl/ui/autofill_bnpl_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation AutofillBnplCoordinator {
  AutofillBnplTableViewController* _viewController;
  AutofillBnplMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:
                    (UINavigationController*)navigationController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[AutofillBnplTableViewController alloc] init];
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.browser->GetProfile());
  _mediator = [[AutofillBnplMediator alloc]
      initWithPersonalDataManager:personalDataManager
                      prefService:self.browser->GetProfile()->GetPrefs()];
  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

@end
