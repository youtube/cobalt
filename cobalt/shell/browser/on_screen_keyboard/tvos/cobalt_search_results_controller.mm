// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#import "cobalt/shell/browser/on_screen_keyboard/tvos/cobalt_search_results_controller.h"

@implementation CobaltSearchResultsController

#pragma mark - UIResponder

// These -pressesBegan and -pressesEnded implementations work together with
// CobaltSearchController's and ContentShellWindowDelegate's.
//
// CobaltSearchController's -presses{Began,Ended} handle Menu key presses
// specially by forwarding them to Kabuki via ContentShellWindowDelegate's
// implementation of CobaltSearchResultsControllerFocusDelegate.
//
// RenderWidgetUIView will eventually receive these events and pass them up, but
// since the web contents view will be part of CobaltSearchController's view
// hierarchy the event will reach its -pressesBegan and -pressesEnded and
// recurse forever. This special case is intercepted and disposed of here
// because this view controller sits between RenderWidgetUIView and
// CobaltSearchController.
- (void)pressesBegan:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  for (UIPress* press in presses) {
    if (press.type == UIPressTypeMenu) {
      return;
    }
  }
  [super pressesBegan:presses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  for (UIPress* press in presses) {
    if (press.type == UIPressTypeMenu) {
      return;
    }
  }
  [super pressesEnded:presses withEvent:event];
}

#pragma mark - UIFocusEnvironment

- (BOOL)shouldUpdateFocusInContext:(UIFocusUpdateContext*)context {
  if ([context.previouslyFocusedView isDescendantOfView:self.view] &&
      ![context.nextFocusedView isDescendantOfView:self.view]) {
    // This condition is entered when e.g. the web contents view is focused in
    // the Search page and the user presses Up: by default, the focus engine
    // will attempt to switch focus from the web contents view to the native
    // keyboard even if the expectation was for Kabuki to just scroll one shelf
    // up and not focus on the on-screen keyboard. Avoid this by always denying
    // the focus switch attempt.
    //
    // Note that this does not mean the native keyboard will never be focused:
    // when there is nothing for Kabuki to focus when the user presses Up, it
    // will eventually call OnScreenKeyboard.show() and
    // OnScreenKeyboard.focus(), the latter of which reaches
    // ContentShellWindowDelegate's -focusOnScreenKeyboard: which will
    // temporarily set userInteractionEnabled to NO in the web contents view,
    // trigger a ContentShellWindowDelegate focus update that will not invoke
    // this function and reach -didUpdateFocusInContext:withAnimationCoordinator
    // below.
    //
    // See b/546217920 and the commit message introducing this change for more
    // information.
    return NO;
  }
  return YES;
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext*)context
       withAnimationCoordinator:(UIFocusAnimationCoordinator*)coordinator {
  if ([context.nextFocusedView isDescendantOfView:self.view]) {
    if (![context.previouslyFocusedView isDescendantOfView:self.view]) {
      // Switching focus from the native keyboard and to the web contents view.
      [_focusDelegate resultsDidReceiveFocus];
    }
  } else if ([context.previouslyFocusedView isDescendantOfView:self.view]) {
    // Switching focus from the web contents view to the native keyboard.
    [_focusDelegate resultsDidLoseFocus];
  }
}

@end
