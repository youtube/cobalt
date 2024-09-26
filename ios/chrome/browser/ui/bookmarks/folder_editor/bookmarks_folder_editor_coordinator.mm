// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksFolderEditorCoordinator () <
    BookmarksFolderEditorViewControllerDelegate,
    BookmarksFolderChooserCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate> {
  // The navigation controller is `nullptr` if the folder chooser view
  // controller is pushed into the base navigation controller.
  // Otherwise, the navigation controller is presented in the base view
  // controller.
  UINavigationController* _navigationController;
  BookmarksFolderEditorViewController* _viewController;
  // Coordinator to show the folder chooser UI.
  BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
  // Parent folder to `_folderNode`. Should never be `nullptr`.
  const bookmarks::BookmarkNode* _parentFolderNode;
  // If `_folderNode` is `nullptr`, the user is adding a new folder. Otherwise
  // the user is editing an existing folder.
  const bookmarks::BookmarkNode* _folderNode;
}

@end

@implementation BookmarksFolderEditorCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                parentFolderNode:
                                    (const bookmarks::BookmarkNode*)
                                        parentFolder {
  DCHECK(parentFolder);
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _parentFolderNode = parentFolder;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                folderNode:
                                    (const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(folder->parent());
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _folderNode = folder;
    _parentFolderNode = folder->parent();
  }
  return self;
}

- (void)start {
  [super start];
  // TODO(crbug.com/1402758): Create a mediator.
  ChromeBrowserState* browserState =
      self.browser->GetBrowserState()->GetOriginalChromeBrowserState();
  bookmarks::BookmarkModel* profileBookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          browserState);
  bookmarks::BookmarkModel* accountBookmarkModel =
      ios::AccountBookmarkModelFactory::GetForBrowserState(browserState);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  _viewController = [[BookmarksFolderEditorViewController alloc]
      initWithProfileBookmarkModel:profileBookmarkModel
              accountBookmarkModel:accountBookmarkModel
                        folderNode:_folderNode
                  parentFolderNode:_parentFolderNode
             authenticationService:authService
                  syncSetupService:syncSetupService
                       syncService:syncService
                           browser:self.browser];
  _viewController.delegate = self;
  _viewController.snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);

  if (_baseNavigationController) {
    [_baseNavigationController pushViewController:_viewController animated:YES];
  } else {
    DCHECK(!_navigationController);
    _navigationController = [[BookmarkNavigationController alloc]
        initWithRootViewController:_viewController];
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
    _navigationController.presentationController.delegate = self;

    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  // Stop child coordinator before stopping `self`.
  [self stopBookmarksFolderChooserCoordinator];

  DCHECK(_viewController);
  if (_navigationController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController = nil;
  } else if (_baseNavigationController &&
             _baseNavigationController.presentingViewController) {
    // If `_baseNavigationController.presentingViewController` is `nil` then
    // the parent coordinator (who owns the `_baseNavigationController`) has
    // already been dismissed. In this case `_baseNavigationController` itself
    // is no longer being presented and this coordinator was dismissed as well.
    DCHECK_EQ(_baseNavigationController.topViewController, _viewController);
    [_baseNavigationController popViewControllerAnimated:YES];
  } else if (!_baseNavigationController) {
    // If there is no `_baseNavigationController` and `_navigationController`,
    // the view controller has been already dismissed. See
    // `presentationControllerDidDismiss:` and
    // `bookmarksFolderEditorDidDismiss:`.
    // Therefore `self.baseViewController.presentedViewController` must be
    // `nil`.
    DCHECK(!self.baseViewController.presentedViewController);
  }
  [_viewController disconnect];
  _viewController = nil;
}

- (BOOL)canDismiss {
  DCHECK(_viewController);
  return [_viewController canDismiss];
}

#pragma mark - BookmarksFolderEditorViewControllerDelegate

- (void)showBookmarksFolderChooserWithParentFolder:
            (const bookmarks::BookmarkNode*)parent
                                       hiddenNodes:
                                           (const std::set<
                                               const bookmarks::BookmarkNode*>&)
                                               hiddenNodes {
  DCHECK(!_folderChooserCoordinator);
  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseNavigationController:(_baseNavigationController
                                            ? _baseNavigationController
                                            : _navigationController)
                               browser:self.browser
                           hiddenNodes:hiddenNodes];
  _folderChooserCoordinator.allowsNewFolders = NO;
  [_folderChooserCoordinator setSelectedFolder:parent];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

- (void)bookmarksFolderEditor:(BookmarksFolderEditorViewController*)folderEditor
       didFinishEditingFolder:(const bookmarks::BookmarkNode*)folder {
  [_delegate bookmarksFolderEditorCoordinator:self
                   didFinishEditingFolderNode:folder];
}

- (void)bookmarksFolderEditorDidDeleteEditedFolder:
    (BookmarksFolderEditorViewController*)folderEditor {
  // Deleting the folder is only allowed when the user is editing an existing
  // folder.
  DCHECK(_folderNode);
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (void)bookmarksFolderEditorDidCancel:
    (BookmarksFolderEditorViewController*)folderEditor {
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (void)bookmarksFolderEditorDidDismiss:
    (BookmarksFolderEditorViewController*)folderEditor {
  DCHECK(_baseNavigationController);
  _baseNavigationController = nil;
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorViewController*)controller {
  [_delegate bookmarksFolderEditorWillCommitTitleChange:self];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);
  [self stopBookmarksFolderChooserCoordinator];

  [_viewController updateParentFolder:folder];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopBookmarksFolderChooserCoordinator];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSBookmarksFolderEditorClosedWithSwipeDown"));
  [_viewController presentationControllerDidAttemptToDismiss];
}

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Resign first responder if trying to dismiss the VC so the keyboard doesn't
  // linger until the VC dismissal has completed.
  [_viewController.view endEditing:YES];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  DCHECK(_navigationController);
  _navigationController = nil;
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [self canDismiss];
}

#pragma mark - Private

- (void)stopBookmarksFolderChooserCoordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

@end
