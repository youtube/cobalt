// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_coordinator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_positioner.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_view_controller.h"
#import "ios/web/public/web_state_id.h"

namespace {
constexpr CGFloat kTabGroupPresentationDuration = 0.3;
constexpr CGFloat kTabGroupDismissalDuration = 0.25;
constexpr CGFloat kTabGroupBackgroundElementDurationFactor = 0.75;
}  // namespace

@interface TabGroupCoordinator () <GridViewControllerDelegate>
@end

@implementation TabGroupCoordinator {
  // Mediator for tab groups.
  TabGroupMediator* _mediator;
  // View controller for tab groups.
  TabGroupViewController* _viewController;
  // Context Menu helper for the tabs.
  TabContextMenuHelper* _tabContextMenuHelper;
  // Tab group to display.
  raw_ptr<const TabGroup> _tabGroup;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group coordinator outside the "
         "Tab Groups experiment.";
  CHECK(tabGroup) << "You need to pass a tab group in order to display it.";
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _tabGroup = tabGroup;
    _animatedPresentation = YES;
  }
  return self;
}

- (UIViewController*)viewController {
  return _viewController;
}

- (void)stopChildCoordinators {
  [_viewController.gridViewController dismissModals];
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self setUpViewController];

  Browser* browser = self.browser;
  _mediator = [[TabGroupMediator alloc]
      initWithWebStateList:browser->GetWebStateList()
                  tabGroup:_tabGroup->GetWeakPtr()
                  consumer:_viewController
              gridConsumer:_viewController.gridViewController
                modeHolder:self.modeHolder];
  _mediator.browser = browser;
  _mediator.tabGroupsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  _mediator.tabGridIdleStatusHandler = self.tabGridIdleStatusHandler;

  _tabContextMenuHelper = [[TabContextMenuHelper alloc]
             initWithProfile:browser->GetProfile()
      tabContextMenuDelegate:self.tabContextMenuDelegate];

  _viewController.mutator = _mediator;
  _viewController.gridViewController.mutator = _mediator;
  _viewController.gridViewController.menuProvider = _tabContextMenuHelper;
  _viewController.gridViewController.dragDropHandler = _mediator;

  [self displayViewControllerAnimated:self.animatedPresentation];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _tabContextMenuHelper = nil;

  [self hideViewControllerAnimated:YES];

  _viewController = nil;
}

#pragma mark - Animation helpers

- (void)displayViewControllerAnimated:(BOOL)animated {
  CHECK(_viewController);

  [self.baseViewController addChildViewController:_viewController];

  UIView* viewAbove = [self.tabGroupPositioner viewAboveTabGroup];
  _viewController.view.frame = self.baseViewController.view.bounds;
  if (viewAbove && viewAbove.superview) {
    [self.baseViewController.view insertSubview:_viewController.view
                                   belowSubview:viewAbove];
  } else {
    [self.baseViewController.view addSubview:_viewController.view];
  }
  [_viewController fadeBlurIn];
  [_viewController didMoveToParentViewController:self.baseViewController];

  if (!animated) {
    [_viewController contentWillAppearAnimated:NO];
    return;
  }

  __weak TabGroupViewController* viewController = _viewController;

  [_viewController prepareForPresentation];

  CGFloat nonGridElementDuration =
      kTabGroupPresentationDuration * kTabGroupBackgroundElementDurationFactor;
  CGFloat topElementDelay =
      kTabGroupPresentationDuration - nonGridElementDuration;

  [UIView animateWithDuration:nonGridElementDuration
                        delay:topElementDelay
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     [viewController animateTopElementsPresentation];
                   }
                   completion:nil];

  [UIView animateWithDuration:nonGridElementDuration
                        delay:0
                      options:UIViewAnimationCurveEaseIn
                   animations:^{
                     [viewController fadeBlurIn];
                   }
                   completion:nil];

  [UIView animateWithDuration:kTabGroupPresentationDuration
                        delay:0
                      options:UIViewAnimationCurveEaseInOut
                   animations:^{
                     [viewController animateGridPresentation];
                   }
                   completion:nil];

  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  nil);
}

- (void)hideViewControllerAnimated:(BOOL)animated {
  __weak TabGroupViewController* viewController = _viewController;

  auto completion = ^void {
    [viewController willMoveToParentViewController:nil];
    [viewController.view removeFromSuperview];
    [viewController removeFromParentViewController];
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    nil);
  };

  if (!animated) {
    completion();
  }

  [UIView animateWithDuration:kTabGroupDismissalDuration
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        [viewController animateDismissal];
      }
      completion:^(BOOL finished) {
        completion();
      }];

  CGFloat backgroundDuration =
      kTabGroupDismissalDuration * kTabGroupBackgroundElementDurationFactor;
  [UIView animateWithDuration:backgroundDuration
                        delay:0
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     [viewController fadeBlurOut];
                   }
                   completion:nil];
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  BOOL incognito = self.browser->GetProfile()->IsOffTheRecord();
  if ([_mediator isItemWithIDSelected:itemID]) {
    if (incognito) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridIncognitoTabGroupOpenCurrentTab"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridRegularTabGroupOpenCurrentTab"));
    }
  } else {
    if (incognito) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabIncognitoGridTabGroupOpenTab"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("MobileTabRegularGridTabGroupOpenTab"));
    }
    [_mediator selectItemWithID:itemID
                         pinned:NO
         isFirstActionOnTabGrid:[self.tabGridIdleStatusHandler status]];
  }

  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);

  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  [handler showActiveTab];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
            didSelectGroup:(const TabGroup*)group {
  NOTREACHED();
}

// TODO(crbug.com/40273478): Remove once inactive tabs do not depends on it
// anymore.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
  NOTREACHED();
}

- (void)gridViewControllerDidMoveItem:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWithID:(web::WebStateID)itemID {
  // No-op.
}

- (void)gridViewControllerDragSessionWillBeginForTab:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDragSessionWillBeginForTabGroup:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController {
  [self.viewController gridViewControllerDidScroll];
}

- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED();
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED();
}

- (void)gridViewControllerDidRequestContextMenu:
    (BaseGridViewController*)gridViewController {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)gridViewControllerDropSessionDidEnter:
    (BaseGridViewController*)gridViewController {
  // No-op
}

- (void)gridViewControllerDropSessionDidExit:
    (BaseGridViewController*)gridViewController {
  // No-op
}

#pragma mark - Private

// Share the current group.
- (void)shareGroup {
  ShareKitService* shareKitService =
      ShareKitServiceFactory::GetForProfile(self.browser->GetProfile());
  if (!_tabGroup || !shareKitService) {
    return;
  }
  ShareKitShareGroupConfiguration* config =
      [[ShareKitShareGroupConfiguration alloc] init];
  config.tabGroup = _tabGroup;
  config.baseViewController = self.baseViewController;
  config.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  shareKitService->ShareGroup(config);
}

// Manage the group with `collabID`.
- (void)manageGroup:(NSString*)collabID {
  ShareKitService* shareKitService =
      ShareKitServiceFactory::GetForProfile(self.browser->GetProfile());
  if (!collabID || !shareKitService) {
    return;
  }
  ShareKitManageConfiguration* config =
      [[ShareKitManageConfiguration alloc] init];
  config.baseViewController = self.baseViewController;
  config.collabID = collabID;
  config.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  shareKitService->ManageGroup(config);
}

// Sets up the `_viewController`.
- (void)setUpViewController {
  ProfileIOS* profile = self.browser->GetProfile();

  // Get the command handler for TabGroupsCommands.
  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);

  // Determine if the tab group is shared and get the collaboration ID.
  tab_groups::TabGroupSyncService* syncService =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  NSString* savedCollabID =
      tab_groups::utils::GetTabGroupCollabID(_tabGroup, syncService);
  BOOL isShared = savedCollabID != nil;

  // Initialize the `_viewController`.
  _viewController = [[TabGroupViewController alloc]
      initWithHandler:handler
            incognito:self.browser->GetProfile()->IsOffTheRecord()
               shared:isShared
             tabGroup:_tabGroup];
  _viewController.gridViewController.delegate = self;

  // Prevent the face pile from being set up for tab groups that are not shared
  // and cannot be shared.
  if (!isShared && !IsSharedTabGroupsCreateEnabled(profile)) {
    return;
  }

  ShareKitService* shareKitService =
      ShareKitServiceFactory::GetForProfile(profile);
  if (!shareKitService) {
    return;
  }

  // Configure the face pile.
  ShareKitFacePileConfiguration* config =
      [[ShareKitFacePileConfiguration alloc] init];
  config.collabID = savedCollabID;
  __weak __typeof(self) weakSelf = self;
  config.completionBlock = ^(NSString* collabID, BOOL isSignedIn) {
    if (collabID) {
      [weakSelf manageGroup:collabID];
    } else {
      [weakSelf shareGroup];
    }
  };
  _viewController.facePile = shareKitService->FacePile(config);
}

@end
