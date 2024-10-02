// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_view_controller.h"

#import <memory>
#import <set>

#import "base/auto_reset.h"
#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/mac/foundation_util.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFolderTitle = kItemTypeEnumZero,
  ItemTypeParentFolder,
};

}  // namespace

@interface BookmarksFolderEditorViewController () <
    AuthenticationServiceObserving,
    BookmarkModelBridgeObserver,
    BookmarkTextFieldItemDelegate,
    SyncObserverModelBridge>
@end

@implementation BookmarksFolderEditorViewController {
  // Model object for profile bookmarks.
  base::WeakPtr<BookmarkModel> _profileBookmarkModel;
  // Observer for `_profileBookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _profileModelBridge;
  // Model object for account bookmarks.
  base::WeakPtr<BookmarkModel> _accountBookmarkModel;
  // Observer for `_accountBookmarkModel` changes.
  std::unique_ptr<BookmarkModelBridge> _accountModelBridge;
  // Authentication service to get signin status.
  base::WeakPtr<AuthenticationService> _authService;
  // Observer for signin status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge> _authServiceBridge;
  SyncSetupService* _syncSetupService;
  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  // The browser for this view controller.
  base::WeakPtr<Browser> _browser;
  ChromeBrowserState* _browserState;
  // Parent folder to `_folder`. Should never be `nullptr`.
  const BookmarkNode* _parentFolder;
  // If `_folderNode` is `nullptr`, the user is adding a new folder. Otherwise
  // the user is editing an existing folder.
  const BookmarkNode* _folder;

  BOOL _edited;
  BOOL _editingExistingFolder;
  // Flag to ignore bookmark model Move notifications when the move is performed
  // by this class.
  BOOL _ignoresOwnMove;
  __weak UIBarButtonItem* _doneItem;
  __strong BookmarkTextFieldItem* _titleItem;
  __strong BookmarkParentFolderItem* _parentFolderItem;
  // The action sheet coordinator, if one is currently being shown.
  __strong ActionSheetCoordinator* _actionSheetCoordinator;
}

#pragma mark - Initialization

- (instancetype)
    initWithProfileBookmarkModel:(BookmarkModel*)profileBookmarkModel
            accountBookmarkModel:(BookmarkModel*)accountBookmarkModel
                      folderNode:(const BookmarkNode*)folder
                parentFolderNode:(const BookmarkNode*)parentFolder
           authenticationService:(AuthenticationService*)authService
                syncSetupService:(SyncSetupService*)syncSetupService
                     syncService:(syncer::SyncService*)syncService
                         browser:(Browser*)browser {
  DCHECK(profileBookmarkModel);
  DCHECK(profileBookmarkModel->loaded());
  if (base::FeatureList::IsEnabled(bookmarks::kEnableBookmarksAccountStorage)) {
    DCHECK(accountBookmarkModel);
    DCHECK(accountBookmarkModel->loaded());
  } else {
    DCHECK(!accountBookmarkModel);
  }
  DCHECK(parentFolder);
  if (folder) {
    BookmarkModel* modelForFolder = bookmark_utils_ios::GetBookmarkModelForNode(
        folder, profileBookmarkModel, accountBookmarkModel);
    DCHECK(!modelForFolder->is_permanent_node(folder));
  }
  DCHECK(browser);

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _profileBookmarkModel = profileBookmarkModel->AsWeakPtr();
    _profileModelBridge = std::make_unique<BookmarkModelBridge>(
        self, _profileBookmarkModel.get());
    if (accountBookmarkModel) {
      _accountBookmarkModel = accountBookmarkModel->AsWeakPtr();
      _accountModelBridge = std::make_unique<BookmarkModelBridge>(
          self, _accountBookmarkModel.get());
    }
    _folder = folder;
    _parentFolder = parentFolder;
    _editingExistingFolder = _folder != nullptr;
    _browser = browser->AsWeakPtr();
    _browserState = browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _authService = authService->GetWeakPtr();
    _authServiceBridge = std::make_unique<AuthenticationServiceObserverBridge>(
        _authService.get(), self);
    // Set up the bookmark model oberver.
    _syncObserverModelBridge =
        std::make_unique<SyncObserverBridge>(self, syncService);
    _syncSetupService = syncSetupService;
  }
  return self;
}

- (void)disconnect {
  _browserState = nullptr;
  _profileBookmarkModel.reset();
  _profileModelBridge.reset();
  _accountBookmarkModel.reset();
  _accountModelBridge.reset();
  _folder = nullptr;
  _parentFolder = nullptr;
  _authService.reset();
  _authServiceBridge.reset();
  _syncObserverModelBridge.reset();
  _syncSetupService = nullptr;
  _titleItem.delegate = nil;
}

#pragma mark - Public

- (void)presentationControllerDidAttemptToDismiss {
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser.get()
                           title:nil
                         message:nil
                   barButtonItem:self.navigationItem.leftBarButtonItem];

  __weak __typeof(self) weakSelf = self;
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  [weakSelf saveFolder];
                }
                 style:UIAlertActionStyleDefault];
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  [weakSelf dismiss];
                }
                 style:UIAlertActionStyleDestructive];
  // IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  weakSelf.navigationItem.leftBarButtonItem.enabled = YES;
                  weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
                }
                 style:UIAlertActionStyleCancel];

  self.navigationItem.leftBarButtonItem.enabled = NO;
  self.navigationItem.rightBarButtonItem.enabled = NO;
  [_actionSheetCoordinator start];
}

// Whether the bookmarks folder editor can be dismissed.
- (BOOL)canDismiss {
  return !_edited;
}

- (void)updateParentFolder:(const BookmarkNode*)parent {
  DCHECK(parent);
  _parentFolder = parent;
  [self updateParentFolderState];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor = self.styler.tableViewBackgroundColor;
  self.tableView.estimatedRowHeight = 150.0;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kBookmarkCellHorizontalLeadingInset,
                                         0, 0)];

  // Add Done button.
  UIBarButtonItem* doneItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(saveFolder)];
  doneItem.accessibilityIdentifier =
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier;
  self.navigationItem.rightBarButtonItem = doneItem;
  _doneItem = doneItem;

  if (_editingExistingFolder) {
    // Add Cancel Button.
    UIBarButtonItem* cancelItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                             target:self
                             action:@selector(dismiss)];
    cancelItem.accessibilityIdentifier = @"Cancel";
    self.navigationItem.leftBarButtonItem = cancelItem;

    [self addToolbar];
  }
  [self updateEditingState];
  [self setupCollectionViewModel];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateSaveButtonState];
  if (_editingExistingFolder) {
    self.navigationController.toolbarHidden = NO;
  } else {
    self.navigationController.toolbarHidden = YES;
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate bookmarksFolderEditorDidDismiss:self];
  }
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self dismiss];
  return YES;
}

#pragma mark - Actions

- (void)dismiss {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderEditorCanceled"));
  [self.view endEditing:YES];
  [self.delegate bookmarksFolderEditorDidCancel:self];
}

- (void)deleteFolder {
  DCHECK(_editingExistingFolder);
  DCHECK(_folder);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderEditorDeletedFolder"));
  std::set<const BookmarkNode*> editedNodes;
  editedNodes.insert(_folder);
  BookmarkModel* modelForFolder = bookmark_utils_ios::GetBookmarkModelForNode(
      _folder, _profileBookmarkModel.get(), _accountBookmarkModel.get());
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                              editedNodes, {modelForFolder}, _browserState)];
  [self.delegate bookmarksFolderEditorDidDeleteEditedFolder:self];
}

- (void)saveFolder {
  DCHECK(_parentFolder);
  BookmarkModel* modelForParentFolder =
      bookmark_utils_ios::GetBookmarkModelForNode(_parentFolder,
                                                  _profileBookmarkModel.get(),
                                                  _accountBookmarkModel.get());
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarksFolderEditorSaved"));
  NSString* folderString = _titleItem.text;
  DCHECK(folderString.length > 0);
  std::u16string folderTitle = base::SysNSStringToUTF16(folderString);

  if (_editingExistingFolder) {
    DCHECK(_folder);
    // Tell delegate if folder title has been changed.
    if (_folder->GetTitle() != folderTitle) {
      [self.delegate bookmarksFolderEditorWillCommitTitleChange:self];
    }

    BookmarkModel* modelForFolder = bookmark_utils_ios::GetBookmarkModelForNode(
        _folder, _profileBookmarkModel.get(), _accountBookmarkModel.get());
    modelForFolder->SetTitle(_folder, folderTitle,
                             bookmarks::metrics::BookmarkEditSource::kUser);
    if (_folder->parent() != _parentFolder) {
      // Currently `MoveBookmarksWithUndoToast(...)` doesn't support moving node
      // between models.
      // TODO(crbug.com/1416567): Revise after nodes can move between models.
      DCHECK_EQ(modelForFolder, modelForParentFolder);
      base::AutoReset<BOOL> autoReset(&_ignoresOwnMove, YES);
      [self.snackbarCommandsHandler
          showSnackbarMessage:bookmark_utils_ios::MoveBookmarksWithUndoToast(
                                  std::set<const BookmarkNode*>{_folder},
                                  modelForParentFolder, _parentFolder,
                                  _browserState)];
    }
  } else {
    DCHECK(!_folder);
    _folder = modelForParentFolder->AddFolder(
        _parentFolder, _parentFolder->children().size(), folderTitle);
  }
  [self.view endEditing:YES];
  [self.delegate bookmarksFolderEditor:self didFinishEditingFolder:_folder];
}

- (void)changeParentFolder {
  base::RecordAction(base::UserMetricsAction(
      "MobileBookmarksFolderEditorOpenedFolderChooser"));
  std::set<const BookmarkNode*> hiddenNodes;
  if (_folder) {
    hiddenNodes.insert(_folder);
  }
  [self.delegate showBookmarksFolderChooserWithParentFolder:_parentFolder
                                                hiddenNodes:hiddenNodes];
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self cancelIfParentFolderIsUnavailable];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded:(BookmarkModel*)model {
  // The bookmark model is assumed to be loaded when this controller is created.
  NOTREACHED();
}

- (void)bookmarkModel:(BookmarkModel*)model
        didChangeNode:(const BookmarkNode*)bookmarkNode {
  if (bookmarkNode == _parentFolder) {
    [self updateParentFolderState];
  }
}

- (void)bookmarkModel:(BookmarkModel*)model
    didChangeChildrenForNode:(const BookmarkNode*)bookmarkNode {
  // No-op.
}

- (void)bookmarkModel:(BookmarkModel*)model
          didMoveNode:(const BookmarkNode*)bookmarkNode
           fromParent:(const BookmarkNode*)oldParent
             toParent:(const BookmarkNode*)newParent {
  if (_ignoresOwnMove) {
    return;
  }
  if (bookmarkNode == _folder) {
    DCHECK(oldParent == _parentFolder);
    _parentFolder = newParent;
    [self updateParentFolderState];
  }
}

- (void)bookmarkModel:(BookmarkModel*)model
        didDeleteNode:(const BookmarkNode*)node
           fromFolder:(const BookmarkNode*)folder {
  if (node == _parentFolder) {
    _parentFolder = NULL;
    [self updateParentFolderState];
    return;
  }
  if (node == _folder) {
    _folder = NULL;
    _editingExistingFolder = NO;
    [self updateEditingState];
  }
}

- (void)bookmarkModelRemovedAllNodes:(BookmarkModel*)model {
  if (model->is_permanent_node(_parentFolder)) {
    return;  // The current parent folder is still valid.
  }

  _parentFolder = NULL;
  [self updateParentFolderState];
}

#pragma mark - BookmarkTextFieldItemDelegate

- (void)textDidChangeForItem:(BookmarkTextFieldItem*)item {
  _edited = YES;
  [self updateSaveButtonState];
}

- (void)textFieldDidBeginEditing:(UITextField*)textField {
  textField.textColor = [BookmarkTextFieldCell textColorForEditing:YES];
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  textField.textColor = [BookmarkTextFieldCell textColorForEditing:NO];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeParentFolder) {
    [self changeParentFolder];
  }
}

#pragma mark - Private

// If `_parentFolder` belongs to `_accountBookmarkModel` and
// `accountBookmarkModel` becomes unavailable, this method will cancel folder
// editor. Returns whether folder editor was cancelled or not.
- (BOOL)cancelIfParentFolderIsUnavailable {
  BookmarkModel* parentFolderModel =
      bookmark_utils_ios::GetBookmarkModelForNode(_parentFolder,
                                                  _profileBookmarkModel.get(),
                                                  _accountBookmarkModel.get());
  if (!bookmark_utils_ios::IsAccountBookmarkModelAvailable(
          _authService.get()) &&
      parentFolderModel == _accountBookmarkModel.get()) {
    [self dismiss];
    return YES;
  }
  return NO;
}

- (void)updateEditingState {
  if (![self isViewLoaded]) {
    return;
  }

  self.view.accessibilityIdentifier =
      (_folder) ? kBookmarkFolderEditViewContainerIdentifier
                : kBookmarkFolderCreateViewContainerIdentifier;

  [self setTitle:(_folder)
                     ? l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_EDIT_TITLE)
                     : l10n_util::GetNSString(
                           IDS_IOS_BOOKMARK_NEW_GROUP_EDITOR_CREATE_TITLE)];
}

// Updates `_parentFolderItem` without updating the table view.
- (void)updateParentFolderItem {
  CHECK(_parentFolderItem);
  _parentFolderItem.title =
      bookmark_utils_ios::TitleForBookmarkNode(_parentFolder);
  BookmarkModelType type = bookmark_utils_ios::GetBookmarkModelType(
      _parentFolder, _profileBookmarkModel.get(), _accountBookmarkModel.get());
  switch (type) {
    case BookmarkModelType::kProfile:
      _parentFolderItem.shouldDisplayCloudSlashIcon =
          bookmark_utils_ios::ShouldDisplayCloudSlashIconForProfileModel(
              _syncSetupService);
      break;
    case BookmarkModelType::kAccount:
      _parentFolderItem.shouldDisplayCloudSlashIcon = NO;
      break;
  }
}

// Updates `_parentFolderItem` and reload the table view cell.
- (void)updateParentFolderState {
  if ([self cancelIfParentFolderIsUnavailable]) {
    return;
  }
  [self updateParentFolderItem];
  NSIndexPath* folderSelectionIndexPath =
      [self.tableViewModel indexPathForItemType:ItemTypeParentFolder
                              sectionIdentifier:SectionIdentifierInfo];
  [self.tableView reloadRowsAtIndexPaths:@[ folderSelectionIndexPath ]
                        withRowAnimation:UITableViewRowAnimationNone];
}

- (void)setupCollectionViewModel {
  [self loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierInfo];

  _titleItem = [[BookmarkTextFieldItem alloc] initWithType:ItemTypeFolderTitle];
  _titleItem.text =
      (_folder)
          ? bookmark_utils_ios::TitleForBookmarkNode(_folder)
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME);
  _titleItem.placeholder =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_EDITOR_NAME_LABEL);
  _titleItem.accessibilityIdentifier = @"Title";
  [self.tableViewModel addItem:_titleItem
       toSectionWithIdentifier:SectionIdentifierInfo];
  _titleItem.delegate = self;

  _parentFolderItem =
      [[BookmarkParentFolderItem alloc] initWithType:ItemTypeParentFolder];
  [self updateParentFolderItem];
  [self.tableViewModel addItem:_parentFolderItem
       toSectionWithIdentifier:SectionIdentifierInfo];
}

// Adds delete button at the bottom.
- (void)addToolbar {
  self.navigationController.toolbarHidden = NO;
  NSString* titleString = l10n_util::GetNSString(IDS_IOS_BOOKMARK_GROUP_DELETE);
  UIBarButtonItem* deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(deleteFolder)];
  deleteButton.accessibilityIdentifier =
      kBookmarkFolderEditorDeleteButtonIdentifier;
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  deleteButton.tintColor = [UIColor colorNamed:kRedColor];

  [self setToolbarItems:@[ spaceButton, deleteButton, spaceButton ]
               animated:NO];
}

- (void)updateSaveButtonState {
  _doneItem.enabled = (_titleItem.text.length > 0);
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateParentFolderState];
}

@end
