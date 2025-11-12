// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "starboard/tvos/shared/search_results_view_controller.h"

@implementation SBDSearchResultsViewController

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    self.view.backgroundColor = [UIColor clearColor];
  }
  return self;
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext*)context
       withAnimationCoordinator:(UIFocusAnimationCoordinator*)coordinator {
  if ([context.nextFocusedView isDescendantOfView:self.view]) {
    if (![context.previouslyFocusedView isDescendantOfView:self.view]) {
      [_focusDelegate resultsDidReceiveFocus];
    }
  } else if ([context.previouslyFocusedView isDescendantOfView:self.view]) {
    [_focusDelegate resultsDidLoseFocus];
  }
}

@end
