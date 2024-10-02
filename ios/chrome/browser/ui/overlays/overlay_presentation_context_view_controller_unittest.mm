// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_view_controller.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/test_modality/test_presented_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/test_modality/test_resizing_presented_overlay_request_config.h"
#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"
#import "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/test_modality/test_presented_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/test_modality/test_resizing_presented_overlay_coordinator.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForUIElementTimeout;

namespace {
// The frame of resizable presened overlay UI.
const CGRect kWindowFrame = {{100.0, 100.0}, {100.0, 100.0}};
}  // namespace

// Test fixture for OverlayPresentationContextViewController.
class OverlayPresentationContextViewControllerTest : public PlatformTest {
 public:
  OverlayPresentationContextViewControllerTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    root_view_controller_ = [[UIViewController alloc] init];
    view_controller_ = [[OverlayPresentationContextViewController alloc] init];
    root_view_controller_.definesPresentationContext = YES;
    scoped_window_.Get().rootViewController = root_view_controller_;
    // Present `view_controller_` over `root_view_controller_` without animation
    // and wait for the presentation to finish.
    view_controller_.modalPresentationStyle =
        UIModalPresentationOverCurrentContext;
    __block bool presentation_finished = NO;
    [root_view_controller_ presentViewController:view_controller_
                                        animated:NO
                                      completion:^{
                                        presentation_finished = YES;
                                      }];
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
      return presentation_finished;
    }));
  }
  ~OverlayPresentationContextViewControllerTest() override {
    // Dismisses `view_controller_` and waits for the dismissal to finish.
    __block bool dismissal_finished = NO;
    [root_view_controller_ dismissViewControllerAnimated:NO
                                              completion:^{
                                                dismissal_finished = YES;
                                              }];
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
      return dismissal_finished;
    }));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  FakeOverlayRequestCoordinatorDelegate delegate_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  OverlayPresentationContextViewController* view_controller_ = nil;
};

// Tests that `view_controller_`'s frame is CGRectZero when there is no overlay
// UI presented upon it.
TEST_F(OverlayPresentationContextViewControllerTest, NoPresentedUI) {
  CGRect container_view_frame =
      view_controller_.presentationController.containerView.frame;
  EXPECT_TRUE(CGRectEqualToRect(container_view_frame, CGRectZero));
  CGRect frame = view_controller_.view.frame;
  EXPECT_TRUE(CGRectEqualToRect(frame, CGRectZero));
}

// Tests that `view_controller_`'s frame is the same as its presenter while
// showing overlay UI presented over its context.
TEST_F(OverlayPresentationContextViewControllerTest,
       PresentedOverCurrentContext) {
  if (@available(iOS 15.7.1, *)) {
    if (@available(iOS 15.7.2, *)) {
    } else {
      // TODO(crbug.com/1409884): Failing on a few 15.7.1 devices.
      return;
    }
  }
  // Create a fake overlay coordinator that presents its UI over
  // `view_controller_`.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TestPresentedOverlay>();
  TestPresentedOverlayCoordinator* coordinator =
      [[TestPresentedOverlayCoordinator alloc]
          initWithBaseViewController:view_controller_
                             browser:browser_.get()
                             request:request.get()
                            delegate:&delegate_];

  // Start the coordinator and wait for its presentation to finish.
  [coordinator startAnimated:NO];
  UIViewController* overlay_view_controller = coordinator.viewController;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return overlay_view_controller.presentingViewController &&
           !overlay_view_controller.beingPresented;
  }));

  // Verify that the presentation context is resized to
  // `root_view_controller_`'s view.
  UIView* root_view = root_view_controller_.view;
  CGRect root_window_frame = [root_view convertRect:root_view.bounds
                                             toView:nil];
  UIView* container_view =
      view_controller_.presentationController.containerView;
  EXPECT_TRUE(CGRectEqualToRect(
      [container_view convertRect:container_view.bounds toView:nil],
      root_window_frame));
  UIView* view = view_controller_.view;
  EXPECT_TRUE(CGRectEqualToRect([view convertRect:view.bounds toView:nil],
                                root_window_frame));

  // Stop the coordinator and wait for its dismissal to finish.
  [coordinator stopAnimated:NO];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !overlay_view_controller.presentingViewController;
  }));

  // Verify that the views are resized to CGRectZero when nothing is presented
  // upon it.
  EXPECT_TRUE(CGRectEqualToRect(container_view.frame, CGRectZero));
  EXPECT_TRUE(CGRectEqualToRect(view.frame, CGRectZero));
}

// Tests that `view_controller_`'s frame is the same as its presented view's
// container view if it is shown using custom UIViewController presentation that
// resizes the contianer view.
TEST_F(OverlayPresentationContextViewControllerTest, ResizingPresentedOverlay) {
  if (@available(iOS 15.7.1, *)) {
    if (@available(iOS 15.7.2, *)) {
    } else {
      // TODO(crbug.com/1409884): Failing on a few 15.7.1 devices.
      return;
    }
  }
  // Create a fake overlay coordinator that presents its UI over
  // `view_controller_` and resizes its presentation container view to
  // kWindowFrame.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TestResizingPresentedOverlay>(
          kWindowFrame);
  TestResizingPresentedOverlayCoordinator* coordinator =
      [[TestResizingPresentedOverlayCoordinator alloc]
          initWithBaseViewController:view_controller_
                             browser:browser_.get()
                             request:request.get()
                            delegate:&delegate_];

  // Start the coordinator and wait for its presentation to finish.
  [coordinator startAnimated:NO];
  UIViewController* overlay_view_controller = coordinator.viewController;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return overlay_view_controller.presentingViewController &&
           !overlay_view_controller.beingPresented;
  }));

  // Verify that the presentation context is resized kWindowFrame.
  UIView* container_view =
      view_controller_.presentationController.containerView;
  EXPECT_TRUE(CGRectEqualToRect(
      [container_view convertRect:container_view.bounds toView:nil],
      kWindowFrame));
  UIView* view = view_controller_.view;
  EXPECT_TRUE(CGRectEqualToRect([view convertRect:view.bounds toView:nil],
                                kWindowFrame));

  // Verify that resizing the presentation context container doesn't affect the
  // layout of the overlay container.
  UIView* overlay_container_view =
      overlay_view_controller.presentationController.containerView;
  EXPECT_TRUE(CGRectEqualToRect([overlay_container_view convertRect:view.bounds
                                                             toView:nil],
                                kWindowFrame));

  // Stop the coordinator and wait for its dismissal to finish.
  [coordinator stopAnimated:NO];
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !overlay_view_controller.presentingViewController;
  }));

  // Verify that the views are resized to CGRectZero when nothing is presented
  // upon it.
  EXPECT_TRUE(CGRectEqualToRect(container_view.frame, CGRectZero));
  EXPECT_TRUE(CGRectEqualToRect(view.frame, CGRectZero));
}
